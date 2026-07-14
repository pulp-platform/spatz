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

module spatz_scalar_only import spatz_pkg::*; import rvv_pkg::*; import fpnew_pkg::*; #(
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
    // Memory Finished
    output logic             [1:0]            spatz_mem_finished_o,
    output logic             [1:0]            spatz_mem_str_finished_o,
    // FPU memory interface interface
    output dreq_t                             fp_lsu_mem_req_o,
    input  drsp_t                             fp_lsu_mem_rsp_i,
    // FPU side channel
    input  roundmode_e                        fpu_rnd_mode_i,
    input  fmt_mode_t                         fpu_fmt_mode_i,
    output status_t                           fpu_status_o
  );

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
  assign spatz_mem_req_o = '0;
  assign spatz_mem_req_valid_o = '0;
  assign spatz_mem_finished_o     = {1'b0, fp_lsu_mem_finished};
  assign spatz_mem_str_finished_o = {1'b0, fp_lsu_mem_str_finished};

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
    .fp_lsu_mem_req_o         ( fp_lsu_mem_req_o       ),
    .fp_lsu_mem_rsp_i         ( fp_lsu_mem_rsp_i       ),
    .fp_lsu_mem_finished_o    ( fp_lsu_mem_finished    ),
    .fp_lsu_mem_str_finished_o( fp_lsu_mem_str_finished),
    // Spatz VLSU side channel
    .spatz_mem_finished_i     ( 1'b0                   ),
    .spatz_mem_str_finished_i ( 1'b0                   )
  );

  ////////////////
  // Controller //
  ////////////////

  spatz_scalar_unit #(
    .RegisterRsp      (RegisterRsp             ),
    .spatz_issue_req_t(spatz_issue_req_t       ),
    .spatz_issue_rsp_t(spatz_issue_rsp_t       ),
    .spatz_rsp_t      (spatz_rsp_t             ),
    .FPUImplementation(FPUImplementation)
  ) i_scalar (
    .clk_i            (clk_i           ),
    .rst_ni           (rst_ni          ),
    .hart_id_i        (hart_id_i      ),
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
    .fpu_status_o     (fpu_status_o    )
  );

endmodule : spatz_scalar_only
