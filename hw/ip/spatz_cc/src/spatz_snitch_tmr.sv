// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// TMR (triple modular redundancy) wrapper around the Snitch integer core.
// Instantiates three independent copies of `snitch`, all driven by
// identical inputs, and majority-votes every replicated `snitch` output
// individually via `bitwise_TMR_voter_fail`. `core_tmr_fault_o` is
// asserted whenever any voted output disagrees across the three replicas
// -- self-corrected by the vote the same cycle, so this is
// diagnostic/counter-only (matches this design's convention: TMR-voter
// mismatches never trigger the uncorrectable-fault recovery interrupt,
// they're expected to be silently corrected).
//
// Drop-in replacement for a plain `snitch` instance: same parameter and
// port list, plus the one new core_tmr_fault_o output.

module spatz_snitch_tmr
  import snitch_pkg::*;
#(
  parameter logic [31:0]                 BootAddr               = 32'h0000_1000,
  parameter int unsigned                 AddrWidth              = 48,
  parameter int unsigned                 DataWidth              = 64,
  parameter bit                          RVE                    = 0,
  // Plain `snitch` defaults this to 1 (VM support enabled, incl.
  // ITLB/DTLB generation). Forwarded here so callers overriding it (e.g.
  // spatz_cc.sv passing 1'b0) actually reach the replicas instead of
  // silently getting VM support they didn't ask for -- previously this
  // wasn't declared at all, so spatz_cc.sv's .VMSupport(1'b0) override
  // was an undefined-parameter no-op (VCS Warning-[AOUP]) and every
  // replica ran with VM support on regardless.
  parameter bit                          VMSupport              = 1,
  parameter bit                          Xdma                   = 0,
  parameter bit                          FP_EN                  = 1,
  parameter bit                          RVF                    = 0,
  parameter bit                          RVD                    = 0,
  parameter bit                          XF16                   = 0,
  parameter bit                          XF16ALT                = 0,
  parameter bit                          XF8                    = 0,
  parameter bit                          XF8ALT                 = 0,
  parameter bit                          XDivSqrt               = 0,
  parameter bit                          RVV                    = 0,
  parameter int unsigned                 FLEN                   = DataWidth,
  parameter type                         dreq_t                 = logic,
  parameter type                         drsp_t                 = logic,
  parameter type                         acc_issue_req_t        = logic,
  parameter type                         acc_issue_rsp_t        = logic,
  parameter type                         acc_rsp_t              = logic,
  parameter type                         pa_t                   = logic,
  parameter type                         l0_pte_t               = logic,
  parameter int unsigned                 NumIntOutstandingLoads = 0,
  parameter int unsigned                 NumIntOutstandingMem   = 0,
  parameter snitch_pma_pkg::snitch_pma_t SnitchPMACfg           = '{default: 0},
  /// Derived parameter *Do not override*
  parameter type                         addr_t                 = logic [AddrWidth-1:0],
  parameter type                         data_t                 = logic [DataWidth-1:0]
) (
  input  logic                     clk_i,
  input  logic                     rst_i,
  input  logic [31:0]              hart_id_i,
  input  interrupts_t              irq_i,
  output logic                     flush_i_valid_o,
  input  logic                     flush_i_ready_i,
  output addr_t                    inst_addr_o,
  output logic                     inst_cacheable_o,
  input  logic [31:0]              inst_data_i,
  output logic                     inst_valid_o,
  input  logic                     inst_ready_i,
  output acc_issue_req_t           acc_qreq_o,
  input  acc_issue_rsp_t           acc_qrsp_i,
  output logic                     acc_qvalid_o,
  input  logic                     acc_qready_i,
  input  acc_rsp_t                 acc_prsp_i,
  input  logic                     acc_pvalid_i,
  output logic                     acc_pready_o,
  input  logic [1:0]               acc_mem_finished_i,
  input  logic [1:0]               acc_mem_str_finished_i,
  output dreq_t                    data_req_o,
  input  drsp_t                    data_rsp_i,
  output logic [1:0]               ptw_valid_o,
  input  logic [1:0]               ptw_ready_i,
  output va_t [1:0]                ptw_va_o,
  output pa_t [1:0]                ptw_ppn_o,
  input  l0_pte_t [1:0]            ptw_pte_i,
  input  logic [1:0]               ptw_is_4mega_i,
  output fpnew_pkg::roundmode_e    fpu_rnd_mode_o,
  output fpnew_pkg::fmt_mode_t     fpu_fmt_mode_o,
  input  fpnew_pkg::status_t       fpu_status_i,
  output snitch_pkg::core_events_t core_events_o,
  // Asserted whenever any voted output disagrees across the three
  // triplicated snitch replicas.
  output logic                     core_tmr_fault_o
);

  localparam int unsigned NumReplicas = 3;

  logic                     flush_i_valid_rep  [NumReplicas];
  addr_t                    inst_addr_rep      [NumReplicas];
  logic                     inst_cacheable_rep [NumReplicas];
  logic                     inst_valid_rep     [NumReplicas];
  acc_issue_req_t           acc_qreq_rep       [NumReplicas];
  logic                     acc_qvalid_rep     [NumReplicas];
  logic                     acc_pready_rep     [NumReplicas];
  dreq_t                    data_req_rep       [NumReplicas];
  logic [1:0]               ptw_valid_rep      [NumReplicas];
  va_t  [1:0]               ptw_va_rep         [NumReplicas];
  pa_t  [1:0]               ptw_ppn_rep        [NumReplicas];
  fpnew_pkg::roundmode_e    fpu_rnd_mode_rep   [NumReplicas];
  fpnew_pkg::fmt_mode_t     fpu_fmt_mode_rep   [NumReplicas];
  snitch_pkg::core_events_t core_events_rep    [NumReplicas];

  for (genvar t = 0; t < NumReplicas; t++) begin : gen_replica
    snitch #(
      .BootAddr              (BootAddr              ),
      .AddrWidth             (AddrWidth             ),
      .DataWidth             (DataWidth             ),
      .acc_issue_req_t       (acc_issue_req_t       ),
      .acc_issue_rsp_t       (acc_issue_rsp_t       ),
      .acc_rsp_t             (acc_rsp_t             ),
      .dreq_t                (dreq_t                ),
      .drsp_t                (drsp_t                ),
      .pa_t                  (pa_t                  ),
      .l0_pte_t              (l0_pte_t              ),
      .SnitchPMACfg          (SnitchPMACfg          ),
      .NumIntOutstandingLoads(NumIntOutstandingLoads),
      .NumIntOutstandingMem  (NumIntOutstandingMem  ),
      .VMSupport             (1'b0                  ),
      .RVE                   (RVE                   ),
      .VMSupport             (VMSupport             ),
      .FP_EN                 (FP_EN                 ),
      .Xdma                  (Xdma                  ),
      .RVF                   (RVF                   ),
      .RVD                   (RVD                   ),
      .RVV                   (RVV                   ),
      .XDivSqrt              (XDivSqrt              ),
      .XF16                  (XF16                  ),
      .XF16ALT               (XF16ALT               ),
      .XF8                   (XF8                   ),
      .XF8ALT                (XF8ALT                ),
      .FLEN                  (FLEN                  )
    ) i_snitch (
      .clk_i                 (clk_i                 ),
      .rst_i                 (rst_i                 ),
      .hart_id_i             (hart_id_i             ),
      .irq_i                 (irq_i                 ),
      .flush_i_valid_o       (flush_i_valid_rep[t]  ),
      .flush_i_ready_i       (flush_i_ready_i       ),
      .inst_addr_o           (inst_addr_rep[t]      ),
      .inst_cacheable_o      (inst_cacheable_rep[t] ),
      .inst_data_i           (inst_data_i           ),
      .inst_valid_o          (inst_valid_rep[t]     ),
      .inst_ready_i          (inst_ready_i          ),
      .acc_qreq_o            (acc_qreq_rep[t]       ),
      .acc_qrsp_i            (acc_qrsp_i            ),
      .acc_qvalid_o          (acc_qvalid_rep[t]     ),
      .acc_qready_i          (acc_qready_i          ),
      .acc_prsp_i            (acc_prsp_i            ),
      .acc_pvalid_i          (acc_pvalid_i          ),
      .acc_pready_o          (acc_pready_rep[t]     ),
      .acc_mem_finished_i    (acc_mem_finished_i    ),
      .acc_mem_str_finished_i(acc_mem_str_finished_i),
      .data_req_o            (data_req_rep[t]       ),
      .data_rsp_i            (data_rsp_i            ),
      .ptw_valid_o           (ptw_valid_rep[t]      ),
      .ptw_ready_i           (ptw_ready_i           ),
      .ptw_va_o              (ptw_va_rep[t]         ),
      .ptw_ppn_o             (ptw_ppn_rep[t]        ),
      .ptw_pte_i             (ptw_pte_i             ),
      .ptw_is_4mega_i        (ptw_is_4mega_i        ),
      .fpu_rnd_mode_o        (fpu_rnd_mode_rep[t]   ),
      .fpu_fmt_mode_o        (fpu_fmt_mode_rep[t]   ),
      .fpu_status_i          (fpu_status_i          ),
      .core_events_o         (core_events_rep[t]    )
    );
  end : gen_replica

  // Per-signal triplicate + majority vote on every replicated output.
  logic fault_flush, fault_inst_addr, fault_inst_cacheable, fault_inst_valid;
  logic fault_acc_qreq, fault_acc_qvalid, fault_acc_pready, fault_data_req;
  logic fault_ptw_valid, fault_ptw_va, fault_ptw_ppn;
  logic fault_fpu_rnd_mode, fault_fpu_fmt_mode, fault_core_events;

  bitwise_TMR_voter_fail #(.DataWidth(1), .VoterType(1)) i_flush_voter (
    .a_i(flush_i_valid_rep[0]), .b_i(flush_i_valid_rep[1]), .c_i(flush_i_valid_rep[2]),
    .majority_o(flush_i_valid_o), .fault_detected_o(fault_flush));

  bitwise_TMR_voter_fail #(.DataWidth($bits(addr_t)), .VoterType(1)) i_inst_addr_voter (
    .a_i(inst_addr_rep[0]), .b_i(inst_addr_rep[1]), .c_i(inst_addr_rep[2]),
    .majority_o(inst_addr_o), .fault_detected_o(fault_inst_addr));

  bitwise_TMR_voter_fail #(.DataWidth(1), .VoterType(1)) i_inst_cacheable_voter (
    .a_i(inst_cacheable_rep[0]), .b_i(inst_cacheable_rep[1]), .c_i(inst_cacheable_rep[2]),
    .majority_o(inst_cacheable_o), .fault_detected_o(fault_inst_cacheable));

  bitwise_TMR_voter_fail #(.DataWidth(1), .VoterType(1)) i_inst_valid_voter (
    .a_i(inst_valid_rep[0]), .b_i(inst_valid_rep[1]), .c_i(inst_valid_rep[2]),
    .majority_o(inst_valid_o), .fault_detected_o(fault_inst_valid));

  bitwise_TMR_voter_fail #(.DataWidth($bits(acc_issue_req_t)), .VoterType(1)) i_acc_qreq_voter (
    .a_i(acc_qreq_rep[0]), .b_i(acc_qreq_rep[1]), .c_i(acc_qreq_rep[2]),
    .majority_o(acc_qreq_o), .fault_detected_o(fault_acc_qreq));

  bitwise_TMR_voter_fail #(.DataWidth(1), .VoterType(1)) i_acc_qvalid_voter (
    .a_i(acc_qvalid_rep[0]), .b_i(acc_qvalid_rep[1]), .c_i(acc_qvalid_rep[2]),
    .majority_o(acc_qvalid_o), .fault_detected_o(fault_acc_qvalid));

  bitwise_TMR_voter_fail #(.DataWidth(1), .VoterType(1)) i_acc_pready_voter (
    .a_i(acc_pready_rep[0]), .b_i(acc_pready_rep[1]), .c_i(acc_pready_rep[2]),
    .majority_o(acc_pready_o), .fault_detected_o(fault_acc_pready));

  bitwise_TMR_voter_fail #(.DataWidth($bits(dreq_t)), .VoterType(1)) i_data_req_voter (
    .a_i(data_req_rep[0]), .b_i(data_req_rep[1]), .c_i(data_req_rep[2]),
    .majority_o(data_req_o), .fault_detected_o(fault_data_req));

  bitwise_TMR_voter_fail #(.DataWidth(2), .VoterType(1)) i_ptw_valid_voter (
    .a_i(ptw_valid_rep[0]), .b_i(ptw_valid_rep[1]), .c_i(ptw_valid_rep[2]),
    .majority_o(ptw_valid_o), .fault_detected_o(fault_ptw_valid));

  bitwise_TMR_voter_fail #(.DataWidth(2*$bits(va_t)), .VoterType(1)) i_ptw_va_voter (
    .a_i(ptw_va_rep[0]), .b_i(ptw_va_rep[1]), .c_i(ptw_va_rep[2]),
    .majority_o(ptw_va_o), .fault_detected_o(fault_ptw_va));

  bitwise_TMR_voter_fail #(.DataWidth(2*$bits(pa_t)), .VoterType(1)) i_ptw_ppn_voter (
    .a_i(ptw_ppn_rep[0]), .b_i(ptw_ppn_rep[1]), .c_i(ptw_ppn_rep[2]),
    .majority_o(ptw_ppn_o), .fault_detected_o(fault_ptw_ppn));

  bitwise_TMR_voter_fail #(.DataWidth($bits(fpnew_pkg::roundmode_e)), .VoterType(1)) i_fpu_rnd_mode_voter (
    .a_i(fpu_rnd_mode_rep[0]), .b_i(fpu_rnd_mode_rep[1]), .c_i(fpu_rnd_mode_rep[2]),
    .majority_o(fpu_rnd_mode_o), .fault_detected_o(fault_fpu_rnd_mode));

  bitwise_TMR_voter_fail #(.DataWidth($bits(fpnew_pkg::fmt_mode_t)), .VoterType(1)) i_fpu_fmt_mode_voter (
    .a_i(fpu_fmt_mode_rep[0]), .b_i(fpu_fmt_mode_rep[1]), .c_i(fpu_fmt_mode_rep[2]),
    .majority_o(fpu_fmt_mode_o), .fault_detected_o(fault_fpu_fmt_mode));

  bitwise_TMR_voter_fail #(.DataWidth($bits(snitch_pkg::core_events_t)), .VoterType(1)) i_core_events_voter (
    .a_i(core_events_rep[0]), .b_i(core_events_rep[1]), .c_i(core_events_rep[2]),
    .majority_o(core_events_o), .fault_detected_o(fault_core_events));

  assign core_tmr_fault_o = fault_flush | fault_inst_addr | fault_inst_cacheable | fault_inst_valid
                          | fault_acc_qreq | fault_acc_qvalid | fault_acc_pready | fault_data_req
                          | fault_ptw_valid | fault_ptw_va | fault_ptw_ppn
                          | fault_fpu_rnd_mode | fault_fpu_fmt_mode | fault_core_events;

endmodule
