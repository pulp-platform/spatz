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
    output spatz_id_t  [3:0] vrf_id_o,
    output vreg_addr_t [2:0] vrf_raddr_o,
    output logic       [2:0] vrf_re_o,
    input  vreg_data_t [2:0] vrf_rdata_i,
    input  logic       [2:0] vrf_rvalid_i
  );

// Include FF
`include "common_cells/registers.svh"

  ///////////////////////
  //  Operation queue  //
  ///////////////////////

  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_ready;

  spill_register #(
    .T(spatz_req_t)
  ) i_operation_queue (
    .clk_i  (clk_i                                          ),
    .rst_ni (rst_ni                                         ),
    .data_i (spatz_req_i                                    ),
    .valid_i(spatz_req_valid_i && spatz_req_i.ex_unit == VFU),
    .ready_o(spatz_req_ready_o                              ),
    .data_o (spatz_req                                      ),
    .valid_o(spatz_req_valid                                ),
    .ready_i(spatz_req_ready                                )
  );

  ///////////////
  //  Control  //
  ///////////////

  // Vector length counter
  vlen_t vl_q, vl_d;
  `FF(vl_q, vl_d, '0)

  // Are we busy?
  logic busy_q, busy_d;
  `FF(busy_q, busy_d, 1'b0)

  // Number of words advanced by vstart
  vlen_t vstart;
  assign vstart = ((spatz_req.vstart / N_IPU) >> (EW_32 - spatz_req.vtype.vsew)) << (EW_32 - spatz_req.vtype.vsew);

  // Are the VFU operands ready?
  logic op1_is_ready, op2_is_ready, op3_is_ready, operands_ready;
  assign op1_is_ready   = spatz_req_valid && (!spatz_req.use_vs1 || vrf_rvalid_i[1]);
  assign op2_is_ready   = spatz_req_valid && (!spatz_req.use_vs2 || vrf_rvalid_i[0]);
  assign op3_is_ready   = spatz_req_valid && (!spatz_req.vd_is_src || vrf_rvalid_i[2]);
  assign operands_ready = op1_is_ready && op2_is_ready && op3_is_ready;

  // Did we write a result back to the VRF?
  logic result_written;
  assign result_written = spatz_req_valid && (!spatz_req.use_vd || vrf_wvalid_i);

  // Did we commit a word to the VRF?
  logic word_committed;
  assign word_committed = operands_ready && result_written;

  // Number of elements in one VRF word
  logic [$clog2(N_IPU*4):0] nr_elem_word;
  assign nr_elem_word = N_IPU * (1 << (EW_32 - spatz_req.vtype.vsew));

  // Did we reach the last elements of the instruction?
  logic last_word;
  assign last_word = spatz_req.vl <= vlen_t'(nr_elem_word);

  always_comb begin: control_proc
    // Maintain state
    vl_d   = vl_q;
    busy_d = busy_q;

    // We are handling an instruction
    spatz_req_ready = 1'b0;

    // Do not ack anything
    vfu_rsp_valid_o = 1'b0;
    vfu_rsp_o       = '0;

    // Change number of remaining elements
    if (word_committed)
      vl_d = vl_q + nr_elem_word;

    // Finished the execution!
    if (spatz_req_valid && vl_d >= spatz_req.vl) begin
      spatz_req_ready = spatz_req_valid;
      busy_d          = 1'b0;
      vl_d            = '0;

      vfu_rsp_o.id    = spatz_req.id;
      vfu_rsp_o.vs2   = spatz_req.vs2;
      vfu_rsp_o.vs1   = spatz_req.vs1;
      vfu_rsp_o.vd    = spatz_req.vd;
      vfu_rsp_valid_o = 1'b1;
    end
    // Do we have a new instruction?
    else if (spatz_req_valid && !busy_d) begin
      // Start at vstart
      vl_d   = vstart;
      busy_d = 1'b1;
    end
  end: control_proc

  //////////////
  // Operands //
  //////////////

  // IPU operands and result signals
  logic [N_IPU*ELEN-1:0] operand1, operand2, operand3;
  logic [N_IPU*ELEN-1:0] result;
  always_comb begin: operand_proc
    if (spatz_req.use_vs1)
      operand1 = vrf_rdata_i[1];
    else begin
      // Replicate scalar operands
      unique case (spatz_req.vtype.vsew)
        EW_8 :   operand1 = {4*N_IPU{spatz_req.rs1[7:0]}};
        EW_16:   operand1 = {2*N_IPU{spatz_req.rs1[15:0]}};
        default: operand1 = {1*N_IPU{spatz_req.rs1}};
      endcase
    end
    operand2 = vrf_rdata_i[0];
    operand3 = vrf_rdata_i[2];
  end: operand_proc

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
  always_comb begin : vreg_addr_proc
    vreg_addr_d = vreg_addr_q;

    if (spatz_req_valid && !busy_q) begin
      vreg_addr_d[0] = (spatz_req.vs2 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[1] = (spatz_req.vs1 + vstart) << $clog2(NrWordsPerVector);
      vreg_addr_d[2] = (spatz_req.vd + vstart) << $clog2(NrWordsPerVector);
    end else if (spatz_req_valid && vl_q < spatz_req.vl && word_committed) begin
      vreg_addr_d[0] = vreg_addr_q[0] + 1;
      vreg_addr_d[1] = vreg_addr_q[1] + 1;
      vreg_addr_d[2] = vreg_addr_q[2] + 1;
    end else if (vl_q >= spatz_req.vl || (last_word && word_committed)) begin
      vreg_addr_d = '0;
    end
  end: vreg_addr_proc

  always_comb begin : operand_req_proc
    vreg_r_req = '0;
    vreg_we    = '0;
    vreg_wbe   = '0;

    if (spatz_req_valid && vl_q < spatz_req.vl) begin
      // Request operands
      vreg_r_req = {spatz_req.vd_is_src, spatz_req.use_vs1, spatz_req.use_vs2};

      // Distribute operands and write result back to register file
      if (operands_ready) begin
        vreg_we  = spatz_req.use_vd;
        vreg_wbe = '1;

        // If we are in the last group or at the start and vstart is nonzero,
        // create the byte enable (be) mask for write back to register file.
        if (spatz_req.vstart != '0 && busy_q == 1'b0)
          unique case (spatz_req.vtype.vsew)
            EW_8 :   vreg_wbe = ~(vreg_be_t'('1) >> (nr_elem_word - spatz_req.vstart[idx_width(N_IPU * 4)-1:0]));
            EW_16:   vreg_wbe = ~(vreg_be_t'('1) >> (nr_elem_word - spatz_req.vstart[idx_width(N_IPU * 2)-1:0]));
            default: vreg_wbe = ~(vreg_be_t'('1) >> (nr_elem_word - spatz_req.vstart[idx_width(N_IPU * 1)-1:0]));
          endcase
        else
          if (last_word)
            unique case (spatz_req.vtype.vsew)
              EW_8 :   vreg_wbe = vreg_be_t'('1) >> vl_q[idx_width(N_IPU * 4):0];
              EW_16:   vreg_wbe = vreg_be_t'('1) >> vl_q[idx_width(N_IPU * 2):0];
              default: vreg_wbe = vreg_be_t'('1) >> vl_q[idx_width(N_IPU * 1):0];
            endcase
      end
    end
  end : operand_req_proc

  // Register file signals
  assign vrf_raddr_o = vreg_addr_q;
  assign vrf_re_o    = vreg_r_req;
  assign vrf_we_o    = vreg_we;
  assign vrf_wbe_o   = vreg_wbe;
  assign vrf_waddr_o = vreg_addr_q[2];
  assign vrf_wdata_o = result;
  assign vrf_id_o    = {4{spatz_req.id}};

  //////////
  // IPUs //
  //////////

  for (genvar ipu = 0; unsigned'(ipu) < N_IPU; ipu++) begin : gen_ipus
    spatz_ipu i_ipu (
      .clk_i            (clk_i                            ),
      .rst_ni           (rst_ni                           ),
      .operation_i      (spatz_req.op                     ),
      .operation_valid_i(spatz_req_valid && operands_ready), // If the VFU is not ready, it is executing something
      .op_s1_i          (operand1[ipu*ELEN +: ELEN]       ),
      .op_s2_i          (operand2[ipu*ELEN +: ELEN]       ),
      .op_d_i           (operand3[ipu*ELEN +: ELEN]       ),
      .carry_i          ('0                               ),
      .sew_i            (spatz_req.vtype.vsew             ),
      .be_o             (/* Unused */                     ),
      .result_o         (result[ipu*ELEN +: ELEN]         )
    );
  end : gen_ipus

endmodule : spatz_vfu
