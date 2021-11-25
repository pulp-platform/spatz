// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vfu import spatz_pkg::*; (
  input  logic clk_i,
  input  logic rst_ni,
  // Spatz req
  input  spatz_req_t spatz_req_i,
  input  logic       spatz_req_valid_i,
  output logic       spatz_req_ready_o,
  // VFU rsp
  output logic     vfu_rsp_valid_o,
  output vfu_rsp_t vfu_rsp_o,
  // VRF
  output vreg_addr_t       vrf_waddr_o,
  output vreg_data_t       vrf_wdata_o,
  output logic             vrf_we_o,
  output vreg_be_t         vrf_wbe_o,
  input  logic             vrf_wvalid_i,
  output vreg_addr_t [2:0] vrf_raddr_o,
  output logic       [2:0] vrf_re_o,
  input  vreg_data_t [2:0] vrf_rdata_i,
  input  logic       [2:0] vrf_rvalid_i
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
  logic vfu_is_ready;
  logic op1_is_ready, op2_is_ready, op3_is_ready;
  logic operands_ready;
  assign operands_ready = op1_is_ready && op2_is_ready && op3_is_ready;

  // Has a new vfu execution request arrived
  logic new_request;
  assign new_request = spatz_req_valid_i && vfu_is_ready && (spatz_req_i.ex_unit == VFU);

  logic [N_IPU*ELEN-1:0]  operand1, operand2, operand3;
  logic [N_IPU*ELENB-1:0] carry;
  logic [N_IPU*ELENB-1:0] result_be;
  logic [N_IPU*ELEN-1:0]  result;

  // Number of elements in one group
  logic [$clog2(N_IPU*4):0] n_group_elem;
  assign n_group_elem = spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ? N_IPU*4 :
                        spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? N_IPU*2 : N_IPU;

  // Has the calculated result been written back to the vrf
  logic result_written;
  // Have we reached the last group to calculate
  logic last_group;
  assign last_group = spatz_req_q.vl <= n_group_elem;
  logic group_commited;
  assign group_commited = operands_ready && result_written;

  // Is vl and vstart zero
  logic  vl_is_zero;
  assign vl_is_zero = spatz_req_q.vl == 0;
  logic  vstart_is_zero;
  assign vstart_is_zero = spatz_req_q.vstart == 0;

  // Number of groups advanced by vstart
  vlen_t vstart_ngropus;
  assign vstart_ngropus = spatz_req_i.vtype.vsew == rvv_pkg::EW_8  ? vlen_t'( spatz_req_i.vstart[$size(vlen_t)-1:$clog2(N_IPU * 4)])         :
                          spatz_req_i.vtype.vsew == rvv_pkg::EW_16 ? vlen_t'({spatz_req_i.vstart[$size(vlen_t)-1:$clog2(N_IPU * 2)], 1'b0 }) :
                                                                     vlen_t'({spatz_req_i.vstart[$size(vlen_t)-1:$clog2(N_IPU)],     2'b00});

  ///////////////////
  // State Handler //
  ///////////////////

  always_comb begin : proc_state_handler
    spatz_req_d = spatz_req_q;

    if (new_request) begin
      spatz_req_d = spatz_req_i;
      // Decrement vl if vstart is nonzero
      if (spatz_req_i.vstart != 0) spatz_req_d.vl = spatz_req_i.vl - vstart_ngropus;
    end else if (!vl_is_zero && group_commited) begin
      // Change number of remaining elements
      spatz_req_d.vl = last_group ? 0 : spatz_req_q.vl - n_group_elem;
      if (!vstart_is_zero) spatz_req_d.vstart = 0;
    end else if (vl_is_zero) begin
      spatz_req_d = '0;
    end
  end : proc_state_handler

  // Respond to controller if we are finished executing
  always_comb begin : proc_vfu_rsp
    vfu_rsp_valid_o = 1'b0;
    vfu_rsp_o       = '0;

    if (last_group && group_commited) begin
      vfu_rsp_o.id    = spatz_req_q.id;
      vfu_rsp_valid_o = 1'b1;
    end
  end

  ///////////////////////
  // Operand Requester //
  ///////////////////////

  vreg_addr_t [2:0] vreg_addr_q, vreg_addr_d;
  vreg_be_t         vreg_wbe;
  logic             vreg_we;
  logic [2:0]       vreg_r_req;

  always_ff @(posedge clk_i or negedge rst_ni) begin : proc_vreg_addr_q
    if(~rst_ni) begin
      vreg_addr_q <= 0;
    end else begin
      vreg_addr_q <= vreg_addr_d;
    end
  end

  // Calculate new vector register address
  always_comb begin : proc_vreg_addr
    vreg_addr_d = vreg_addr_q;

    if (new_request) begin
      vreg_addr_d[0] = {spatz_req_i.vs2 + vstart_ngropus, $clog2(VELE)'(0)};
      vreg_addr_d[1] = {spatz_req_i.vs1 + vstart_ngropus, $clog2(VELE)'(0)};
      vreg_addr_d[2] = {spatz_req_i.vd  + vstart_ngropus, $clog2(VELE)'(0)};
    end else if (!vl_is_zero && !last_group && group_commited) begin
      vreg_addr_d[0] = vreg_addr_q[0] + 1;
      vreg_addr_d[1] = vreg_addr_q[1] + 1;
      vreg_addr_d[2] = vreg_addr_q[2] + 1;
    end else if (vl_is_zero || (last_group && group_commited)) begin
      vreg_addr_d = '0;
    end
  end

  always_comb begin : proc_op_req
    vreg_r_req = '0;
    vreg_we = '0;
    vreg_wbe = '0;

    if (!vl_is_zero) begin
      // Request operands
      vreg_r_req = {spatz_req_q.vd_is_src, spatz_req_q.use_vs1, spatz_req_q.use_vs2};

      // Distribute operands and write result back to register file
      if (operands_ready) begin
        vreg_we  = spatz_req_q.use_vd;
        vreg_wbe = '1;

        // If we are in the last group or at the start and vstart is nonzero,
        // create the byte enable (be) mask for write back to register file.
        if (last_group || !vstart_is_zero) begin
          automatic logic [N_IPU*4-1:0] base_mask = '1;
          automatic int vend = spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ? spatz_req_q.vl[$clog2(N_IPU * 4):0] :
                               spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? spatz_req_q.vl[$clog2(N_IPU * 2):0] :
                                                                          spatz_req_q.vl[$clog2(N_IPU):0];
          automatic int vstart = spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ?         spatz_req_q.vstart[$clog2(N_IPU * 4)-1:0]  :
                                 spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? {1'b0,  spatz_req_q.vstart[$clog2(N_IPU * 2)-1:0]} :
                                                                            {2'b00, spatz_req_q.vstart[$clog2(N_IPU)-1:0]};
          automatic int subtrahend = !vstart_is_zero ? vstart : vend;
          automatic int shift = n_group_elem - subtrahend;
          vreg_wbe = base_mask >> shift;
          vreg_wbe = !vstart_is_zero ? ~vreg_wbe : vreg_wbe;
        end
      end
    end
  end : proc_op_req

  // Register file signals
  assign vrf_raddr_o = vreg_addr_q;
  assign vrf_re_o    = vreg_r_req;
  assign vrf_we_o    = vreg_we;
  assign vrf_wbe_o   = vreg_wbe;
  assign vrf_waddr_o = vreg_addr_q[2];
  assign vrf_wdata_o = result;

  // Operand signals
  assign op1_is_ready = spatz_req_q.use_vs1   ? vrf_rvalid_i[1] : 1'b1;
  assign op2_is_ready = spatz_req_q.use_vs2   ? vrf_rvalid_i[0] : 1'b1;
  assign op3_is_ready = spatz_req_q.vd_is_src ? vrf_rvalid_i[2] : 1'b1;
  assign operand1 = spatz_req_q.use_vs1                      ? vrf_rdata_i[1] :
                    spatz_req_q.vtype.vsew == rvv_pkg::EW_8  ? {4*N_IPU{spatz_req_q.rs1[7:0]}} :
                    spatz_req_q.vtype.vsew == rvv_pkg::EW_16 ? {2*N_IPU{spatz_req_q.rs1[15:0]}} :
                                                               {N_IPU{spatz_req_q.rs1}};
  assign operand2 = vrf_rdata_i[0];
  assign operand3 = vrf_rdata_i[2];
  assign result_written = spatz_req_q.use_vd ? vrf_wvalid_i : 1'b1;

  // Is the vfu ready for a new request from the controller
  assign vfu_is_ready = vl_is_zero || (last_group && group_commited);
  assign spatz_req_ready_o = vfu_is_ready;

  //////////
  // IPUs //
  //////////

  for (genvar i = 0; unsigned'(i) < N_IPU; i++) begin : gen_ipus
    spatz_ipu i_ipu (
      .clk_i      (clk_i),
      .rst_ni     (rst_ni),
      .operation_i(spatz_req_q.op),
      .op_s1_i    (operand1[i*ELEN +: ELEN]),
      .op_s2_i    (operand2[i*ELEN +: ELEN]),
      .op_d_i     (operand3[i*ELEN +: ELEN]),
      .carry_i    ('0),
      .sew_i      (spatz_req_q.vtype.vsew),
      .be_o       (result_be[i*ELENB +: ELENB]),
      .result_o   (result[i*ELEN +: ELEN])
    );
  end : gen_ipus

endmodule : spatz_vfu
