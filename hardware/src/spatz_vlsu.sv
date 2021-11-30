// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vlsu import spatz_pkg::*; (
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
  output logic                        x_mem_valid_o,
  input  logic                        x_mem_ready_i,
  output core_v_xif_pkg::x_mem_req_t  x_mem_req_o,
  input  core_v_xif_pkg::x_mem_resp_t x_mem_resp_i,
  //X-Interface Memory Result
  input  logic                          x_mem_result_valid_i,
  input  core_v_xif_pkg::x_mem_result_t x_mem_result_i
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

  assign spatz_req_ready_o = vlsu_is_ready;

  ///////////////////
  // State Handler //
  ///////////////////

  always_comb begin : proc_state
    spatz_req_d = spatz_req_q;

    if (new_request) begin
      spatz_req_d = spatz_req_i;
    end
  end

  /////////////////
  // Mem Request //
  /////////////////

  logic result_queue_empty, result_queue_full, result_queue_push, result_queue_pop;
  core_v_xif_pkg::x_mem_result_t result_queue_data;

  always_comb begin : proc_mem_req
    x_mem_req_o = '0;
    x_mem_valid_o = 1'b0;
    result_queue_pop = 1'b0;
    if (spatz_req_d.op == VLE && new_request) begin
      x_mem_req_o.id = spatz_req_i.id;
      x_mem_req_o.addr = spatz_req_i.rs1;
      x_mem_valid_o = 1'b1;
    end else if (spatz_req_i.op == VSE && new_request) begin
      if (!result_queue_empty) begin
        x_mem_req_o.id = spatz_req_i.id;
        x_mem_req_o.addr = spatz_req_i.rs1;
        x_mem_req_o.we = 1'b1;
        x_mem_req_o.wdata = result_queue_data.rdata;
        x_mem_valid_o = 1'b1;
        result_queue_pop = 1'b1;
      end
    end
  end

  ////////////////
  // Mem Result //
  ////////////////

  // Fifo storing new loaded values not yet saved to vregfile
  fifo_v3 #(
    .FALL_THROUGH( 1                     ),
    .DATA_WIDTH  ( $bits(x_mem_result_i) ),
    .DEPTH       ( 4                     )
  ) i_fifo (
    .clk_i     ( clk_i              ),
    .rst_ni    ( rst_ni             ),
    .flush_i   ( 1'b0               ),
    .testmode_i( 1'b0               ),
    .full_o    ( result_queue_full  ),
    .empty_o   ( result_queue_empty ),
    .usage_o   ( /* Unused */       ),
    .data_i    ( x_mem_result_i     ),
    .push_i    ( result_queue_push  ),
    .data_o    ( result_queue_data  ),
    .pop_i     ( result_queue_pop   )
  );

  always_comb begin : proc_mem_res
    result_queue_push = 1'b0;
    if (x_mem_result_valid_i && !result_queue_full) begin
      result_queue_push = 1'b1;
    end

    if (!result_queue_empty) begin
      //vrf_wdata_o =
    end
  end

  assign vrf_waddr_o = '0;
  assign vrf_wdata_o = '0;
  assign vrf_we_o = '0;
  assign vrf_wbe_o = '0;
  assign vrf_raddr_o = '0;
  assign vrf_re_o = '0;

endmodule : spatz_vlsu
