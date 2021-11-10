// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_controller
  import spatz_pkg::*;
  import rvv_pkg::*;
(
  input  logic clk_i,
  input  logic rst_ni,
  // X-Interface Issue
  input  logic x_issue_valid_i,
  output logic x_issue_ready_o,
  input  core_v_xif_pkg::x_issue_req_t  x_issue_req_i,
  output core_v_xif_pkg::x_issue_resp_t x_issue_resp_o,
  // X-Interface Result
  output logic x_result_valid_o,
  input  logic x_result_ready_i,
  output core_v_xif_pkg::x_result_t x_result_o,
  // Decoder req
  output logic decoder_req_valid_o,
  output decoder_req_t decoder_req_o,
  // Decoder rsp
  input  logic decoder_rsp_valid_i,
  input  decoder_rsp_t decoder_rsp_i,
  // CSRs
  input  vlen_t  vstart_i,
  input  vlen_t  vl_i,
  input  vtype_t vtype_i,
  // Spatz req
  output logic spatz_req_valid_o,
  output spatz_req_t spatz_req_o
);

  logic spatz_ready = 1'b1;

  always_comb begin : proc_decoder
  decoder_req_o = '0;
  decoder_req_valid_o = 1'b0;

  // Decode new instruction if one is received and spatz is ready
  if (x_issue_valid_i & spatz_ready) begin
  decoder_req_o.instr = x_issue_req_i.instr;
  decoder_req_o.rs1   = x_issue_req_i.rs[0];
  decoder_req_o.rs2   = x_issue_req_i.rs[1];
  decoder_req_valid_o = 1'b1;
  end
  end // proc_decoder

  always_comb begin : proc_controller
    x_issue_resp_o = '0;
    x_result_o = '0;
    x_result_valid_o = '0;
    spatz_req_o = decoder_rsp_i.spatz_req;
    spatz_req_valid_o = 1'b0;

    // New decoded instruction if valid
    if (decoder_rsp_valid_i & ~decoder_rsp_i.instr_illegal) begin
    spatz_req_valid_o = 1'b1;
      case (decoder_rsp_i.spatz_req.op)
      // Read CSR and write back to cpu
      VCSR: begin
      if (decoder_rsp_i.spatz_req.use_rd) begin
      unique case (decoder_rsp_i.spatz_req.op_csr.addr)
      riscv_instr::CSR_VSTART: x_result_o.data = 32'(vstart_i);
      riscv_instr::CSR_VL:     x_result_o.data = 32'(vl_i);
      riscv_instr::CSR_VTYPE:  x_result_o.data = 32'(vtype_i);
      riscv_instr::CSR_VLENB:  x_result_o.data = 32'(VLENB);
      endcase
      end
      x_result_valid_o = 1'b1;
      x_result_o.we = 1'b1;
      x_issue_resp_o.writeback = 1'b1;
      end
      VCFG: begin
      x_result_valid_o = 1'b1;
      end
      endcase // Operation type
    // New instruction is illegal
    end else if (decoder_rsp_valid_i & decoder_rsp_i.instr_illegal) begin
      x_issue_resp_o.exc = 1'b1;
      x_result_o.exc = 1'b1;
    end
  end // proc_controller

  assign x_issue_ready_o = spatz_ready;

endmodule : spatz_controller
