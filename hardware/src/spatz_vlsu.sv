// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vlsu import spatz_pkg::*; #(
  parameter NR_MEM_PORTS         = 1,
  parameter NR_OUTSTANDING_LOADS = 8,
  parameter type x_mem_req_t     = logic,
  parameter type x_mem_resp_t    = logic,
  parameter type x_mem_result_t  = logic
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Spatz req
  input  spatz_req_t spatz_req_i,
  input  logic       spatz_req_valid_i,
  output logic       spatz_req_ready_o,
  // VFU rsp
  output logic      vlsu_rsp_valid_o,
  output vlsu_rsp_t vlsu_rsp_o,
  // VRF
  output vreg_addr_t vrf_waddr_o,
  output vreg_data_t vrf_wdata_o,
  output logic       vrf_we_o,
  output vreg_be_t   vrf_wbe_o,
  input  logic       vrf_wvalid_i,
  output vreg_addr_t vrf_raddr_o,
  output logic       vrf_re_o,
  input  vreg_data_t vrf_rdata_i,
  input  logic       vrf_rvalid_i,
  // X-Interface Memory Request
  output logic          [NR_MEM_PORTS-1:0] x_mem_valid_o,
  input  logic          [NR_MEM_PORTS-1:0] x_mem_ready_i,
  output x_mem_req_t    [NR_MEM_PORTS-1:0] x_mem_req_o,
  input  x_mem_resp_t   [NR_MEM_PORTS-1:0] x_mem_resp_i,
  //X-Interface Memory Result
  input  logic          [NR_MEM_PORTS-1:0] x_mem_result_valid_i,
  input  x_mem_result_t [NR_MEM_PORTS-1:0] x_mem_result_i
);

  // Include FF
  `include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req_d, spatz_req_q;
  `FF(spatz_req_q, spatz_req_d, '0)

  // Is vfu and the ipu operands ready
  logic vlsu_is_ready;
  assign vlsu_is_ready = 1'b1;
  logic op_data_is_ready, op_offset_is_ready;
  assign op_data_is_ready = 1'b1;
  assign op_offset_is_ready = 1'b1;

  // Has a new vfu execution request arrived
  logic new_request;
  assign new_request = spatz_req_valid_i && vlsu_is_ready && (spatz_req_i.ex_unit == LSU);

  // Is instruction a load
  logic is_load;
  assign is_load = (spatz_req_q.op == VLE) || (spatz_req_q.op == VLSE) || (spatz_req_q.op == VLXE);

  assign spatz_req_ready_o = vlsu_is_ready;

  assign vlsu_rsp_valid_o = '0;
  assign vlsu_rsp_o = '0;

  ///////////////////
  // State Handler //
  ///////////////////

  vreg_addr_t [2:0] vreg_addr_q, vreg_addr_d;

  always_comb begin : proc_state
    spatz_req_d = spatz_req_q;

    if (new_request) begin
      spatz_req_d = spatz_req_i;
    end
    if (vreg_addr_q == '1) begin
      spatz_req_d.vl = '0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin : proc_vreg_addr_q
    if(~rst_ni) begin
      vreg_addr_q <= 0;
    end else begin
      vreg_addr_q <= vreg_addr_d;
    end
  end

  always_comb begin : proc_
    vrf_waddr_o = '0;
    vrf_wdata_o = '0;
    vrf_we_o = '0;
    vrf_wbe_o = '0;
    vreg_addr_d = vreg_addr_q;
    if (spatz_req_q.vl != '0 && spatz_req_q.rs1 == '0) begin
      vrf_waddr_o = vreg_addr_q;
      vrf_wdata_o = '0;
      vrf_we_o = '1;
      vrf_wbe_o = '1;

      if (vrf_wvalid_i) begin
        vreg_addr_d = vreg_addr_q + 1;
      end
    end
  end

  /////////////////
  // Mem Request //
  /////////////////

  logic result_queue_empty, result_queue_full, result_queue_push, result_queue_pop;
  x_mem_result_t result_queue_data;

  always_comb begin : proc_mem_req
    x_mem_req_o = '0;
    x_mem_valid_o = '0;
    result_queue_pop = 1'b0;
    if (spatz_req_d.op == VLE && new_request && spatz_req_d.rs1 != '0) begin
      x_mem_req_o[0].id = spatz_req_i.id;
      x_mem_req_o[0].addr = spatz_req_i.rs1;
      x_mem_valid_o[0] = 1'b1;
    end else if (spatz_req_i.op == VSE && new_request && spatz_req_d.rs1 != '0) begin
      if (!result_queue_empty) begin
        x_mem_req_o[1].id = spatz_req_i.id;
        x_mem_req_o[1].addr = spatz_req_i.rs1;
        x_mem_req_o[1].we = 1'b1;
        x_mem_req_o[1].wdata = result_queue_data.rdata;
        x_mem_valid_o[1] = 1'b1;
        result_queue_pop = 1'b1;
      end
    end
  end

  ////////////////
  // Mem Result //
  ////////////////

  // Fifo storing new loaded values not yet saved to vregfile
  fifo_v3 #(
    .FALL_THROUGH( 1                       ),
    .DATA_WIDTH  ( $bits(x_mem_result_i[0])),
    .DEPTH       ( 4                       )
  ) i_fifo (
    .clk_i     ( clk_i              ),
    .rst_ni    ( rst_ni             ),
    .flush_i   ( 1'b0               ),
    .testmode_i( 1'b0               ),
    .full_o    ( result_queue_full  ),
    .empty_o   ( result_queue_empty ),
    .usage_o   ( /* Unused */       ),
    .data_i    ( x_mem_result_i[0]  ),
    .push_i    ( result_queue_push  ),
    .data_o    ( result_queue_data  ),
    .pop_i     ( result_queue_pop   )
  );

  always_comb begin : proc_mem_res
    result_queue_push = 1'b0;
    if (x_mem_result_valid_i[0] && !result_queue_full) begin
      result_queue_push = 1'b1;
    end

    if (!result_queue_empty) begin
      //vrf_wdata_o =
    end
  end

  assign vrf_raddr_o = '0;
  assign vrf_re_o = '0;

endmodule : spatz_vlsu
