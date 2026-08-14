// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Dual Modular Redundancy (DMR) wrapper around two independent fpnew_top
// instances. This is the "component-wise" (CW) protection variant's
// replacement for a single, unprotected fpnew_top instance in
// spatz_vfu.sv's gen_fpnew block -- a drop-in alternative to the
// end-to-end (e2e) variant's spatz_fpu_shadow_checker.sv.
//
// Unlike the shadow checker (a cheap approximate/exact-recompute *checker*
// that snoops on one real FPU), this module fully duplicates the FPU
// compute datapath: two complete fpnew_top instances of identical size,
// each fed from its own independently-latched copy of the instruction
// content (operands, opcode, formats, rounding mode, tag), so a bit-flip
// in one copy's staging registers or internal pipeline cannot also appear
// in the other.
//
// Recovery model: with only two copies, a mismatch cannot be resolved in
// hardware -- there is no way to tell which copy is right. Rather than
// build a microarchitectural retry/replay path (which the e2e variant's
// FPU protection does not have either, making that an unfair point of
// comparison), this module always forwards Primary's (i_fpu_a's) result
// best-effort on a mismatch and pulses dup_fault_o, which -- like every
// other uncorrectable-class fault in this design (ECC double-bit errors,
// TMR voter mismatches, the lockstep core mismatch) -- is escalated
// through the cluster's sticky per-hart uncorrectable-fault interrupt
// (see spatz_cluster_peripheral's UNCORRECTABLE_IRQ_STATUS) so that
// recovery happens the same way for both variants: by re-executing the
// kernel.
//
// Area cost: this module contains two full fpnew_top instances (roughly
// 2x the area of a single FPU lane) but, since there is no retry, adds no
// timing side effects of its own -- latency and throughput are identical
// to a single fpnew_top in both the fault-free and faulted case.

module spatz_fpu_dmr
  import fpnew_pkg::*;
#(
  parameter fpnew_pkg::fpu_features_t       Features                    = fpnew_pkg::RV64D_Xsflt,
  parameter fpnew_pkg::fpu_implementation_t Implementation              = fpnew_pkg::DEFAULT_NOREGS,
  parameter type                            TagType                     = logic,
  parameter fpnew_pkg::rsr_impl_t           StochasticRndImplementation = fpnew_pkg::DEFAULT_NO_RSR,
  localparam int unsigned                   WIDTH                       = Features.Width
) (
  input  logic                   clk_i,
  input  logic                   rst_ni,
  input  logic [31:0]            hart_id_i,

  // Instruction content (combinational, i.e. NOT pre-staged by the
  // caller -- this module owns both copies' input staging registers).
  input  logic [WIDTH-1:0]       operand1_i,
  input  logic [WIDTH-1:0]       operand2_i,
  input  logic [WIDTH-1:0]       operand3_i,
  input  fpnew_pkg::roundmode_e  rnd_mode_i,
  input  fpnew_pkg::operation_e  op_i,
  input  logic                   op_mod_i,
  input  fpnew_pkg::fp_format_e  src_fmt_i,
  input  fpnew_pkg::fp_format_e  dst_fmt_i,
  input  fpnew_pkg::int_format_e int_fmt_i,
  input  logic                   vectorial_op_i,
  input  TagType                 tag_i,

  input  logic                   in_valid_i,
  output logic                   in_ready_o,
  input  logic                   flush_i,

  output logic [WIDTH-1:0]       result_o,
  output fpnew_pkg::status_t     status_o,
  output TagType                 tag_o,

  output logic                   out_valid_o,
  input  logic                   out_ready_i,

  output logic                   busy_o,

  // Pulses one cycle when the two copies disagree. Always uncorrectable
  // (DMR alone cannot tell which copy is right) -- feeds the cluster's
  // uncorrectable-fault recovery interrupt, same as every other
  // uncorrectable-class fault in this design.
  output logic                   dup_fault_o
);

// Include FF
`include "common_cells/registers.svh"

  // ------------------------------------------------------------------
  // Input side: fan the incoming instruction content out to two
  // *independent* register banks, one per FPU copy. A bit-flip in one
  // bank cannot corrupt the other, since each is a separate flip-flop
  // loaded from the same combinational source on the same cycle.
  // ------------------------------------------------------------------
  typedef struct packed {
    logic [WIDTH-1:0]       operand1;
    logic [WIDTH-1:0]       operand2;
    logic [WIDTH-1:0]       operand3;
    fpnew_pkg::roundmode_e  rnd_mode;
    fpnew_pkg::operation_e  op;
    logic                   op_mod;
    fpnew_pkg::fp_format_e  src_fmt;
    fpnew_pkg::fp_format_e  dst_fmt;
    fpnew_pkg::int_format_e int_fmt;
    logic                   vectorial_op;
    TagType                 tag;
  } fpu_op_t;

  fpu_op_t issue_data;
  assign issue_data = fpu_op_t'{
    operand1:     operand1_i,
    operand2:     operand2_i,
    operand3:     operand3_i,
    rnd_mode:     rnd_mode_i,
    op:           op_i,
    op_mod:       op_mod_i,
    src_fmt:      src_fmt_i,
    dst_fmt:      dst_fmt_i,
    int_fmt:      int_fmt_i,
    vectorial_op: vectorial_op_i,
    tag:          tag_i
  };

  fpu_op_t a_op_q, b_op_q;
  logic    a_in_valid_q, b_in_valid_q;
  logic    a_pipe_in_ready, b_pipe_in_ready;
  logic    a_stage_ready, b_stage_ready;
  logic    issue_ready;

  assign a_stage_ready = !a_in_valid_q || a_pipe_in_ready;
  assign b_stage_ready = !b_in_valid_q || b_pipe_in_ready;
  assign issue_ready    = a_stage_ready && b_stage_ready;
  assign in_ready_o      = issue_ready;

  `FFL(a_op_q,       issue_data, in_valid_i && issue_ready, '0)
  `FFL(a_in_valid_q, in_valid_i, issue_ready,                1'b0)
  `FFL(b_op_q,       issue_data, in_valid_i && issue_ready, '0)
  `FFL(b_in_valid_q, in_valid_i, issue_ready,                1'b0)

  // ------------------------------------------------------------------
  // The two FPU copies -- lockstep by construction (identical config,
  // identical inputs, identical accept timing), so any difference in
  // their outputs can only be explained by a fault.
  // ------------------------------------------------------------------
  logic [WIDTH-1:0]   a_result,  b_result;
  fpnew_pkg::status_t a_status,  b_status;
  TagType             a_tag,     b_tag;
  logic               a_out_valid, b_out_valid;
  logic               a_out_ready, b_out_ready;
  logic               a_busy,      b_busy;

  fpnew_top #(
    .Features                   (Features                   ),
    .Implementation             (Implementation              ),
    .TagType                    (TagType                     ),
    .StochasticRndImplementation(StochasticRndImplementation )
  ) i_fpu_a (
    .clk_i         (clk_i             ),
    .rst_ni        (rst_ni            ),
    .hart_id_i     (hart_id_i         ),
    .flush_i       (flush_i           ),
    .busy_o        (a_busy            ),
    .operands_i    ({a_op_q.operand3, a_op_q.operand2, a_op_q.operand1}),
    .in_valid_i    (a_in_valid_q      ),
    .in_ready_o    (a_pipe_in_ready   ),
    .op_i          (a_op_q.op         ),
    .src_fmt_i     (a_op_q.src_fmt    ),
    .dst_fmt_i     (a_op_q.dst_fmt    ),
    .int_fmt_i     (a_op_q.int_fmt    ),
    .vectorial_op_i(a_op_q.vectorial_op),
    .op_mod_i      (a_op_q.op_mod     ),
    .tag_i         (a_op_q.tag        ),
    .simd_mask_i   ('1                ),
    .rnd_mode_i    (a_op_q.rnd_mode   ),
    .result_o      (a_result          ),
    .out_valid_o   (a_out_valid       ),
    .out_ready_i   (a_out_ready       ),
    .status_o      (a_status          ),
    .tag_o         (a_tag             )
  );

  fpnew_top #(
    .Features                   (Features                   ),
    .Implementation             (Implementation              ),
    .TagType                    (TagType                     ),
    .StochasticRndImplementation(StochasticRndImplementation )
  ) i_fpu_b (
    .clk_i         (clk_i             ),
    .rst_ni        (rst_ni            ),
    .hart_id_i     (hart_id_i         ),
    .flush_i       (flush_i           ),
    .busy_o        (b_busy            ),
    .operands_i    ({b_op_q.operand3, b_op_q.operand2, b_op_q.operand1}),
    .in_valid_i    (b_in_valid_q      ),
    .in_ready_o    (b_pipe_in_ready   ),
    .op_i          (b_op_q.op         ),
    .src_fmt_i     (b_op_q.src_fmt    ),
    .dst_fmt_i     (b_op_q.dst_fmt    ),
    .int_fmt_i     (b_op_q.int_fmt    ),
    .vectorial_op_i(b_op_q.vectorial_op),
    .op_mod_i      (b_op_q.op_mod     ),
    .tag_i         (b_op_q.tag        ),
    .simd_mask_i   ('1                ),
    .rnd_mode_i    (b_op_q.rnd_mode   ),
    .result_o      (b_result          ),
    .out_valid_o   (b_out_valid       ),
    .out_ready_i   (b_out_ready       ),
    .status_o      (b_status          ),
    .tag_o         (b_tag             )
  );

  // ------------------------------------------------------------------
  // Compare and forward. No retry: report-only, exactly like the e2e
  // shadow checker's role, just backed by a full duplicate instead of an
  // approximate/exact-recompute check.
  // ------------------------------------------------------------------
  logic candidate_valid, match;

  assign candidate_valid = a_out_valid || b_out_valid;
  assign match            = a_out_valid && b_out_valid
                           && (a_result == b_result)
                           && (a_status == b_status)
                           && (a_tag    == b_tag);

  assign out_valid_o = candidate_valid;
  assign result_o     = a_out_valid ? a_result : b_result;
  assign status_o     = a_out_valid ? a_status : b_status;
  assign tag_o        = a_out_valid ? a_tag    : b_tag;
  assign dup_fault_o  = candidate_valid && !match;

  assign a_out_ready = a_out_valid && out_ready_i;
  assign b_out_ready = b_out_valid && out_ready_i;

  assign busy_o = a_busy | b_busy;

endmodule
