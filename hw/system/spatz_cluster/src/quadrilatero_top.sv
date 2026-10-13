// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Danilo Cammarata, ETH Zurich

module quadrilatero_top import fpnew_pkg::*; import quadrilatero_pkg::*; #(
    parameter int unsigned  NrMemPorts       = 1,
    // Memory request (VLSU)
    parameter type          quad_mem_req_t   = logic,
    parameter type          quad_mem_rsp_t   = logic,
    // Snitch interface
    parameter type          quad_issue_req_t = logic,
    parameter type          quad_issue_rsp_t = logic,
    parameter type          quad_rsp_t       = logic,
    // ReqRsp
    parameter type          amo_op_e         = logic,
    parameter amo_op_e      AMONone          = 1'b0 ,
    // Derived parameters. DO NOT CHANGE!
    localparam int unsigned DataWidth        = quadrilatero_pkg::DATA_WIDTH + hsiao_ecc_pkg::min_ecc(quadrilatero_pkg::DATA_WIDTH),
    localparam int unsigned LLEN             = quadrilatero_pkg::LLEN      ,
    localparam int unsigned LSU_PORTS        = quadrilatero_pkg::LSU_PORTS 
  ) (
    input  logic                              clk_i,
    input  logic                              rst_ni,
    // Snitch Interface
    input  logic           [2:0]              issue_valid_i,
    output logic           [2:0]              issue_ready_o,
    input  quad_issue_req_t[2:0]              issue_req_i,
    output quad_issue_rsp_t[2:0]              issue_rsp_o,
    output logic           [2:0]              rsp_valid_o,
    input  logic           [2:0]              rsp_ready_i,
    output quad_rsp_t      [2:0]              rsp_o,
    // Memory Request
    output quad_mem_req_t   [NrMemPorts-1:0] quad_mem_req_o,
    output logic            [NrMemPorts-1:0] quad_mem_req_valid_o,
    input  logic            [NrMemPorts-1:0] quad_mem_req_ready_i,
    input  quad_mem_rsp_t   [NrMemPorts-1:0] quad_mem_rsp_i,
    input  logic            [NrMemPorts-1:0] quad_mem_rsp_valid_i
  );

  // ------------------
  // XIF Signals
  // ------------------
  logic                  [2:0]  quad_result_valid; 
  logic                  [2:0]  quad_result_ready; 
  xif_pkg::x_issue_req_t [2:0]  quad_issue_req   ;
  xif_pkg::x_issue_resp_t[2:0]  quad_issue_resp  ;
  xif_pkg::x_commit_t    [2:0]  quad_commit      ;
  logic                  [2:0]  quad_commit_valid;
  xif_pkg::x_result_t    [2:0]  quad_result      ;

  logic[xif_pkg::X_ID_WIDTH-1 : 0] id_cnt_d [2:0];
  logic[xif_pkg::X_ID_WIDTH-1 : 0] id_cnt_q [2:0];
  logic[xif_pkg::X_ID_WIDTH-1 : 0] seq_cnt_q[2:0];
  logic[xif_pkg::X_ID_WIDTH-1 : 0] voter_q  [2:0];

  for(genvar i=0; i<3; i++) begin
    always_comb begin: xif_signals

      // ID Conversion
      id_cnt_d[i]   = id_cnt_q[i] + (issue_valid_i[i] & issue_ready_o[i]);
        
      // XIF Issue Request
      quad_issue_req[i].instr     = issue_req_i[i].data_op;
      quad_issue_req[i].mode      = 2'b11;
      quad_issue_req[i].id        = id_cnt_q[i];
      quad_issue_req[i].rs[0]     = issue_req_i[i].data_arga;
      quad_issue_req[i].rs[1]     = issue_req_i[i].data_argb;
      quad_issue_req[i].rs[2]     = issue_req_i[i].data_argc;
      quad_issue_req[i].rs_valid  = '1  ;
      quad_issue_req[i].ecs       = '0  ;
      quad_issue_req[i].ecs_valid = 1'b1;

      // XIF Issue Response
      issue_rsp_o[i].accept    = quad_issue_resp[i].accept;
      issue_rsp_o[i].writeback = quad_issue_resp[i].writeback;
      issue_rsp_o[i].loadstore = quad_issue_resp[i].loadstore;
      issue_rsp_o[i].exception = quad_issue_resp[i].exc;
      issue_rsp_o[i].isfloat   = 1'b0;

      // XIF Commit
      quad_commit_valid[i]        = issue_valid_i[i];
      quad_commit[i].id           = id_cnt_q[i]   ;
      quad_commit[i].commit_kill  = 1'b0;

      // XIF Result
      rsp_o[i].id      = quad_result[i].rd  ;
      rsp_o[i].error   = quad_result[i].err ;
      rsp_o[i].data    = quad_result[i].data;
      rsp_valid_o[i]   = quad_result[i].we & quad_result_valid[i];
      quad_result_ready[i] = !rsp_valid_o[i] | rsp_ready_i[i];
    end
  end
  
  // ------------------
  // Memory Signals
  // ------------------
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0]                    quad_obi_req   ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0]                    quad_obi_we    ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0][DataWidth/8 - 1:0] quad_obi_be    ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0][31:0]              quad_obi_addr  ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0][DataWidth - 1:0]   quad_obi_wdata ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0]                    quad_obi_gnt   ;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0]                    quad_obi_rvalid;
  logic [LSU_PORTS-1:0][NrMemPorts/LSU_PORTS-1:0][DataWidth - 1:0]   quad_obi_rdata ;

  for (genvar lsu = 0; lsu < LSU_PORTS; lsu++) begin
    for (genvar port = 0; port < NrMemPorts/LSU_PORTS; port++) begin
      if(port < LLEN/LSU_PORTS/quadrilatero_pkg::DATA_WIDTH) begin
        // TCDM request
        assign quad_mem_req_valid_o[port+lsu*NrMemPorts/LSU_PORTS] = quad_obi_req  [lsu][port];
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].write = quad_obi_we   [lsu][port];
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].strb  = quad_obi_be   [lsu][port];
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].addr  = quad_obi_addr [lsu][port][16:0];
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].data  = quad_obi_wdata[lsu][port];
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].amo   = AMONone;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].user  = '0;

        // TCDM response
        assign quad_obi_rdata [lsu][port] = quad_mem_rsp_i[port+lsu*NrMemPorts/LSU_PORTS].data ;
        assign quad_obi_rvalid[lsu][port] = quad_mem_rsp_valid_i[port+lsu*NrMemPorts/LSU_PORTS];
        assign quad_obi_gnt   [lsu][port] = quad_mem_req_ready_i[port+lsu*NrMemPorts/LSU_PORTS];
      end else begin

        // TCDM request
        assign quad_mem_req_valid_o[port+lsu*NrMemPorts/LSU_PORTS] = '0;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].write = '0;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].strb  = '0;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].addr  = '0;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].data  = '0;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].amo   = AMONone;
        assign quad_mem_req_o[port+lsu*NrMemPorts/LSU_PORTS].user  = '0;        
      end
    end
  end  

  // ------------------
  // Quadrilatero
  // ------------------
  quadrilatero i_quadrilatero (
      .clk_i,
      .rst_ni,

      // OBI signals
      .mem_req_o            (quad_obi_req      ),
      .mem_we_o             (quad_obi_we       ),
      .mem_be_o             (quad_obi_be       ),
      .mem_addr_o           (quad_obi_addr     ),
      .mem_wdata_o          (quad_obi_wdata    ),
      .mem_gnt_i            (quad_obi_gnt      ),
      .mem_rvalid_i         (quad_obi_rvalid   ),
      .mem_rdata_i          (quad_obi_rdata    ),

      // Compressed Interface
      .x_compressed_valid_i (        '0        ),
      .x_compressed_ready_o (/*    Unused    */),
      .x_compressed_req_i   (        '0        ),
      .x_compressed_resp_o  (/*    Unused    */),

      // Issue Interface
      .x_issue_valid_i      (issue_valid_i     ),
      .x_issue_ready_o      (issue_ready_o     ),
      .x_issue_req_i        (quad_issue_req    ),
      .x_issue_resp_o       (quad_issue_resp   ),

      // Commit Interface
      .x_commit_valid_i     (quad_commit_valid ),
      .x_commit_i           (quad_commit       ),

      // Memory Request/Response Interface
      .x_mem_valid_o        (/*    Unused    */),
      .x_mem_ready_i        (        '0        ),
      .x_mem_req_o          (/*    Unused    */),
      .x_mem_resp_i         (        '0        ),

      // Memory Result Interface
      .x_mem_result_valid_i (        '0        ),
      .x_mem_result_i       (        '0        ),

      // Result Interface
      .x_result_valid_o     (quad_result_valid ),
      .x_result_ready_i     (quad_result_ready ),
      .x_result_o           (quad_result       )
  );

  // Sequential block
  for(genvar i=0; i<3; i++) begin
    always_ff @(negedge rst_ni, posedge clk_i) begin : seq_block
      if (!rst_ni) begin
        seq_cnt_q[i]   <= '0;
      end else begin
        seq_cnt_q[i]   <= id_cnt_d[i];
      end
    end
    bitwise_TMR_voter_fail #(
      .DataWidth(xif_pkg::X_ID_WIDTH),
      .VoterType(1)
    ) i_seq_vote (
      .a_i             (seq_cnt_q  [0]),
      .b_i             (seq_cnt_q  [1]),
      .c_i             (seq_cnt_q  [2]),
      .majority_o      (voter_q    [i]),
      .fault_detected_o()
    );
    assign id_cnt_q[i] = voter_q[i];
  end
  
endmodule