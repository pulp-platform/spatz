// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The Vector Functional Unit (VFU) executes all arithmetic and logical
// vector instructions. It can be configured with a parameterizable amount
// of IPUs that work in parallel.

module spatz_vfu import spatz_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width; (
    input  logic             clk_i,
    input  logic             rst_ni,
    // Spatz req
    input  spatz_req_t       spatz_req_i,
    input  logic             spatz_req_valid_i,
    output logic             spatz_req_ready_o,
    // VFU rsp
    output logic             vfu_rsp_valid_o,
    output vfu_rsp_t         vfu_rsp_o,
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

  // Are the VFU operands ready?
  logic op1_is_ready, op2_is_ready, op3_is_ready, operands_ready;
  assign op1_is_ready   = !spatz_req_q.use_vs1 || vrf_rvalid_i[1];
  assign op2_is_ready   = !spatz_req_q.use_vs2 || vrf_rvalid_i[0];
  assign op3_is_ready   = !spatz_req_q.vd_is_src || vrf_rvalid_i[2];
  assign operands_ready = op1_is_ready && op2_is_ready && op3_is_ready;

  // Did we write a result back to the VRF?
  logic result_written;
  assign result_written = !spatz_req_q.use_vd || vrf_wvalid_i;

  // Is vl and vstart zero?
  logic vl_is_zero;
  logic vstart_is_zero;
  assign vl_is_zero     = spatz_req_q.vl == 0;
  assign vstart_is_zero = spatz_req_q.vstart == 0;

  // Number of elements in one VRF word
  logic [$clog2(N_IPU*4):0] nr_elem_word;
  assign nr_elem_word = N_IPU * (EW_32 - spatz_req_q.vtype.vsew);

  // Did we reach the last elements of the instruction?
  logic last_word;
  assign last_word = spatz_req_q.vl <= nr_elem_word;
  // Did we commit a word to the VRF?
  logic word_committed;
  assign word_committed = operands_ready && result_written;

  // Is the VFU ready for a new request from the controller?
  logic vfu_is_ready;
  assign vfu_is_ready      = vl_is_zero || (last_word && word_committed);
  assign spatz_req_ready_o = vfu_is_ready;

  // Did a new VFU execution request arrive?
  logic new_request;
  assign new_request = spatz_req_valid_i && vfu_is_ready && (spatz_req_i.ex_unit == VFU);

  // IPU operands and result signals
  logic [N_IPU*ELEN-1:0] operand1, operand2, operand3;
  logic [N_IPU*ELEN-1:0] result;
  assign operand1 = spatz_req_q.use_vs1 ? vrf_rdata_i[1] :
                    spatz_req_q.vtype.vsew == EW_8 ? {4*N_IPU{spatz_req_q.rs1[7:0]}}   :
                    spatz_req_q.vtype.vsew == EW_16 ? {2*N_IPU{spatz_req_q.rs1[15:0]}} :
                                                      {1*N_IPU{spatz_req_q.rs1}};
  assign operand2 = vrf_rdata_i[0];
  assign operand3 = vrf_rdata_i[2];

  // Number of words advanced by vstart
  vlen_t vstart_nwords;
  assign vstart_nwords = vlen_t'(((spatz_req_i.vstart / N_IPU) >> (EW_32 - spatz_req_i.vtype.vsew)) << (EW_32 - spatz_req_i.vtype.vsew));

  ///////////////////
  // State Handler //
  ///////////////////

  always_comb begin : proc_state_handler
    // Maintain state
    spatz_req_d = spatz_req_q;

    if (new_request) begin
      spatz_req_d    = spatz_req_i;
      // Decrement vl by vstart
      spatz_req_d.vl = spatz_req_i.vl - vstart_nwords;
    end else if (!vl_is_zero && word_committed) begin
      // Change number of remaining elements
      spatz_req_d.vl     = last_word ? '0 : spatz_req_q.vl - nr_elem_word;
      spatz_req_d.vstart = '0;
    end else if (vl_is_zero) begin
      spatz_req_d = '0;
    end
  end : proc_state_handler

  // Respond to controller when we are finished executing
  always_comb begin : proc_vfu_rsp
    vfu_rsp_valid_o = 1'b0;
    vfu_rsp_o       = '0;

    if (last_word && word_committed) begin
      vfu_rsp_o.id    = spatz_req_q.id;
      vfu_rsp_o.vs2   = spatz_req_q.vs2;
      vfu_rsp_o.vs1   = spatz_req_q.vs1;
      vfu_rsp_o.vd    = spatz_req_q.vd;
      vfu_rsp_valid_o = 1'b1;
    end
  end

  ///////////////////////
  // Operand Requester //
  ///////////////////////

  vreg_be_t       vreg_wbe;
  logic           vreg_we;
  logic     [2:0] vreg_r_req;

  // Address register
  vreg_addr_t [2:0] vreg_addr_q, vreg_addr_d;
  `FF(vreg_addr_q, vreg_addr_d, '0)

  // Calculate new vector register address
  always_comb begin : proc_vreg_addr
    vreg_addr_d = vreg_addr_q;

    if (new_request) begin
      vreg_addr_d[0] = (spatz_req_i.vs2 + vstart_nwords) << $clog2(NrWordsPerVector);
      vreg_addr_d[1] = (spatz_req_i.vs1 + vstart_nwords) << $clog2(NrWordsPerVector);
      vreg_addr_d[2] = (spatz_req_i.vd + vstart_nwords) << $clog2(NrWordsPerVector);
    end else if (!vl_is_zero && !last_word && word_committed) begin
      vreg_addr_d[0] = vreg_addr_q[0] + 1;
      vreg_addr_d[1] = vreg_addr_q[1] + 1;
      vreg_addr_d[2] = vreg_addr_q[2] + 1;
    end else if (vl_is_zero || (last_word && word_committed)) begin
      vreg_addr_d = '0;
    end
  end

  always_comb begin : proc_op_req
    automatic vlen_t vend       = spatz_req_q.vtype.vsew == EW_8 ? spatz_req_q.vl[idx_width(N_IPU * 4):0]       : spatz_req_q.vtype.vsew == EW_16 ? spatz_req_q.vl[idx_width(N_IPU * 2):0]       : spatz_req_q.vl[idx_width(N_IPU):0];
    automatic vlen_t vstart     = spatz_req_q.vtype.vsew == EW_8 ? spatz_req_q.vstart[idx_width(N_IPU * 4)-1:0] : spatz_req_q.vtype.vsew == EW_16 ? spatz_req_q.vstart[idx_width(N_IPU * 2)-1:0] : spatz_req_q.vstart[idx_width(N_IPU)-1:0];
    automatic vlen_t subtrahend = !vstart_is_zero ? vstart                                                      : vend;
    automatic vlen_t shift      = nr_elem_word - subtrahend;

    vreg_r_req = '0;
    vreg_we    = '0;
    vreg_wbe   = '0;

    if (!vl_is_zero) begin
      // Request operands
      vreg_r_req = {spatz_req_q.vd_is_src, spatz_req_q.use_vs1, spatz_req_q.use_vs2};

      // Distribute operands and write result back to register file
      if (operands_ready) begin
        vreg_we  = spatz_req_q.use_vd;
        vreg_wbe = '1;

        // If we are in the last group or at the start and vstart is nonzero,
        // create the byte enable (be) mask for write back to register file.
        if (last_word || !vstart_is_zero) begin
          vreg_wbe = vreg_be_t'('1) >> shift;
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

  //////////
  // IPUs //
  //////////

  for (genvar ipu = 0; unsigned'(ipu) < N_IPU; ipu++) begin : gen_ipus
    spatz_ipu i_ipu (
      .clk_i            (clk_i                     ),
      .rst_ni           (rst_ni                    ),
      .operation_i      (spatz_req_q.op            ),
      .operation_valid_i(!vfu_is_ready             ), // If the VFU is not ready, it is executing something
      .op_s1_i          (operand1[ipu*ELEN +: ELEN]),
      .op_s2_i          (operand2[ipu*ELEN +: ELEN]),
      .op_d_i           (operand3[ipu*ELEN +: ELEN]),
      .carry_i          ('0                        ),
      .sew_i            (spatz_req_q.vtype.vsew    ),
      .be_o             (/* Unused */              ),
      .result_o         (result[ipu*ELEN +: ELEN]  )
    );
  end : gen_ipus

endmodule : spatz_vfu
