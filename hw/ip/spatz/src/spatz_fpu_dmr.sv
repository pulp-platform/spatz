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
// to a single fpnew_top in both the fault-free and faulted case. That "no
// timing side effects" promise applies specifically to result_o/status_o/
// tag_o/out_valid_o (the FUNCTIONAL forwarding path) -- see the comment
// above dup_fault_comb below for why the (diagnostic-only) fault path
// deliberately does NOT keep that promise, and is pipelined instead.
//
// Timing note (pre-empted here rather than discovered after a backend
// run): a_result/b_result are each the direct output of a full fpnew_top
// -- i.e. whatever combinational rounding/exception-flag tail logic runs
// after that instance's own last internal pipeline register. The e2e
// variant's spatz_fpu_shadow_checker.sv hit a severe backend timing
// violation from exactly this class of signal (fpnew's live, undeferred
// tail) reaching a comparator and then its module output with no register
// in between, on a path that ultimately runs all the way up to the
// cluster-shared uncorrectable_irq flop in spatz_cluster_peripheral
// several hierarchy levels above this module (spatz_vfu -> spatz ->
// spatz_cc -> spatz_cluster -> spatz_cluster_peripheral). This module's
// own comparator (a full WIDTH-bit equality on TWO live results, plus
// status_t and tag compares, all stacked in one cycle) is at least as
// heavy as what tripped that violation, and dup_fault_o has no output
// register at all by default -- so the same class of violation is
// pre-emptively fixed here (see dup_fault_comb below) rather than waiting
// to hit it on this design's own backend run.

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

  // Pulses when the two copies disagree. Always uncorrectable (DMR alone
  // cannot tell which copy is right) -- feeds the cluster's
  // uncorrectable-fault recovery interrupt, same as every other
  // uncorrectable-class fault in this design. This is a REGISTERED output
  // (see cmp_q/match_q/dup_fault_q below), pulsing THREE cycles after
  // out_valid_o/candidate_valid rather than same-cycle -- deliberately, to
  // break up the comparator + long route to the cluster peripheral into
  // three clean cycles instead of one combinational one. No consumer
  // relies on any fixed latency here: it only feeds a diagnostic counter
  // and a sticky, software-masked interrupt bit (same reasoning as the
  // e2e variant's fault_o).
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
  // Forward (functional path). No retry: best-effort forward of
  // Primary's (i_fpu_a's) result on a mismatch, same-cycle, straight off
  // the live a_result/b_result -- this MUST stay same-cycle (see the
  // "no timing side effects" note in the header comment: retiming this
  // would add a cycle of FP latency to every op, fault or not).
  // ------------------------------------------------------------------
  logic candidate_valid;

  assign candidate_valid = a_out_valid || b_out_valid;
  assign out_valid_o      = candidate_valid;
  assign result_o          = a_out_valid ? a_result : b_result;
  assign status_o          = a_out_valid ? a_status : b_status;
  assign tag_o             = a_out_valid ? a_tag    : b_tag;

  assign a_out_ready = a_out_valid && out_ready_i;
  assign b_out_ready = b_out_valid && out_ready_i;

  assign busy_o = a_busy | b_busy;

  // ------------------------------------------------------------------
  // Compare (diagnostic path, deliberately pipelined -- see the header
  // comment's "Timing note"). THREE deferred stages -- not two -- so that
  // no single cycle ever combines more than one "heavy" operation. This is
  // a stricter split than what closed the equivalent violation in the e2e
  // variant's spatz_fpu_shadow_checker.sv (which needed two): the FPU here
  // is the design's own critical path with essentially no slack to begin
  // with, so this errs toward more/smaller stages rather than relying on
  // "should just fit":
  //
  //   1. cmp_q: pure register copy of {a,b}_{out_valid,result,status,tag}
  //      -- no logic at all, so this stage's own timing is trivial no
  //      matter how heavy fpnew_top's own rounding/exception tail is.
  //      This is what actually breaks the critical path: a_result/
  //      b_result now reach a register directly, instead of feeding any
  //      comparator in the same cycle as fpnew's own tail logic.
  //   2. match_q: three INDEPENDENT sub-compares computed off cmp_q (a
  //      clean register) and registered in parallel -- result_match_q
  //      (the one genuinely wide, WIDTH-bit equality, which is why it
  //      gets a cycle to itself instead of being summed with anything
  //      else that same cycle), status_match_q and tag_match_q (each much
  //      narrower), and the valid flags carried forward. Parallel,
  //      independent registers in the same stage don't stack in depth the
  //      way series logic would -- each one is only as deep as its own
  //      (narrow) compare.
  //   3. dup_fault_q: the final reduction is now just a handful of ANDs
  //      over already-registered 1-bit flags -- about as cheap as
  //      combinational logic gets -- registered before dup_fault_o leaves
  //      this module, so the long route up through spatz_vfu/spatz/
  //      spatz_cc/spatz_cluster to spatz_cluster_peripheral's
  //      uncorrectable_irq flop also gets its own clean cycle.
  //
  // Net latency: dup_fault_o pulses THREE cycles after candidate_valid/
  // out_valid_o instead of same-cycle. Safe by the same reasoning as the
  // e2e variant's fault_o: no consumer of an uncorrectable-fault pulse in
  // this design relies on any fixed latency relative to the op it
  // reports on -- it only feeds a diagnostic saturating counter and a
  // sticky, software-masked interrupt-status bit (see
  // spatz_fault_monitor.sv and spatz_cluster_peripheral.sv).
  // ------------------------------------------------------------------
  typedef struct packed {
    logic               a_valid;
    logic               b_valid;
    logic [WIDTH-1:0]   a_result;
    logic [WIDTH-1:0]   b_result;
    fpnew_pkg::status_t a_status;
    fpnew_pkg::status_t b_status;
    TagType             a_tag;
    TagType             b_tag;
  } dmr_cmp_entry_t;

  dmr_cmp_entry_t cmp_d, cmp_q;

  assign cmp_d = dmr_cmp_entry_t'{
    a_valid:  a_out_valid,
    b_valid:  b_out_valid,
    a_result: a_result,
    b_result: b_result,
    a_status: a_status,
    b_status: b_status,
    a_tag:    a_tag,
    b_tag:    b_tag
  };

  `FF(cmp_q, cmp_d, '0)

  // Stage 2: independent sub-compares, registered in parallel -- see the
  // comment above for why result_match gets a cycle to itself.
  logic any_valid_d,    any_valid_q;
  logic both_valid_d,   both_valid_q;
  logic result_match_d, result_match_q;
  logic status_match_d, status_match_q;
  logic tag_match_d,    tag_match_q;

  assign any_valid_d    = cmp_q.a_valid || cmp_q.b_valid;
  assign both_valid_d   = cmp_q.a_valid && cmp_q.b_valid;
  assign result_match_d = (cmp_q.a_result == cmp_q.b_result);
  assign status_match_d = (cmp_q.a_status == cmp_q.b_status);
  assign tag_match_d    = (cmp_q.a_tag    == cmp_q.b_tag);

  `FF(any_valid_q,    any_valid_d,    1'b0)
  `FF(both_valid_q,   both_valid_d,   1'b0)
  `FF(result_match_q, result_match_d, 1'b0)
  `FF(status_match_q, status_match_d, 1'b0)
  `FF(tag_match_q,    tag_match_d,    1'b0)

  // Stage 3: trivial reduction of already-registered 1-bit flags.
  logic dup_fault_comb, dup_fault_q;

  assign dup_fault_comb = any_valid_q
                         && !(both_valid_q && result_match_q && status_match_q && tag_match_q);

  `FF(dup_fault_q, dup_fault_comb, 1'b0)

  assign dup_fault_o = dup_fault_q;

endmodule
