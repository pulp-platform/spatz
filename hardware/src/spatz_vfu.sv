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
  // Spatz rsp
  // VRF
  output vreg_addr_t       vrf_waddr_o,
  output vreg_data_t       vrf_wdata_o,
  output logic             vrf_we_o,
  output vreg_be_t         vrf_wbe_o,
  input  logic             vrf_wvalid_i,
  output vreg_addr_t [2:0] vrf_raddr_o,
  input  logic       [2:0] vrf_re_o,
  input  vreg_data_t [2:0] vrf_rdata_i,
  input  logic       [2:0] vrf_rvalid_i
);

  logic vfu_is_ready;
  logic op1_is_ready, op2_is_ready, op3_is_ready;
  logic operands_ready = op1_is_ready && op2_is_ready && op3_is_ready;

  logic last_group;
  logic result_written;

  logic new_request = spatz_req_valid_i && vfu_is_ready && (spatz_req_i.ex_unit == VFU);

  logic [N_IPU*ELEN-1:0]  operand1, operand2, operand3;
  logic [N_IPU*ELENB-1:0] carry;
  logic [N_IPU*ELENB-1:0] result_be;
  logic [N_IPU*ELEN-1:0]  result;

  ///////////////
  // Spatz Req //
  ///////////////

  spatz_req_t spatz_req_d, spatz_req_q;

  always_ff @(posedge clk_i or negedge rst_ni) begin : proc_spatz_req
    if(~rst_ni) begin
      spatz_req_q <= 0;
    end else begin
      spatz_req_q <= spatz_req_d;
    end
  end

  assign vfu_is_ready = (spatz_req_q.vl == '0) || (last_group && operands_ready && result_written);
  assign spatz_req_ready_o = vfu_is_ready;

  ///////////////////
  // State Handler //
  ///////////////////

  always_comb begin : proc_state_handler
    spatz_req_d = spatz_req_q;
    last_group = 1'b0;

    if (new_request) begin
      spatz_req_d = spatz_req_i;
    end else if (spatz_req_q.vl != 0 && operands_ready && result_written) begin
      // Change number of remaining elements
      if (spatz_req_q.vtype.vsew == rvv_pkg::EW_8) begin
        spatz_req_d.vl >>= $clog2(N_IPU) + 2;
        last_group = spatz_req_d.vl <= $clog2(N_IPU) * 4;
      end else if (spatz_req_q.vtype.vsew == rvv_pkg::EW_16) begin
        spatz_req_d.vl >>= $clog2(N_IPU) + 1;
        last_group = spatz_req_d.vl <= $clog2(N_IPU) * 2;
      end else begin
        spatz_req_d.vl >>= $clog2(N_IPU);
        last_group = spatz_req_d.vl <= $clog2(N_IPU);
      end
    end
  end : proc_state_handler

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

  always_comb begin : proc_vreg_addr
    vreg_addr_d = vreg_addr_q;

    if (new_request && vfu_is_ready) begin
      vreg_addr_d[0] = {spatz_req_q.vs2, $clog2(VELE)'(0)};
      vreg_addr_d[1] = {spatz_req_q.vs1, $clog2(VELE)'(0)};
      vreg_addr_d[2] = {spatz_req_q.vd,  $clog2(VELE)'(0)};
    end else if (operands_ready && result_written) begin
      vreg_addr_d[0] = vreg_addr_q[0] + N_IPU;
      vreg_addr_d[1] = vreg_addr_q[1] + N_IPU;
      vreg_addr_d[2] = vreg_addr_q[2] + N_IPU;
    end
  end

  always_comb begin : proc_op_req
    vreg_r_req = '0;

    if (!vfu_is_ready || last_group) begin
      // Request operands
      vreg_r_req = {spatz_req_q.use_vs2, spatz_req_q.use_vs1, spatz_req_q.vd_is_src};

      // Distribute operands
      if (operands_ready) begin
        vreg_we  = spatz_req_q.use_vd;
        vreg_wbe = result_be;
      end
    end
  end : proc_op_req

  assign vrf_raddr_o = vreg_addr_q;
  assign vrf_re_o = vreg_r_req;
  assign vrf_we_o = vreg_we;
  assign vrf_wbe_o = vreg_wbe;
  assign vrf_waddr_o = vreg_addr_q;
  assign vrf_wdata_o = result;

  assign op1_is_ready = vrf_rvalid_i[1];
  assign op2_is_ready = vrf_rvalid_i[0];
  assign op3_is_ready = vrf_rvalid_i[2];
  assign operand1 = vrf_rdata_i[1];
  assign operand2 = vrf_rdata_i[0];
  assign operand3 = vrf_rdata_i[2];

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
