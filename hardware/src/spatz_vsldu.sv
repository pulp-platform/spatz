// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The vector slide unit executes all slide instructions

module spatz_vsldu
  import spatz_pkg::*;
  import rvv_pkg::*;
  import cf_math_pkg::idx_width;
(
  input  logic clk_i,
  input  logic rst_ni,

  // Spatz req
  input  spatz_req_t spatz_req_i,
  input  logic       spatz_req_valid_i,
  output logic       spatz_req_ready_o,

  // VSLDU rsp
  output logic       vsldu_rsp_valid_o,
  output vsldu_rsp_t vsldu_rsp_o,

  // VRF
  output vreg_addr_t vrf_waddr_o,
  output vreg_data_t vrf_wdata_o,
  output logic       vrf_we_o,
  output vreg_be_t   vrf_wbe_o,
  input  logic       vrf_wvalid_i,
  output vreg_addr_t vrf_raddr_o,
  output logic       vrf_re_o,
  input  vreg_data_t vrf_rdata_i,
  input  logic       vrf_rvalid_i
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
  logic vsldu_is_ready;

  // Has a new vsldu execution request arrived
  logic new_vsldu_request;
  assign new_vsldu_request = spatz_req_valid_i & vsldu_is_ready & (spatz_req_i.ex_unit == SLD);

  // Is the vector length zero (no active instruction)
  logic  is_vl_zero;
  assign is_vl_zero = spatz_req_q.vl == 'd0;

  // Is the instruction slide up
  logic  is_slide_up;
  assign is_slide_up = spatz_req_q.op == VSLIDEUP;

  vlen_t slide_amount_q, slide_amount_d;
  `FF(slide_amount_q, slide_amount_d, '0)

  // Signal to the controller if we are ready for a new instruction or not
  assign spatz_req_ready_o = vsldu_is_ready;

  // Vector register file counter signals
  logic  vreg_counter_clear;
  logic  vreg_counter_en;
  logic  vreg_counter_load;
  vlen_t vreg_counter_delta;
  vlen_t vreg_counter_load_value;
  vlen_t vreg_counter_value;
  logic  vreg_counter_overflow;

  // Is the register file operation valid and are we at the last one
  logic vreg_operation_valid;
  logic vreg_operation_first;
  logic vreg_operation_last;
  logic vreg_operations_finished;

  ///////////////////
  // State Handler //
  ///////////////////

  // Accept a new operation or clear req register if we are finished
  always_comb begin
    spatz_req_d = spatz_req_q;
    slide_amount_d = slide_amount_q;

    if (new_vsldu_request) begin
      spatz_req_d = spatz_req_i;
      slide_amount_d = spatz_req_i.op_sld.one_up_down ? 'd1 : spatz_req_i.rs1;

      // Convert vl/vstart/slide_amount to number of bytes for all element widths
      if (spatz_req_i.vtype.vsew == EW_16) begin
        spatz_req_d.vl     = spatz_req_i.vl << 1;
        spatz_req_d.vstart = spatz_req_i.vstart << 1;
        slide_amount_d     = slide_amount_d << 1;
      end else if (spatz_req_i.vtype.vsew == EW_32) begin
        spatz_req_d.vl     = spatz_req_i.vl << 2;
        spatz_req_d.vstart = spatz_req_i.vstart << 2;
        slide_amount_d     = slide_amount_d << 2;
      end
    end else if (!is_vl_zero && vsldu_is_ready && !new_vsldu_request) begin
      // If we are ready for a new instruction but there is none, clear req register
      spatz_req_d = '0;
      slide_amount_d = '0;
    end
  end

  // Respond to controller if we are finished executing
  always_comb begin : vsldu_rsp
    vsldu_rsp_valid_o = 1'b0;
    vsldu_rsp_o       = '0;

    // Write back accessed register file vector to clear scoreboard entry
    if (vsldu_is_ready && (spatz_req_q.vl != '0)) begin
      vsldu_rsp_o.id    = spatz_req_q.id;
      vsldu_rsp_o.vd    = spatz_req_q.vd;
      vsldu_rsp_o.vs2   = spatz_req_q.vs2;
      vsldu_rsp_valid_o = 1'b1;
    end
  end

  assign vsldu_is_ready = vreg_operations_finished;

  delta_counter #(
    .WIDTH($bits(vlen_t))
  ) i_delta_counter_vreg (
    .clk_i     (clk_i),
    .rst_ni    (rst_ni),
    .clear_i   (vreg_counter_clear),
    .en_i      (vreg_counter_en),
    .load_i    (vreg_counter_load),
    .down_i    (1'b0), // We always count up
    .delta_i   (vreg_counter_delta),
    .d_i       (vreg_counter_load_value),
    .q_o       (vreg_counter_value),
    .overflow_o(vreg_counter_overflow)
  );

  always_comb begin
    // The total amount of elements we have to work through
    automatic int unsigned max = spatz_req_q.vl;
    // How many elements are left to do
    automatic int unsigned delta = max - vreg_counter_value;
    // Have both vrf read and write executed successfully
    automatic logic vrf_transaction_valid = (vrf_wvalid_i & vrf_we_o) & (vrf_rvalid_i & vrf_re_o);

    vreg_counter_load_value = spatz_req_q.vstart;
    if (!spatz_req_d.op_sld.one_up_down && spatz_req_d.vstart < slide_amount_d && spatz_req_i.op == VSLIDEUP) begin
      vreg_counter_load_value = slide_amount_d;
    end
    vreg_counter_load = new_vsldu_request;

    vreg_operation_valid     = (delta != 'd0) & ~is_vl_zero;
    vreg_operation_first     = vreg_operation_valid &
                               (~spatz_req_q.op_sld.one_up_down && is_slide_up && spatz_req_q.vstart < slide_amount_q ? vreg_counter_value == slide_amount_q :
                               vreg_counter_value == spatz_req_q.vstart);
    vreg_operation_last      = vreg_operation_valid & (delta <= VELEB);

    vreg_counter_clear = 1'b0;
    vreg_counter_delta = ~vreg_operation_valid ? 'd0 :
                         vreg_operation_last   ? delta :
                         vreg_operation_first  ? VELEB - spatz_req_q.vstart[$clog2(VELEB)-1:0] :
                         VELEB;

    vreg_counter_en = vrf_transaction_valid;

    vreg_operations_finished = ~vreg_operation_valid | (vreg_operation_last & vreg_counter_en);
  end


  ////////////////////////
  // Address Generation //
  ////////////////////////

  vlen_t sld_offset_rd, sld_offset_wd;

  always_comb begin
    sld_offset_rd = is_slide_up ? -slide_amount_q[$bits(vlen_t)-1:$clog2(VELEB)] : slide_amount_q[$bits(vlen_t)-1:$clog2(VELEB)];
    vrf_raddr_o = {spatz_req_q.vs2, $clog2(VELE)'(1'b0)} + vreg_counter_value[$bits(vlen_t)-1:$clog2(VELEB)] + sld_offset_rd;

    vrf_waddr_o = {spatz_req_q.vd, $clog2(VELE)'(1'b0)} + vreg_counter_value[$bits(vlen_t)-1:$clog2(VELEB)];
  end

  ////////////
  // Slider //
  ////////////

  vreg_data_t shift_overflow_q, shift_overflow_d;
  `FF(shift_overflow_q, shift_overflow_d, '0)

  logic [$clog2(VELEB)-1:0] in_elem_offset, in_elem_flipped_offset;
  assign in_elem_offset = slide_amount_q[$clog2(VELEB)-1:0];
  assign in_elem_flipped_offset = VELEB - in_elem_offset;

  vreg_data_t data_in, data_out, data_low, data_high;

  always_comb begin
    shift_overflow_d = shift_overflow_q;
    data_in = '0;
    data_out = '0;
    data_high = '0;
    data_low = '0;

    // If we have a slide up operation, flip all bytes around (d[-i] = d[i])
    if (is_slide_up) begin
      for (int b_src = 0; b_src < VELEB; b_src++) begin
        data_in[(VELEB-b_src-1)*8 +: 8] = vrf_rdata_i[b_src*8 +: 8];
      end
    end else begin
      data_in = vrf_rdata_i;
      if (vreg_counter_value >= MAXVL - slide_amount_q) begin
        data_in = '0;
      end
    end

    for (int b_src = 0; b_src < VELEB; b_src++) begin
      if (b_src >= in_elem_offset) begin
        // high elements
        for (int b_dst = 0; b_dst < b_src; b_dst++) begin
          if (b_src-b_dst == in_elem_offset) begin
            data_high[b_dst*8 +: 8] = data_in[b_src*8 +: 8];
          end
        end
      end else begin
        // low elements
        for (int b_dst = b_src; b_dst < VELEB; b_dst++) begin
          if (b_dst-b_src == in_elem_flipped_offset) begin
            data_low[b_dst*8 +: 8] = data_in[b_src*8 +: 8];
          end
        end
      end
    end

    if (is_slide_up) begin
      if (vreg_counter_en) shift_overflow_d = data_low;
      data_out = data_high | shift_overflow_q;
    end else begin
      if (vreg_counter_en) shift_overflow_d = data_high;
      data_out = data_low | shift_overflow_q;

      if (spatz_req_q.op_sld.one_up_down && vreg_operation_last) begin
        data_out = data_out | spatz_req_q.rs1 << 8*(vreg_counter_delta-(3'b001<<spatz_req_q.vtype.vsew));
      end
    end

    // If we have a slide up operation, flip all bytes back around (d[i] = d[-i])
    if (is_slide_up) begin
      for (int b_src = 0; b_src < VELEB; b_src++) begin
        vrf_wdata_o[(VELEB-b_src-1)*8 +: 8] = data_out[b_src*8 +: 8];
      end

      if (spatz_req_q.op_sld.one_up_down && vreg_operation_first) begin
        vrf_wdata_o = vrf_wdata_o | spatz_req_q.rs1 << 8*spatz_req_q.vstart;
      end
    end else begin
      vrf_wdata_o = data_out;
    end

    for (int i = 0; i < VELEB; i++) begin
      vrf_wbe_o[i] = i < vreg_counter_delta;
    end

    if (vreg_operation_first) begin
      for (int i = 0; i < VELEB; i++) begin
        vrf_wbe_o[i] = (i >= vreg_counter_value[$clog2(VELEB)-1:0]) & (i < (vreg_counter_value[$clog2(VELEB)-1:0] + vreg_counter_delta));
      end
    end

    if (vreg_operations_finished) shift_overflow_d = '0;
  end

  // VRF signals
  assign vrf_re_o  = vreg_operation_valid;
  assign vrf_we_o  = vrf_re_o & vrf_rvalid_i;

endmodule : spatz_vsldu
