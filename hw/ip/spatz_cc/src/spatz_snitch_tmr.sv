// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

/// TMR wrapper around the Snitch integer core (build-time `RELCORE_TMR` core).
///
/// Instantiates three independent copies of `snitch`, all driven by identical
/// inputs, and bitwise majority-votes every replicated `snitch` output (the
/// hive_req_o handshake/payload signals, the accelerator offload request, the
/// TCDM data request, the PTW request and the FPU side-channel) in a single
/// wide vote. `core_tmr_fault_o` is asserted whenever any bit of any voted
/// output disagrees across the three replicas.
module spatz_snitch_tmr
  import snitch_pkg::interrupts_t;
  import snitch_pkg::va_t;
#(
  /// Boot address of core.
  parameter logic [31:0] BootAddr  = 32'h0000_1000,
  /// Physical Address width of the core.
  parameter int unsigned AddrWidth = 48,
  /// Data width of memory interface.
  parameter int unsigned DataWidth = 64,
  /// Reduced-register extension.
  parameter bit          RVE       = 0,
  /// Enable Snitch DMA as accelerator.
  parameter bit          Xdma      = 0,
  parameter bit          Xssr      = 0,
  /// Enable FP in general
  parameter bit          FP_EN     = 1,
  /// Enable F Extension.
  parameter bit          RVF       = 0,
  /// Enable D Extension.
  parameter bit          RVD       = 0,
  parameter bit          XF16      = 0,
  parameter bit          XF16ALT   = 0,
  parameter bit          XF8       = 0,
  parameter bit          XF8ALT    = 0,
  /// Enable div/sqrt unit (buggy - use with caution)
  parameter bit          XDivSqrt  = 0,
  /// Enable V Extension
  parameter bit          RVV       = 0,
  parameter bit          XFVEC     = 0,
  parameter bit          XFDOTP    = 0,
  parameter bit          XFAUX     = 0,
  int unsigned           FLEN      = DataWidth,
  /// Enable virtual memory support.
  parameter bit          VMSupport = 1,
  /// Enable experimental IPU extension.
  parameter bit          Xipu      = 1,
  /// Data port request type.
  parameter type         dreq_t    = logic,
  /// Data port response type.
  parameter type         drsp_t          = logic,
  parameter type         acc_issue_req_t = logic,
  parameter type         acc_issue_rsp_t = logic,
  parameter type         acc_rsp_t       = logic,
  parameter type         pa_t            = logic,
  parameter type         l0_pte_t        = logic,
  parameter int unsigned NumIntOutstandingLoads = 0,
  parameter int unsigned NumIntOutstandingMem = 0,
  parameter int unsigned NumDTLBEntries = 0,
  parameter int unsigned NumITLBEntries = 0,
  parameter snitch_pma_pkg::snitch_pma_t SnitchPMACfg = '{default: 0},
  /// Derived parameter *Do not override*
  parameter type addr_t = logic [AddrWidth-1:0],
  parameter type data_t = logic [DataWidth-1:0]
) (
  input  logic          clk_i,
  input  logic          rst_i,
  input  logic [31:0]   hart_id_i,
  /// Interrupts
  input  interrupts_t   irq_i,
  /// Instruction cache flush request (TMR-voted)
  output logic          flush_i_valid_o,
  /// Flush has completed when the signal goes to `1`.
  /// Tie to `1` if unused
  input  logic          flush_i_ready_i,
  // Instruction Refill Port
  output addr_t         inst_addr_o,
  output logic          inst_cacheable_o,
  input  logic [31:0]   inst_data_i,
  output logic          inst_valid_o, // TMR-voted
  input  logic          inst_ready_i,
  /// Accelerator Interface - Master Port
  output acc_issue_req_t acc_qreq_o,
  input  acc_issue_rsp_t acc_qrsp_i,
  output logic           acc_qvalid_o,
  input  logic           acc_qready_i,
  input  acc_rsp_t       acc_prsp_i,
  input  logic           acc_pvalid_i,
  output logic           acc_pready_o,
  // Accelerator finished a memory operation
  input  logic [1:0]    acc_mem_finished_i,
  // Accelerator finished a memory store operation
  input  logic [1:0]    acc_mem_str_finished_i,
  /// TCDM Data Interface
  output dreq_t         data_req_o,
  input  drsp_t         data_rsp_i,
  // Address Translation interface.
  output logic    [1:0] ptw_valid_o, // TMR-voted
  input  logic    [1:0] ptw_ready_i,
  output va_t     [1:0] ptw_va_o,
  output pa_t     [1:0] ptw_ppn_o,
  input  l0_pte_t [1:0] ptw_pte_i,
  input  logic    [1:0] ptw_is_4mega_i,
  // FPU **un-timed** Side-channel
  output fpnew_pkg::roundmode_e     fpu_rnd_mode_o,
  output fpnew_pkg::fmt_mode_t      fpu_fmt_mode_o,
  input  fpnew_pkg::status_t        fpu_status_i,
  // Core events for performance counters
  output snitch_pkg::core_events_t  core_events_o,
  // Set when any bit of any voted output disagrees across the three replicas.
  output logic                      core_tmr_fault_o
);

  localparam int unsigned NumReplicas = 3;

  logic           flush_i_valid_rep [NumReplicas];
  logic           inst_valid_rep    [NumReplicas];
  logic [1:0]     ptw_valid_rep     [NumReplicas];

  addr_t          inst_addr_rep      [NumReplicas];
  logic           inst_cacheable_rep [NumReplicas];
  acc_issue_req_t acc_qreq_rep       [NumReplicas];
  logic           acc_qvalid_rep     [NumReplicas];
  logic           acc_pready_rep     [NumReplicas];
  dreq_t          data_req_rep       [NumReplicas];
  va_t  [1:0]     ptw_va_rep         [NumReplicas];
  pa_t  [1:0]     ptw_ppn_rep        [NumReplicas];
  fpnew_pkg::roundmode_e    fpu_rnd_mode_rep [NumReplicas];
  fpnew_pkg::fmt_mode_t     fpu_fmt_mode_rep [NumReplicas];
  snitch_pkg::core_events_t core_events_rep  [NumReplicas];

  for (genvar t = 0; t < NumReplicas; t++) begin : gen_replica
    snitch #(
      .BootAddr               (BootAddr              ),
      .AddrWidth              (AddrWidth             ),
      .DataWidth              (DataWidth             ),
      .RVE                    (RVE                   ),
      .Xdma                   (Xdma                  ),
      .Xssr                   (Xssr                  ),
      .FP_EN                  (FP_EN                 ),
      .RVF                    (RVF                   ),
      .RVD                    (RVD                   ),
      .XF16                   (XF16                  ),
      .XF16ALT                (XF16ALT               ),
      .XF8                    (XF8                   ),
      .XF8ALT                 (XF8ALT                ),
      .XDivSqrt               (XDivSqrt              ),
      .RVV                    (RVV                   ),
      .XFVEC                  (XFVEC                 ),
      .XFDOTP                 (XFDOTP                ),
      .XFAUX                  (XFAUX                 ),
      .FLEN                   (FLEN                  ),
      .VMSupport              (VMSupport             ),
      .Xipu                   (Xipu                  ),
      .dreq_t                 (dreq_t                ),
      .drsp_t                 (drsp_t                ),
      .acc_issue_req_t        (acc_issue_req_t       ),
      .acc_issue_rsp_t        (acc_issue_rsp_t       ),
      .acc_rsp_t              (acc_rsp_t             ),
      .pa_t                   (pa_t                  ),
      .l0_pte_t               (l0_pte_t              ),
      .NumIntOutstandingLoads (NumIntOutstandingLoads),
      .NumIntOutstandingMem   (NumIntOutstandingMem  ),
      .NumDTLBEntries         (NumDTLBEntries        ),
      .NumITLBEntries         (NumITLBEntries        ),
      .SnitchPMACfg           (SnitchPMACfg          ),
      .addr_t                 (addr_t                ),
      .data_t                 (data_t                )
    ) i_snitch (
      .clk_i                  (clk_i                 ),
      .rst_i                  (rst_i                 ),
      .hart_id_i              (hart_id_i             ),
      .irq_i                  (irq_i                 ),
      .flush_i_valid_o        (flush_i_valid_rep[t]  ),
      .flush_i_ready_i        (flush_i_ready_i       ),
      .inst_addr_o            (inst_addr_rep[t]      ),
      .inst_cacheable_o       (inst_cacheable_rep[t] ),
      .inst_data_i            (inst_data_i           ),
      .inst_valid_o           (inst_valid_rep[t]     ),
      .inst_ready_i           (inst_ready_i          ),
      .acc_qreq_o             (acc_qreq_rep[t]       ),
      .acc_qrsp_i             (acc_qrsp_i            ),
      .acc_qvalid_o           (acc_qvalid_rep[t]     ),
      .acc_qready_i           (acc_qready_i          ),
      .acc_prsp_i             (acc_prsp_i            ),
      .acc_pvalid_i           (acc_pvalid_i          ),
      .acc_pready_o           (acc_pready_rep[t]     ),
      .acc_mem_finished_i     (acc_mem_finished_i    ),
      .acc_mem_str_finished_i (acc_mem_str_finished_i),
      .data_req_o             (data_req_rep[t]       ),
      .data_rsp_i             (data_rsp_i            ),
      .ptw_valid_o            (ptw_valid_rep[t]      ),
      .ptw_ready_i            (ptw_ready_i           ),
      .ptw_va_o               (ptw_va_rep[t]         ),
      .ptw_ppn_o              (ptw_ppn_rep[t]        ),
      .ptw_pte_i              (ptw_pte_i             ),
      .ptw_is_4mega_i         (ptw_is_4mega_i        ),
      .fpu_rnd_mode_o         (fpu_rnd_mode_rep[t]   ),
      .fpu_fmt_mode_o         (fpu_fmt_mode_rep[t]   ),
      .fpu_status_i           (fpu_status_i          ),
      .core_events_o          (core_events_rep[t]    )
    );
  end

  // Bundle every replicated snitch output into one flat vector (plain
  // concatenation, no typedef struct) and majority-vote it in a single
  // bitwise_TMR_voter_fail instance.
  localparam int unsigned OutWidth =
      1 +                                  // flush_i_valid
      1 +                                  // inst_valid
      $bits(addr_t) +                      // inst_addr
      1 +                                  // inst_cacheable
      $bits(acc_issue_req_t) +             // acc_qreq
      1 +                                  // acc_qvalid
      1 +                                  // acc_pready
      $bits(dreq_t) +                      // data_req
      2 +                                  // ptw_valid
      2*$bits(va_t) +                      // ptw_va
      2*$bits(pa_t) +                      // ptw_ppn
      $bits(fpnew_pkg::roundmode_e) +      // fpu_rnd_mode
      $bits(fpnew_pkg::fmt_mode_t) +       // fpu_fmt_mode
      $bits(snitch_pkg::core_events_t);    // core_events

  logic [OutWidth-1:0] out_rep [NumReplicas];
  logic [OutWidth-1:0] out_voted;

  for (genvar t = 0; t < NumReplicas; t++) begin : gen_out_bundle
    assign out_rep[t] = {
      flush_i_valid_rep[t], inst_valid_rep[t], inst_addr_rep[t], inst_cacheable_rep[t],
      acc_qreq_rep[t], acc_qvalid_rep[t], acc_pready_rep[t], data_req_rep[t],
      ptw_valid_rep[t], ptw_va_rep[t], ptw_ppn_rep[t],
      fpu_rnd_mode_rep[t], fpu_fmt_mode_rep[t], core_events_rep[t]
    };
  end

  bitwise_TMR_voter_fail #(
    .DataWidth (OutWidth),
    .VoterType (1)
  ) i_snitch_out_voter (
    .a_i              (out_rep[0]      ),
    .b_i              (out_rep[1]      ),
    .c_i              (out_rep[2]      ),
    .majority_o       (out_voted       ),
    .fault_detected_o (core_tmr_fault_o)
  );

  assign {
    flush_i_valid_o, inst_valid_o, inst_addr_o, inst_cacheable_o,
    acc_qreq_o, acc_qvalid_o, acc_pready_o, data_req_o,
    ptw_valid_o, ptw_va_o, ptw_ppn_o,
    fpu_rnd_mode_o, fpu_fmt_mode_o, core_events_o
  } = out_voted;

endmodule
