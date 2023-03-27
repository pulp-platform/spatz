// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// This is the toplevel module of Spatz. It contains all other SPatz modules.
// This includes the Controller, which interfaces with the main core and handles
// instruction decoding, operation issuing to the other units, and result write
// back to the core. The Vector Function Unit (VFU) is the high high throughput
// unit that executes all arithmetic and logical operations. The Load/Store Unit
// (LSU) is used to load vectors from memory to the register file and store them
// back again. Finally, the Vector Register File (VRF) is the main register file
// that stores all of the currently used vectors close to the execution units.

module spatz import spatz_pkg::*; import rvv_pkg::*; import fpnew_pkg::*; import spatz_xif_pkg::*; #(
    parameter  int unsigned NrMemPorts          = 1,
    // Derived parameters. DO NOT CHANGE!
    localparam int unsigned NumOutstandingLoads = snitch_pkg::NumIntOutstandingLoads
  ) (
    input logic                             clk_i,
    input logic                             rst_ni,
    // X-Interface Issue
    input logic                             x_issue_valid_i,
    output logic                            x_issue_ready_o,
    input spatz_x_issue_req_t               x_issue_req_i,
    output spatz_x_issue_resp_t             x_issue_resp_o,
    // X-Interface Result
    output logic                            x_result_valid_o,
    input logic                             x_result_ready_i,
    output spatz_x_result_t                 x_result_o,
    // Memory Request
    output spatz_mem_req_t [NrMemPorts-1:0] spatz_mem_req_o,
    output logic [NrMemPorts-1:0]           spatz_mem_req_valid_o,
    input logic [NrMemPorts-1:0]            spatz_mem_req_ready_i,
    input spatz_mem_resp_t [NrMemPorts-1:0] spatz_mem_resp_i,
    input logic [NrMemPorts-1:0]            spatz_mem_resp_valid_i,
    output logic [NrMemPorts-1:0]           spatz_mem_resp_ready_o,
    // Memory Finished
    output logic [1:0]                      spatz_mem_finished_o,
    output logic [1:0]                      spatz_mem_str_finished_o,
    // FPU memory interface interface
    output spatz_mem_req_t                  fp_lsu_mem_req_o,
    output logic                            fp_lsu_mem_req_valid_o,
    input logic                             fp_lsu_mem_req_ready_i,
    input spatz_mem_resp_t                  fp_lsu_mem_resp_i,
    input logic                             fp_lsu_mem_resp_valid_i,
    output logic                            fp_lsu_mem_resp_ready_o,
    // FPU side channel
    input roundmode_e                       fpu_rnd_mode_i,
    output status_t                         fpu_status_o
  );

  ////////////////
  // Parameters //
  ////////////////

  // Number of ports of the vector register file
  localparam NrWritePorts = 3;
  localparam NrReadPorts  = 6;

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
  spatz_x_issue_req_t  x_issue_req;
  logic                x_issue_valid;
  logic                x_issue_ready;
  spatz_x_issue_resp_t x_issue_resp;
  spatz_x_result_t     x_result;
  logic                x_result_valid;
  logic                x_result_ready;

  // Did we finish a memory request?
  logic fp_lsu_mem_finished;
  logic fp_lsu_mem_str_finished;
  logic spatz_mem_finished;
  logic spatz_mem_str_finished;
  assign spatz_mem_finished_o     = {spatz_mem_finished, fp_lsu_mem_finished};
  assign spatz_mem_str_finished_o = {spatz_mem_str_finished, fp_lsu_mem_str_finished};

  if (!FPU) begin: gen_no_fpu_sequencer
    // Spatz configured without an FPU. Just forward the requests to Spatz.
    assign x_issue_req     = x_issue_req_i;
    assign x_issue_valid   = x_issue_valid_i;
    assign x_issue_ready_o = x_issue_ready;
    assign x_issue_resp_o  = x_issue_resp;

    assign x_result_o       = x_result;
    assign x_result_valid_o = x_result_valid;
    assign x_result_ready   = x_result_ready_i;

    // Tie the memory interface to zero
    assign fp_lsu_mem_req_o        = '0;
    assign fp_lsu_mem_req_valid_o  = 1'b0;
    assign fp_lsu_mem_resp_ready_o = 1'b0;
    assign fp_lsu_mem_finished     = 1'b0;
    assign fp_lsu_mem_str_finished = 1'b0;
  end: gen_no_fpu_sequencer else begin: gen_fpu_sequencer
    spatz_fpu_sequencer #(
      .x_issue_req_t      (spatz_x_issue_req_t ),
      .x_issue_resp_t     (spatz_x_issue_resp_t),
      .x_result_t         (spatz_x_result_t    ),
      .NumOutstandingLoads(NumOutstandingLoads )
    ) i_fpu_sequencer (
      .clk_i                    (clk_i                  ),
      .rst_ni                   (rst_ni                 ),
      // Snitch interface
      .x_issue_req_i            (x_issue_req_i          ),
      .x_issue_valid_i          (x_issue_valid_i        ),
      .x_issue_ready_o          (x_issue_ready_o        ),
      .x_issue_resp_o           (x_issue_resp_o         ),
      .x_result_o               (x_result_o             ),
      .x_result_valid_o         (x_result_valid_o       ),
      .x_result_ready_i         (x_result_ready_i       ),
      // Spatz interface
      .x_issue_req_o            (x_issue_req            ),
      .x_issue_valid_o          (x_issue_valid          ),
      .x_issue_ready_i          (x_issue_ready          ),
      .x_issue_resp_i           (x_issue_resp           ),
      .x_result_i               (x_result               ),
      .x_result_valid_i         (x_result_valid         ),
      .x_result_ready_o         (x_result_ready         ),
      // Memory interface
      .fp_lsu_mem_req_o         (fp_lsu_mem_req_o       ),
      .fp_lsu_mem_req_valid_o   (fp_lsu_mem_req_valid_o ),
      .fp_lsu_mem_req_ready_i   (fp_lsu_mem_req_ready_i ),
      .fp_lsu_mem_resp_i        (fp_lsu_mem_resp_i      ),
      .fp_lsu_mem_resp_valid_i  (fp_lsu_mem_resp_valid_i),
      .fp_lsu_mem_resp_ready_o  (fp_lsu_mem_resp_ready_o),
      .fp_lsu_mem_finished_o    (fp_lsu_mem_finished    ),
      .fp_lsu_mem_str_finished_o(fp_lsu_mem_str_finished),
      // Spatz VLSU side channel
      .spatz_mem_finished_i     (spatz_mem_finished     ),
      .spatz_mem_str_finished_i (spatz_mem_str_finished )
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
    .clk_i   (clk_i     ),
    .rst_ni  (rst_ni    ),
    // Write Ports
    .waddr_i (vrf_waddr ),
    .wdata_i (vrf_wdata ),
    .we_i    (vrf_we    ),
    .wbe_i   (vrf_wbe   ),
    .wvalid_o(vrf_wvalid),
    // Read Ports
    .raddr_i (vrf_raddr ),
    .re_i    (vrf_re    ),
    .rdata_o (vrf_rdata ),
    .rvalid_o(vrf_rvalid)
  );

  ////////////////
  // Controller //
  ////////////////

  // Scoreboard read enable and write enable input signals
  logic      [NrReadPorts-1:0]              sb_re;
  logic      [NrWritePorts-1:0]             sb_we;
  spatz_id_t [NrReadPorts+NrWritePorts-1:0] sb_id;

  spatz_controller #(
    .NrVregfilePorts(NrReadPorts+NrWritePorts),
    .NrWritePorts   (NrWritePorts            ),
    .x_issue_req_t  (spatz_x_issue_req_t     ),
    .x_issue_resp_t (spatz_x_issue_resp_t    ),
    .x_result_t     (spatz_x_result_t        )
  ) i_controller (
    .clk_i            (clk_i           ),
    .rst_ni           (rst_ni          ),
    // X-intf
    .x_issue_valid_i  (x_issue_valid   ),
    .x_issue_ready_o  (x_issue_ready   ),
    .x_issue_req_i    (x_issue_req     ),
    .x_issue_resp_o   (x_issue_resp    ),
    .x_result_valid_o (x_result_valid  ),
    .x_result_ready_i (x_result_ready  ),
    .x_result_o       (x_result        ),
    // FPU side channel
    .fpu_rnd_mode_i   (fpu_rnd_mode_i  ),
    // Spatz req
    .spatz_req_valid_o(spatz_req_valid ),
    .spatz_req_o      (spatz_req       ),
    // VFU
    .vfu_req_ready_i  (vfu_req_ready   ),
    .vfu_rsp_valid_i  (vfu_rsp_valid   ),
    .vfu_rsp_ready_o  (vfu_rsp_ready   ),
    .vfu_rsp_i        (vfu_rsp         ),
    // VFU
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

  spatz_vfu i_vfu (
    .clk_i            (clk_i                                                   ),
    .rst_ni           (rst_ni                                                  ),
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
    .NrMemPorts (NrMemPorts )
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
    // X-Interface Memory
    .spatz_mem_req_o         (spatz_mem_req_o                                      ),
    .spatz_mem_req_valid_o   (spatz_mem_req_valid_o                                ),
    .spatz_mem_req_ready_i   (spatz_mem_req_ready_i                                ),
    .spatz_mem_resp_i        (spatz_mem_resp_i                                     ),
    .spatz_mem_resp_valid_i  (spatz_mem_resp_valid_i                               ),
    .spatz_mem_resp_ready_o  (spatz_mem_resp_ready_o                               ),
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
