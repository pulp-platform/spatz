// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The vector slide unit executes all slide instructions

module spatz_vsldu import spatz_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width; (
    input logic clk_i,
    input logic rst_ni,

    // Spatz req
    input  spatz_req_t spatz_req_i,
    input  logic       spatz_req_valid_i,
    output logic       spatz_req_ready_o,

    // VSLDU rsp
    output logic       vsldu_rsp_valid_o,
    output vsldu_rsp_t vsldu_rsp_o,

    // VRF
    output vreg_addr_t       vrf_waddr_o,
    output vreg_data_t       vrf_wdata_o,
    output logic             vrf_we_o,
    output vreg_be_t         vrf_wbe_o,
    input  logic             vrf_wvalid_i,
    output spatz_id_t  [1:0] vrf_id_o,
    output vreg_addr_t       vrf_raddr_o,
    output logic             vrf_re_o,
    input  vreg_data_t       vrf_rdata_i,
    input  logic             vrf_rvalid_i
  );

// Include FF
`include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req_d, spatz_req_q;
  `FF(spatz_req_q, spatz_req_d, '0)

  // Is the register file operation valid and are we at the last one
  logic vreg_operation_valid;
  logic vreg_operation_first;
  logic vreg_operation_last;
  logic vreg_operations_finished;

  // Is the VSLDU ready?
  logic vsldu_is_ready;
  assign vsldu_is_ready = vreg_operations_finished;

  // Has a new vsldu execution request arrived
  logic new_vsldu_request;
  assign new_vsldu_request = spatz_req_valid_i & vsldu_is_ready & (spatz_req_i.ex_unit == SLD);

  // Is the vector length zero (no active instruction)
  logic is_vl_zero;
  assign is_vl_zero = spatz_req_q.vl == 'd0;

  // Is the instruction slide up
  logic is_slide_up;
  assign is_slide_up = spatz_req_q.op == VSLIDEUP;

  // Number of bytes we slide up or down
  vlen_t slide_amount_q, slide_amount_d;
  `FF(slide_amount_q, slide_amount_d, '0)

  // Are we doing a vregfile read prefetch (when we slide down)
  logic prefetch_q, prefetch_d;
  `FF(prefetch_q, prefetch_d, 1'b0);

  // Signal to the controller if we are ready for a new instruction or not
  assign spatz_req_ready_o = vsldu_is_ready;

  // Vector register file counter signals
  logic  vreg_counter_en;
  logic  vreg_counter_load;
  vlen_t vreg_counter_delta;
  vlen_t vreg_counter_d;
  vlen_t vreg_counter_q;

  delta_counter #(
    .WIDTH($bits(vlen_t))
  ) i_delta_counter_vreg (
    .clk_i     (clk_i             ),
    .rst_ni    (rst_ni            ),
    .clear_i   (1'b0              ),
    .en_i      (vreg_counter_en   ),
    .load_i    (vreg_counter_load ),
    .down_i    (1'b0              ), // We always count up
    .delta_i   (vreg_counter_delta),
    .d_i       (vreg_counter_d    ),
    .q_o       (vreg_counter_q    ),
    .overflow_o(/* Unused */      )
  );

  ///////////////////////
  //  Output Register  //
  ///////////////////////

  typedef struct packed {
    vreg_addr_t waddr;
    vreg_data_t wdata;
    vreg_be_t wbe;
  } vrf_req_t;

  vrf_req_t vrf_req_d, vrf_req_q;
  logic     vrf_req_valid_d, vrf_req_ready_d;
  logic     vrf_req_valid_q, vrf_req_ready_q;

  spill_register #(
    .T(vrf_req_t)
  ) i_vrf_req_register (
    .clk_i  (clk_i          ),
    .rst_ni (rst_ni         ),
    .data_i (vrf_req_d      ),
    .valid_i(vrf_req_valid_d),
    .ready_o(vrf_req_ready_d),
    .data_o (vrf_req_q      ),
    .valid_o(vrf_req_valid_q),
    .ready_i(vrf_req_ready_q)
  );

  assign vrf_waddr_o     = vrf_req_q.waddr;
  assign vrf_wdata_o     = vrf_req_q.wdata;
  assign vrf_wbe_o       = vrf_req_q.wbe;
  assign vrf_we_o        = vrf_req_valid_q;
  assign vrf_id_o        = {2{spatz_req_q.id}};
  assign vrf_req_ready_q = vrf_wvalid_i;

  ///////////////////
  // State Handler //
  ///////////////////

  // Accept a new operation or clear req register if we are finished
  always_comb begin
    spatz_req_d    = spatz_req_q;
    slide_amount_d = slide_amount_q;
    prefetch_d     = prefetch_q;

    if (new_vsldu_request) begin
      spatz_req_d    = spatz_req_i;
      slide_amount_d = spatz_req_i.op_sld.insert ? (spatz_req_i.op_sld.vmv ? 'd0 : 'd1) : spatz_req_i.rs1;

      // Convert vl/vstart/slide_amount to number of bytes for all element widths
      if (spatz_req_i.vtype.vsew == EW_8) begin
        if (spatz_req_i.op_sld.vmv && spatz_req_d.op_sld.insert)
          spatz_req_d.rs1 = {4{spatz_req_i.rs1[7:0]}};
      end else if (spatz_req_i.vtype.vsew == EW_16) begin
        spatz_req_d.vl     = spatz_req_i.vl << 1;
        spatz_req_d.vstart = spatz_req_i.vstart << 1;
        slide_amount_d     = slide_amount_d << 1;
        if (spatz_req_i.op_sld.vmv && spatz_req_d.op_sld.insert)
          spatz_req_d.rs1 = {2{spatz_req_i.rs1[15:0]}};
      end else if (spatz_req_i.vtype.vsew == EW_32) begin
        spatz_req_d.vl     = spatz_req_i.vl << 2;
        spatz_req_d.vstart = spatz_req_i.vstart << 2;
        slide_amount_d     = slide_amount_d << 2;
      end

      prefetch_d = spatz_req_i.op == VSLIDEUP ? spatz_req_d.vstart >= VRFWordBWidth : 1'b1;
    end else if (!is_vl_zero && vsldu_is_ready && !new_vsldu_request &&
        !vrf_req_valid_d && (!vrf_req_valid_q || (vrf_req_valid_q && vrf_req_ready_q))) begin
      // If we are ready for a new instruction but there is none, clear req register
      spatz_req_d = '0;
    end

    if (prefetch_q && vrf_re_o && vrf_rvalid_i) prefetch_d = 1'b0;
  end

  // Respond to controller if we are finished executing
  logic op_finished_q;
  `FF(op_finished_q, (!is_vl_zero && |vreg_operations_finished && !vrf_req_valid_d && (!vrf_we_o || vrf_wvalid_i)), 1'b0)

  spatz_id_t op_id_q;
  `FF(op_id_q, spatz_req_q.id, '0)

  always_comb begin : vsldu_rsp
    vsldu_rsp_valid_o = 1'b0;
    vsldu_rsp_o       = '0;

    // Write back accessed register file vector to clear scoreboard entry
    if (op_finished_q) begin
      vsldu_rsp_o.id    = op_id_q;
      vsldu_rsp_valid_o = 1'b1;
    end
  end

  always_comb begin
    // How many elements are left to do
    automatic int unsigned delta = spatz_req_q.vl - vreg_counter_q;

    vreg_counter_d = spatz_req_d.vstart;
    if (!spatz_req_d.op_sld.insert && spatz_req_d.vstart < slide_amount_d && spatz_req_i.op == VSLIDEUP && new_vsldu_request) begin
      vreg_counter_d = slide_amount_d;
    end else if (!spatz_req_q.op_sld.insert && spatz_req_q.vstart < slide_amount_q && is_slide_up && ~new_vsldu_request) begin
      vreg_counter_d = slide_amount_q;
    end else if (~new_vsldu_request) begin
      vreg_counter_d = spatz_req_q.vstart;
    end
    vreg_counter_load = new_vsldu_request;

    vreg_operation_valid     = (delta != 'd0) & ~is_vl_zero;
    vreg_operation_first     = vreg_operation_valid & ~prefetch_q & vreg_counter_q == vreg_counter_d;
    vreg_operation_last      = vreg_operation_valid & ~prefetch_q & (delta <= (VRFWordBWidth - vreg_counter_q[$clog2(VRFWordBWidth)-1:0]));
    vreg_counter_delta       = ~vreg_operation_valid ? 'd0 : vreg_operation_last ? delta : vreg_operation_first ? VRFWordBWidth - vreg_counter_d[$clog2(VRFWordBWidth)-1:0] : VRFWordBWidth;
    vreg_counter_en          = ((spatz_req_q.use_vs2 & vrf_re_o & vrf_rvalid_i) | ~spatz_req_q.use_vs2) & ((spatz_req_q.use_vd & vrf_req_valid_d & vrf_req_ready_d) | ~spatz_req_q.use_vd);
    vreg_operations_finished = ~vreg_operation_valid | (vreg_operation_last & vreg_counter_en);
  end

  ////////////////////////
  // Address Generation //
  ////////////////////////

  vlen_t sld_offset_rd;

  always_comb begin
    sld_offset_rd   = is_slide_up ? (prefetch_q ? -slide_amount_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)] - 1 : -slide_amount_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)]) : prefetch_q ? slide_amount_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)] : slide_amount_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)] + 1;
    vrf_raddr_o     = {spatz_req_q.vs2, $clog2(NrWordsPerVector)'(1'b0)} + vreg_counter_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)] + sld_offset_rd;
    vrf_req_d.waddr = {spatz_req_q.vd, $clog2(NrWordsPerVector)'(1'b0)} + vreg_counter_q[$bits(vlen_t)-1:$clog2(VRFWordBWidth)];
  end

  ////////////
  // Slider //
  ////////////

  // Shift overflow register
  vreg_data_t shift_overflow_q, shift_overflow_d;
  `FF(shift_overflow_q, shift_overflow_d, '0)

  // Number of bytes we have to shift the elements around
  // inside the register element
  logic [$clog2(VRFWordBWidth)-1:0] in_elem_offset, in_elem_flipped_offset;
  assign in_elem_offset         = slide_amount_q[$clog2(VRFWordBWidth)-1:0];
  assign in_elem_flipped_offset = VRFWordBWidth - in_elem_offset;

  // Data signals for different stages of the shift
  vreg_data_t data_in, data_out, data_low, data_high;

  always_comb begin
    shift_overflow_d = shift_overflow_q;

    data_in   = '0;
    data_out  = '0;
    data_high = '0;
    data_low  = '0;

    vrf_req_d.wbe   = 'x;
    vrf_req_d.wdata = 'x;

    // Is there a vector instruction executing now?
    if (!is_vl_zero) begin
      if (is_slide_up && spatz_req_q.op_sld.insert && spatz_req_q.op_sld.vmv) begin
        for (int b_src = 0; b_src < VRFWordBWidth; b_src++)
          data_in[(VRFWordBWidth-b_src-1)*8 +: 8] = spatz_req_q.rs1[b_src*8%ELEN +: 8];
      end else if (is_slide_up) begin
        // If we have a slide up operation, flip all bytes around (d[-i] = d[i])
        for (int b_src = 0; b_src < VRFWordBWidth; b_src++)
          data_in[(VRFWordBWidth-b_src-1)*8 +: 8] = vrf_rdata_i[b_src*8 +: 8];
      end else begin
        data_in = vrf_rdata_i;

        // If we are already over the MAXVL, all continuing elements are zero
        if ((vreg_counter_q >= MAXVL - slide_amount_q) || (vreg_operation_last && spatz_req_q.op_sld.insert))
          data_in = '0;
      end

      // Shift direct elements into the correct position
      for (int b_src = 0; b_src < VRFWordBWidth; b_src++)
        if (b_src >= in_elem_offset) begin
          // high elements
          for (int b_dst = 0; b_dst <= b_src; b_dst++)
            if (b_src-b_dst == in_elem_offset)
              data_high[b_dst*8 +: 8] = data_in[b_src*8 +: 8];
        end else begin
          // low elements
          for (int b_dst = b_src; b_dst < VRFWordBWidth; b_dst++)
            if (b_dst-b_src == in_elem_flipped_offset)
              data_low[b_dst*8 +: 8] = data_in[b_src*8 +: 8];
        end

      // Combine overflow and direct elements together
      if (is_slide_up) begin
        if (vreg_counter_en || prefetch_q)
          shift_overflow_d = data_low;
        data_out = data_high | shift_overflow_q;
      end else begin
        if (vreg_counter_en || prefetch_q)
          shift_overflow_d = data_high;
        data_out = data_low | shift_overflow_q;

        // Insert rs1 element at the last position
        if (spatz_req_q.op_sld.insert && vreg_operation_last) begin
          for (int b = 0; b < VRFWordBWidth; b++)
            if (b >= (vreg_counter_q[$clog2(VRFWordBWidth)-1:0] + vreg_counter_delta - (3'b001<<spatz_req_q.vtype.vsew)))
              data_out[b*8 +: 8] = data_low[b*8 +: 8];
          data_out = data_out | (vreg_data_t'(spatz_req_q.rs1) << 8*(vreg_counter_q[$clog2(VRFWordBWidth)-1:0]+vreg_counter_delta-(3'b001<<spatz_req_q.vtype.vsew)));
        end
      end

      // If we have a slide up operation, flip all bytes back around (d[i] = d[-i])
      if (is_slide_up) begin
        for (int b_src = 0; b_src < VRFWordBWidth; b_src++)
          vrf_req_d.wdata[(VRFWordBWidth-b_src-1)*8 +: 8] = data_out[b_src*8 +: 8];

        // Insert rs1 element at the first position
        if (spatz_req_q.op_sld.insert && !spatz_req_q.op_sld.vmv && vreg_operation_first && spatz_req_q.vstart == 'd0)
          vrf_req_d.wdata = vrf_req_d.wdata | vreg_data_t'(spatz_req_q.rs1);
      end else begin
        vrf_req_d.wdata = data_out;
      end

      // Create byte enable mask
      for (int i = 0; i < VRFWordBWidth; i++)
        vrf_req_d.wbe[i] = i < vreg_counter_delta;

      // Special byte enable mask case when we are operating on the first register element.
      if (vreg_operation_first)
        for (int i = 0; i < VRFWordBWidth; i++)
          vrf_req_d.wbe[i] = (i >= vreg_counter_q[$clog2(VRFWordBWidth)-1:0]) & (i < (vreg_counter_q[$clog2(VRFWordBWidth)-1:0] + vreg_counter_delta));
    end

    // Reset overflow register when finished
    if (vreg_operations_finished) shift_overflow_d = '0;
  end

  // VRF signals
  assign vrf_re_o        = spatz_req_q.use_vs2 & (vreg_operation_valid | prefetch_q);
  assign vrf_req_valid_d = spatz_req_q.use_vd & vrf_re_o & vrf_rvalid_i & ~prefetch_q;

endmodule : spatz_vsldu
