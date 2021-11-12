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
  // Spatz req
  output logic spatz_req_valid_o,
  output spatz_req_t spatz_req_o
);

  // Include FF
  `include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Spatz request
  spatz_req_t spatz_req;
  logic       spatz_req_valid;

  //////////
  // CSRs //
  //////////

  // CSR registers
  vlen_t  vstart_d, vstart_q;
  vlen_t  vl_d, vl_q;
  vtype_t vtype_d, vtype_q;

  `FF(vstart_q, vstart_d, '0)
  `FF(vl_q, vl_d, '0)
  `FF(vtype_q, vtype_d, '{vill: 1'b1, default: '0})

  always_comb begin : proc_vcsr
    vstart_d = vstart_q;
    vl_d     = vl_q;
    vtype_d  = vtype_q;

    if (spatz_req_valid) begin

      // Reset vstart to zero
      if (decoder_rsp.spatz_req.op_cgf.reset_vstart) begin
        vstart_d = '0;
      end

      // Set new vstart when written over vcsrs
      if (decoder_rsp.spatz_req.op == VCSR) begin
        if (decoder_rsp.spatz_req.op_cgf.write_vstart) begin
          vstart_d = vlen_t'(decoder_rsp.spatz_req.rs1);
        end else if (decoder_rsp.spatz_req.op_cgf.set_vstart) begin
          vstart_d = vstart_q | vlen_t'(decoder_rsp.spatz_req.rs1);
        end else if (decoder_rsp.spatz_req.op_cgf.clear_vstart) begin
          vstart_d = vstart_q & ~vlen_t'(decoder_rsp.spatz_req.rs1);
        end
      end

      if (decoder_rsp.spatz_req.op == VCFG) begin
        // Check if vtype is valid
        if ((vtype_d.vsew > EW_32) || (vtype_d.vlmul == LMUL_RES) || (vtype_d.vlmul + $clog2(ELEN) - 1 < vtype_d.vsew)) begin
          // Invalid
          vtype_d = '{vill: 1'b1, default: '0};
          vl_d    = '0;
        end else begin
          // Valid

          // Set new vtype
          vtype_d = decoder_rsp.spatz_req.vtype;
          if (~decoder_rsp.spatz_req.op_cgf.keep_vl) begin
            // Normal stripmining mode or set to MAXVL
            automatic int unsigned vlmax = 0;
            vlmax = VLENB >> decoder_rsp.spatz_req.vtype.vsew;

            unique case (decoder_rsp.spatz_req.vtype.vlmul)
              LMUL_F2: vlmax >>= 1;
              LMUL_F4: vlmax >>= 2;
              LMUL_F8: vlmax >>= 3;
              LMUL_1: vlmax <<= 0;
              LMUL_2: vlmax <<= 1;
              LMUL_4: vlmax <<= 2;
              LMUL_8: vlmax <<= 3;
            endcase

            vl_d = (decoder_rsp.spatz_req.rs1 == '1) ? MAXVL : (vlmax < decoder_rsp.spatz_req.rs1) ? vlmax : decoder_rsp.spatz_req.rs1;
          end else begin
            // Keep vl mode

            // If new vtype changes ration, mark as illegal
            if (($signed(decoder_rsp.spatz_req.vtype.vsew) - $signed(vtype_q.vsew)) != ($signed(decoder_rsp.spatz_req.vtype.vlmul) - $signed(vtype_q.vlmul))) begin
              vtype_d = '{vill: 1'b1, default: '0};
              vl_d    = '0;
            end
          end
        end
      end // decoder_rsp.spatz_req.op == VCFG
    end // spatz_req_valid
  end

  /////////////
  // Decoder //
  /////////////

  // Decoder req
  decoder_req_t decoder_req;
  logic         decoder_req_valid;
  // Decoder rsp
  decoder_rsp_t decoder_rsp;
  logic         decoder_rsp_valid;

  spatz_decoder i_decoder (
    .clk_i (clk_i),
    .rst_ni(rst_ni),
    // Request
    .decoder_req_i      (decoder_req),
    .decoder_req_valid_i(decoder_req_valid),
    // Response
    .decoder_rsp_o      (decoder_rsp),
    .decoder_rsp_valid_o(decoder_rsp_valid)
  );

  ////////////////
  // Controller //
  ////////////////

  logic spatz_ready = 1'b1;

  always_comb begin : proc_decoder
    decoder_req = '0;
    decoder_req_valid = 1'b0;

    // Decode new instruction if one is received and spatz is ready
    if (x_issue_valid_i & spatz_ready) begin
      decoder_req.instr = x_issue_req_i.instr;
      decoder_req.rs1   = x_issue_req_i.rs[0];
      decoder_req.rs2   = x_issue_req_i.rs[1];
      decoder_req_valid = 1'b1;
    end
  end // proc_decoder

  always_comb begin : proc_controller
    x_issue_resp_o = '0;
    x_result_o = '0;
    x_result_valid_o = '0;
    spatz_req = decoder_rsp.spatz_req;
    spatz_req_valid = 1'b0;

    // New decoded instruction if valid
    if (decoder_rsp_valid && ~decoder_rsp.instr_illegal) begin
      spatz_req_valid = 1'b1;
      case (decoder_rsp.spatz_req.ex_unit)
        CON: begin
          // Read CSR and write back to cpu
          if (decoder_rsp.spatz_req.op == VCSR) begin
            if (decoder_rsp.spatz_req.use_rd) begin
              unique case (decoder_rsp.spatz_req.op_csr.addr)
                riscv_instr::CSR_VSTART: x_result_o.data = 32'(vstart_q);
                riscv_instr::CSR_VL:     x_result_o.data = 32'(vl_q);
                riscv_instr::CSR_VTYPE:  x_result_o.data = 32'(vtype_q);
                riscv_instr::CSR_VLENB:  x_result_o.data = 32'(VLENB);
              endcase
            end
            x_result_valid_o = 1'b1;
            x_result_o.we = 1'b1;
            x_issue_resp_o.writeback = 1'b1;
          end else begin
            // Change configuration and send back vl
            x_result_o.data = 32'(vl_d);
            x_result_o.we = 1'b1;
            x_result_valid_o = 1'b1;
            x_issue_resp_o.writeback = 1'b1;
          end
        end // CON
        VFU: begin
          spatz_req.vtype = vtype_q;
          spatz_req.vl = vl_q;
          spatz_req.vstart = vstart_q;
          spatz_req_valid = 1'b1;
        end // VFU
        LSU: begin

        end
        SLD: begin

        end
      endcase // Operation type
    // New instruction is illegal
    end else if (decoder_rsp_valid & decoder_rsp.instr_illegal) begin
      x_issue_resp_o.exc = 1'b1;
      x_result_o.exc = 1'b1;
    end
  end // proc_controller

  assign x_issue_ready_o = spatz_ready;

  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
