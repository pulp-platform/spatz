// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// This is the toplevel module of Spatz. It contains all other Spatz modules.
// This includes the Controller, which interfaces with the main core and handles
// instruction decoding, operation issuing to the other units, and result write
// back to the core. The Vector Functional Unit (VFU) is the high throughput
// unit that executes all arithmetic and logical operations. The Load/Store Unit
// (LSU) is used to load vectors from memory to the register file and store them
// back again. Finally, the Vector Register File (VRF) is the main register file
// that stores all of the currently used vectors close to the execution units.

module spatz import spatz_pkg::*; import rvv_pkg::*; import fpnew_pkg::*; #(
    parameter int                  unsigned NrMemPorts          = 1,
    parameter bit                           RegisterRsp         = 0,
    // Memory request (VLSU)
    parameter type                          spatz_mem_req_t     = logic,
    parameter type                          spatz_mem_rsp_t     = logic,
    // Memory request (FP Sequencer)
    parameter type                          dreq_t              = logic,
    parameter type                          drsp_t              = logic,
    // Snitch interface
    parameter type                          spatz_issue_req_t   = logic,
    parameter type                          spatz_issue_rsp_t   = logic,
    parameter type                          spatz_rsp_t         = logic,
    /// FPU configuration.
    parameter fpu_implementation_t          FPUImplementation   = fpu_implementation_t'(0),
    // Derived parameters. DO NOT CHANGE!
    parameter int                  unsigned NumOutstandingLoads = 8
  ) (
    input  logic                              clk_i,
    input  logic                              rst_ni,
    input  logic                              testmode_i,
    input  logic             [31:0]           hart_id_i,
    // Snitch Interface
    input  logic                              issue_valid_i,
    output logic                              issue_ready_o,
    input  spatz_issue_req_t                  issue_req_i,
    output spatz_issue_rsp_t                  issue_rsp_o,
    output logic                              rsp_valid_o,
    input  logic                              rsp_ready_i,
    output spatz_rsp_t                        rsp_o,
    // Memory Request
    output spatz_mem_req_t   [NrMemPorts-1:0] spatz_mem_req_o,
    output logic             [NrMemPorts-1:0] spatz_mem_req_valid_o,
    input  logic             [NrMemPorts-1:0] spatz_mem_req_ready_i,
    input  spatz_mem_rsp_t   [NrMemPorts-1:0] spatz_mem_rsp_i,
    input  logic             [NrMemPorts-1:0] spatz_mem_rsp_valid_i,
    output logic             [NrMemPorts-1:0] spatz_mem_rsp_ready_o,
    // Memory Finished
    output logic             [1:0]            spatz_mem_finished_o,
    output logic             [1:0]            spatz_mem_str_finished_o,
    // FPU memory interface interface
`ifdef MEMPOOL_SPATZ
    output logic                              fp_lsu_mem_req_valid_o,
    input  logic                              fp_lsu_mem_req_ready_i,
    input  logic                              fp_lsu_mem_rsp_valid_i,
    output logic                              fp_lsu_mem_rsp_ready_o,
`endif
    output dreq_t                             fp_lsu_mem_req_o,
    input  drsp_t                             fp_lsu_mem_rsp_i,
    // FPU side channel
    input  roundmode_e                        fpu_rnd_mode_i,
    input  fmt_mode_t                         fpu_fmt_mode_i,
    output status_t                           fpu_status_o
  );

  ////////////////
  // Parameters //
  ////////////////

  // Number of ports of the vector register file
  localparam int unsigned NrWritePorts = 3;
  localparam int unsigned NrReadPorts  = 6;

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

  logic     vfu_req_ready;
  logic     vfu_rsp_ready;
  logic     vfu_rsp_valid;
  vfu_rsp_t vfu_rsp;

  logic      vlsu_req_ready;
  logic      vlsu_rsp_valid;
  vlsu_rsp_t vlsu_rsp;

  logic       vsldu_req_ready;
  logic       vsldu_rsp_valid;
  vsldu_rsp_t vsldu_rsp;

  /////////////////////
  //  FPU sequencer  //
  /////////////////////

  // X Interface
  spatz_issue_req_t issue_req;
  logic             issue_valid;
  logic             issue_ready;
  spatz_issue_rsp_t issue_rsp;
  spatz_rsp_t       resp;
  logic             resp_valid;
  logic             resp_ready;

  // Did we finish a memory request?
  logic fp_lsu_mem_finished;
  logic fp_lsu_mem_str_finished;
  logic spatz_mem_finished;
  logic spatz_mem_str_finished;
  assign spatz_mem_finished_o     = {spatz_mem_finished, fp_lsu_mem_finished};
  assign spatz_mem_str_finished_o = {spatz_mem_str_finished, fp_lsu_mem_str_finished};

  if (!FPU) begin: gen_no_fpu_sequencer
    // Spatz configured without an FPU. Just forward the requests to Spatz.
    assign issue_req     = issue_req_i;
    assign issue_valid   = issue_valid_i;
    assign issue_ready_o = issue_ready;
    assign issue_rsp_o   = issue_rsp;

    assign rsp_o       = resp;
    assign rsp_valid_o = resp_valid;
    assign resp_ready  = rsp_ready_i;

    // Tie the memory interface to zero
    assign fp_lsu_mem_req_o        = '0;
`ifdef MEMPOOL_SPATZ
    assign fp_lsu_mem_req_valid_o  = 1'b0;
    assign fp_lsu_mem_rsp_ready_o  = 1'b0;
`endif
    assign fp_lsu_mem_finished     = 1'b0;
    assign fp_lsu_mem_str_finished = 1'b0;
  end: gen_no_fpu_sequencer else begin: gen_fpu_sequencer
    spatz_fpu_sequencer #(
      .dreq_t             (dreq_t              ),
      .drsp_t             (drsp_t              ),
      .spatz_issue_req_t  (spatz_issue_req_t   ),
      .spatz_issue_rsp_t  (spatz_issue_rsp_t   ),
      .spatz_rsp_t        (spatz_rsp_t         ),
      .NumOutstandingLoads(NumOutstandingLoads )
    ) i_fpu_sequencer (
      .clk_i                    ( clk_i                  ),
      .rst_ni                   ( rst_ni                 ),
      // Snitch interface
      .issue_req_i              ( issue_req_i            ),
      .issue_valid_i            ( issue_valid_i          ),
      .issue_ready_o            ( issue_ready_o          ),
      .issue_rsp_o              ( issue_rsp_o            ),
      .resp_o                   ( rsp_o                  ),
      .resp_valid_o             ( rsp_valid_o            ),
      .resp_ready_i             ( rsp_ready_i            ),
      // Spatz interface
      .issue_req_o              ( issue_req              ),
      .issue_valid_o            ( issue_valid            ),
      .issue_ready_i            ( issue_ready            ),
      .issue_rsp_i              ( issue_rsp              ),
      .resp_i                   ( resp                   ),
      .resp_valid_i             ( resp_valid             ),
      .resp_ready_o             ( resp_ready             ),
      // Memory interface
`ifdef MEMPOOL_SPATZ
      .fp_lsu_mem_req_valid_o   ( fp_lsu_mem_req_valid_o ),
      .fp_lsu_mem_req_ready_i   ( fp_lsu_mem_req_ready_i ),
      .fp_lsu_mem_rsp_valid_i   ( fp_lsu_mem_rsp_valid_i ),
      .fp_lsu_mem_rsp_ready_o   ( fp_lsu_mem_rsp_ready_o ),
`endif
      .fp_lsu_mem_req_o         ( fp_lsu_mem_req_o       ),
      .fp_lsu_mem_rsp_i         ( fp_lsu_mem_rsp_i       ),
      .fp_lsu_mem_finished_o    ( fp_lsu_mem_finished    ),
      .fp_lsu_mem_str_finished_o( fp_lsu_mem_str_finished),
      // Spatz VLSU side channel
      .spatz_mem_finished_i     ( spatz_mem_finished     ),
      .spatz_mem_str_finished_i ( spatz_mem_str_finished )
    );
  end: gen_fpu_sequencer

  /////////
  // VRF //
  /////////

  // Write ports
  vrf_addr_t [NrWritePorts-1:0] vrf_waddr;
  vrf_data_t [NrWritePorts-1:0] vrf_wdata;
  logic      [NrWritePorts-1:0] vrf_we;
  vrf_be_t   [NrWritePorts-1:0] vrf_wbe;
  logic      [NrWritePorts-1:0] vrf_wvalid;
  // Read ports
  vrf_addr_t [NrReadPorts-1:0]  vrf_raddr;
  logic      [NrReadPorts-1:0]  vrf_re;
  vrf_data_t [NrReadPorts-1:0]  vrf_rdata;
  logic      [NrReadPorts-1:0]  vrf_rvalid;

  spatz_vrf #(
    .NrReadPorts (NrReadPorts ),
    .NrWritePorts(NrWritePorts)
  ) i_vrf (
    .clk_i     (clk_i     ),
    .rst_ni    (rst_ni    ),
    .testmode_i(testmode_i),
    // Write Ports
    .waddr_i   (vrf_waddr ),
    .wdata_i   (vrf_wdata ),
    .we_i      (vrf_we    ),
    .wbe_i     (vrf_wbe   ),
    .wvalid_o  (vrf_wvalid),
    // Read Ports
    .raddr_i   (vrf_raddr ),
    .re_i      (vrf_re    ),
    .rdata_o   (vrf_rdata ),
    .rvalid_o  (vrf_rvalid)
  );

  ////////////////
  // Controller //
  ////////////////

  // Scoreboard read enable and write enable input signals
  logic      [NrReadPorts-1:0]              sb_re;
  logic      [NrWritePorts-1:0]             sb_we;
  spatz_id_t [NrReadPorts+NrWritePorts-1:0] sb_id;

  spatz_controller #(
    .NrVregfilePorts  (NrReadPorts+NrWritePorts),
    .NrWritePorts     (NrWritePorts            ),
    .RegisterRsp      (RegisterRsp             ),
    .spatz_issue_req_t(spatz_issue_req_t       ),
    .spatz_issue_rsp_t(spatz_issue_rsp_t       ),
    .spatz_rsp_t      (spatz_rsp_t             )
  ) i_controller (
    .clk_i            (clk_i           ),
    .rst_ni           (rst_ni          ),
    // X-intf
    .issue_valid_i    (issue_valid     ),
    .issue_ready_o    (issue_ready     ),
    .issue_req_i      (issue_req       ),
    .issue_rsp_o      (issue_rsp       ),
    .rsp_valid_o      (resp_valid      ),
    .rsp_ready_i      (resp_ready      ),
    .rsp_o            (resp            ),
    // FPU side channel
    .fpu_rnd_mode_i   (fpu_rnd_mode_i  ),
    .fpu_fmt_mode_i   (fpu_fmt_mode_i  ),
    // Spatz request
    .spatz_req_valid_o(spatz_req_valid ),
    .spatz_req_o      (spatz_req       ),
    // VFU
    .vfu_req_ready_i  (vfu_req_ready   ),
    .vfu_rsp_valid_i  (vfu_rsp_valid   ),
    .vfu_rsp_ready_o  (vfu_rsp_ready   ),
    .vfu_rsp_i        (vfu_rsp         ),
    // VLSU
    .vlsu_req_ready_i (vlsu_req_ready  ),
    .vlsu_rsp_valid_i (vlsu_rsp_valid  ),
    .vlsu_rsp_i       (vlsu_rsp        ),
    // VLSD
    .vsldu_req_ready_i(vsldu_req_ready ),
    .vsldu_rsp_valid_i(vsldu_rsp_valid ),
    .vsldu_rsp_i      (vsldu_rsp       ),
    // Scoreboard check
    .sb_id_i          (sb_id           ),
    .sb_wrote_result_i(vrf_wvalid      ),
    .sb_enable_i      ({sb_we, sb_re}  ),
    .sb_enable_o      ({vrf_we, vrf_re})
  );

  /////////
  // VFU //
  /////////

  spatz_vfu #(
    .FPUImplementation(FPUImplementation)
  ) i_vfu (
    .clk_i            (clk_i                                                   ),
    .rst_ni           (rst_ni                                                  ),
    .hart_id_i        (hart_id_i                                               ),
    // Request
    .spatz_req_i      (spatz_req                                               ),
    .spatz_req_valid_i(spatz_req_valid                                         ),
    .spatz_req_ready_o(vfu_req_ready                                           ),
    // Response
    .vfu_rsp_valid_o  (vfu_rsp_valid                                           ),
    .vfu_rsp_ready_i  (vfu_rsp_ready                                           ),
    .vfu_rsp_o        (vfu_rsp                                                 ),
    // VRF
    .vrf_waddr_o      (vrf_waddr[VFU_VD_WD]                                    ),
    .vrf_wdata_o      (vrf_wdata[VFU_VD_WD]                                    ),
    .vrf_we_o         (sb_we[VFU_VD_WD]                                        ),
    .vrf_wbe_o        (vrf_wbe[VFU_VD_WD]                                      ),
    .vrf_wvalid_i     (vrf_wvalid[VFU_VD_WD]                                   ),
    .vrf_raddr_o      (vrf_raddr[VFU_VD_RD:VFU_VS2_RD]                         ),
    .vrf_re_o         (sb_re[VFU_VD_RD:VFU_VS2_RD]                             ),
    .vrf_rdata_i      (vrf_rdata[VFU_VD_RD:VFU_VS2_RD]                         ),
    .vrf_rvalid_i     (vrf_rvalid[VFU_VD_RD:VFU_VS2_RD]                        ),
    .vrf_id_o         ({sb_id[SB_VFU_VD_WD], sb_id[SB_VFU_VD_RD:SB_VFU_VS2_RD]}),
    // FPU side-channel
    .fpu_status_o     (fpu_status_o                                            )
  );

  //////////
  // VLSU //
  //////////

  spatz_vlsu #(
    .NrMemPorts      (NrMemPorts      ),
    .spatz_mem_req_t (spatz_mem_req_t ),
    .spatz_mem_rsp_t (spatz_mem_rsp_t ),
    .NrOutstandingLoads(NumOutstandingLoads )
  ) i_vlsu (
    .clk_i                   (clk_i                                                ),
    .rst_ni                  (rst_ni                                               ),
    // Request
    .spatz_req_i             (spatz_req                                            ),
    .spatz_req_valid_i       (spatz_req_valid                                      ),
    .spatz_req_ready_o       (vlsu_req_ready                                       ),
    // Response
    .vlsu_rsp_valid_o        (vlsu_rsp_valid                                       ),
    .vlsu_rsp_o              (vlsu_rsp                                             ),
    // VRF
    .vrf_waddr_o             (vrf_waddr[VLSU_VD_WD]                                ),
    .vrf_wdata_o             (vrf_wdata[VLSU_VD_WD]                                ),
    .vrf_we_o                (sb_we[VLSU_VD_WD]                                    ),
    .vrf_wbe_o               (vrf_wbe[VLSU_VD_WD]                                  ),
    .vrf_wvalid_i            (vrf_wvalid[VLSU_VD_WD]                               ),
    .vrf_raddr_o             (vrf_raddr[VLSU_VD_RD:VLSU_VS2_RD]                    ),
    .vrf_re_o                (sb_re[VLSU_VD_RD:VLSU_VS2_RD]                        ),
    .vrf_rdata_i             (vrf_rdata[VLSU_VD_RD:VLSU_VS2_RD]                    ),
    .vrf_rvalid_i            (vrf_rvalid[VLSU_VD_RD:VLSU_VS2_RD]                   ),
    .vrf_id_o                ({sb_id[SB_VLSU_VD_WD], sb_id[VLSU_VD_RD:VLSU_VS2_RD]}),
    // Interface Memory
    .spatz_mem_req_o         (spatz_mem_req_o                                      ),
    .spatz_mem_req_valid_o   (spatz_mem_req_valid_o                                ),
    .spatz_mem_req_ready_i   (spatz_mem_req_ready_i                                ),
    .spatz_mem_rsp_i         (spatz_mem_rsp_i                                      ),
    .spatz_mem_rsp_valid_i   (spatz_mem_rsp_valid_i                                ),
    .spatz_mem_rsp_ready_o   (spatz_mem_rsp_ready_o                                ),
    .spatz_mem_finished_o    (spatz_mem_finished                                   ),
    .spatz_mem_str_finished_o(spatz_mem_str_finished                               )
  );

  ///////////
  // VSLDU //
  ///////////

  spatz_vsldu i_vsldu (
    .clk_i            (clk_i                                          ),
    .rst_ni           (rst_ni                                         ),
    // Request
    .spatz_req_i      (spatz_req                                      ),
    .spatz_req_valid_i(spatz_req_valid                                ),
    .spatz_req_ready_o(vsldu_req_ready                                ),
    // Response
    .vsldu_rsp_valid_o(vsldu_rsp_valid                                ),
    .vsldu_rsp_o      (vsldu_rsp                                      ),
    // VRF
    .vrf_waddr_o      (vrf_waddr[VSLDU_VD_WD]                         ),
    .vrf_wdata_o      (vrf_wdata[VSLDU_VD_WD]                         ),
    .vrf_we_o         (sb_we[VSLDU_VD_WD]                             ),
    .vrf_wbe_o        (vrf_wbe[VSLDU_VD_WD]                           ),
    .vrf_wvalid_i     (vrf_wvalid[VSLDU_VD_WD]                        ),
    .vrf_raddr_o      (vrf_raddr[VSLDU_VS2_RD]                        ),
    .vrf_re_o         (sb_re[VSLDU_VS2_RD]                            ),
    .vrf_rdata_i      (vrf_rdata[VSLDU_VS2_RD]                        ),
    .vrf_rvalid_i     (vrf_rvalid[VSLDU_VS2_RD]                       ),
    .vrf_id_o         ({sb_id[SB_VSLDU_VD_WD], sb_id[SB_VSLDU_VS2_RD]})
  );

  ////////////////
  // Assertions //
  ////////////////

  if (spatz_pkg::N_IPU == 0)
    $error("[spatz] Each Spatz needs at least one IPU");

  if (spatz_pkg::N_FU != 2**$clog2(spatz_pkg::N_FU))
    $error("[spatz] The number of FUs needs to be a power of two");

  if (spatz_pkg::VLEN != 2**$clog2(spatz_pkg::VLEN))
    $error("[spatz] The vector length needs to be a power of two");

  if (spatz_pkg::ELEN*spatz_pkg::N_FU > spatz_pkg::VLEN)
    $error("[spatz] VLEN needs a min size of N_FU*%d.", spatz_pkg::ELEN);

  if (NrMemPorts == 0)
    $error("[spatz] Spatz requires at least one memory port.");

endmodule : spatz
