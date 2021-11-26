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
  output spatz_req_t spatz_req_o,
  // VFU
  input  logic     vfu_req_ready_i,
  input  logic     vfu_rsp_valid_i,
  input  vfu_rsp_t vfu_rsp_i
);

  // Include FF
  `include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Spatz ready to handle new request
  logic spatz_ready_q, spatz_ready_d;
  `FF(spatz_ready_q, spatz_ready_d, '1);

  // Spatz request
  spatz_req_t spatz_req;
  logic       spatz_req_valid;
  logic       spatz_req_illegal;

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
      if (spatz_req.op_cgf.reset_vstart) begin
        vstart_d = '0;
      end

      // Set new vstart when written over vcsrs
      if (spatz_req.op == VCSR) begin
        if (spatz_req.op_cgf.write_vstart) begin
          vstart_d = vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cgf.set_vstart) begin
          vstart_d = vstart_q | vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cgf.clear_vstart) begin
          vstart_d = vstart_q & ~vlen_t'(spatz_req.rs1);
        end
      end

      if (spatz_req.op == VCFG) begin
        // Check if vtype is valid
        if ((vtype_d.vsew > EW_32) || (vtype_d.vlmul == LMUL_RES) || (vtype_d.vlmul == LMUL_F8) || (signed'(vtype_d.vlmul) + signed'($clog2(ELENB)) < signed'(vtype_d.vsew))) begin
          // Invalid
          vtype_d = '{vill: 1'b1, default: '0};
          vl_d    = '0;
        end else begin
          // Valid

          // Set new vtype
          vtype_d = spatz_req.vtype;
          if (!spatz_req.op_cgf.keep_vl) begin
            // Normal stripmining mode or set to MAXVL
            automatic int unsigned vlmax = 0;
            vlmax = VLENB >> spatz_req.vtype.vsew;

            unique case (spatz_req.vtype.vlmul)
              LMUL_F2: vlmax >>= 1;
              LMUL_F4: vlmax >>= 2;
              LMUL_1:  vlmax <<= 0;
              LMUL_2:  vlmax <<= 1;
              LMUL_4:  vlmax <<= 2;
              LMUL_8:  vlmax <<= 3;
              default: vlmax = vlmax;
            endcase

            vl_d = (spatz_req.rs1 == '1) ? MAXVL : (vlmax < spatz_req.rs1) ? vlmax : spatz_req.rs1;
          end else begin
            // Keep vl mode

            // If new vtype changes ration, mark as illegal
            if (($signed(spatz_req.vtype.vsew) - $signed(vtype_q.vsew)) != ($signed(spatz_req.vtype.vlmul) - $signed(vtype_q.vlmul))) begin
              vtype_d = '{vill: 1'b1, default: '0};
              vl_d    = '0;
            end
          end
        end
      end // spatz_req.op == VCFG
    end // spatz_req_valid
  end

  ////////////////
  // Scoreboard //
  ////////////////

  // Scoreboard keeping track of which execution unit is
  // accessing which register.
  // CON = None
  //logic [NRVREG-1:0] sb_q, sb_d;
  //`FF(sb_q, sb_d, '0)

  //////////////
  // Decoding //
  //////////////

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

  // Decode new instruction if new request arrives
  always_comb begin : proc_decode
    decoder_req = '0;
    decoder_req_valid = 1'b0;

    // Decode new instruction if one is received and spatz is ready
    if (x_issue_valid_i && spatz_ready_q) begin
      decoder_req.instr     = x_issue_req_i.instr;
      decoder_req.rs1       = x_issue_req_i.rs[0];
      decoder_req.rs1_valid = x_issue_req_i.rs_valid[0];
      decoder_req.rs2       = x_issue_req_i.rs[1];
      decoder_req.rs2_valid = x_issue_req_i.rs_valid[1];
      decoder_req_valid     = 1'b1;
    end
  end // proc_decode

  ////////////////////
  // Request Buffer //
  ////////////////////

  // Spatz request buffer
  spatz_req_t spatz_req_buffer_q, spatz_req_buffer_d;
  logic       spatz_req_buffer_empty_q, spatz_req_buffer_empty_d;

  // Spatz request buffer is used when there is a new request but execution
  // unit is still busy
  always_ff @(posedge clk_i or negedge rst_ni) begin : proc_spatz_req_buffer
    if(~rst_ni) begin
      spatz_req_buffer_q       <= 0;
      spatz_req_buffer_empty_q <= 0;
    end else begin
      spatz_req_buffer_q       <= spatz_req_buffer_d;
      spatz_req_buffer_empty_q <= spatz_req_buffer_empty_d;
    end
  end

  /////////////
  // Issuing //
  /////////////

  logic retire_csr;

  //logic operands_ready, destination_ready;
  //assign operands_ready = (decoder_rsp_valid | ~spatz_req_buffer_empty_q) & (spatz_req.use_vs1 & ~sb_q[spatz_req.vs1]);
  //assign destination_ready = (decoder_rsp_valid | ~spatz_req_buffer_empty_q) & (~spatz_req.use_vd | (spatz_req.use_vd & ~sb_q[spatz_req.vd]));

  logic stall, vfu_stall, vlsu_stall, vsld_stall;
  assign stall = vfu_stall | vlsu_stall | vsld_stall;
  assign vfu_stall = ~vfu_req_ready_i & ((decoder_rsp_valid & (spatz_req.ex_unit == VFU))
                     | (~spatz_req_buffer_empty_q & (spatz_req_buffer_q.ex_unit == VFU)));
  assign vlsu_stall = 1'b0;
  assign vsld_stall = 1'b0;

  always_comb begin : proc_issue
    retire_csr = 1'b0;
    spatz_ready_d = spatz_ready_q;
    spatz_req = spatz_req_buffer_empty_q ? decoder_rsp.spatz_req : spatz_req_buffer_q;
    spatz_req.id = x_issue_req_i.id;
    spatz_req_illegal = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
    spatz_req_valid = 1'b0;

    spatz_req_buffer_d       = spatz_req_buffer_q;
    spatz_req_buffer_empty_d = spatz_req_buffer_empty_q;

    // New istruction wants to access busy resource
    if (decoder_rsp_valid && stall) begin
      spatz_req_buffer_d = spatz_req;
      spatz_req_buffer_empty_d = 1'b0;
      spatz_ready_d = 1'b0;
    // New decoded instruction if valid
    end else if ((decoder_rsp_valid && ~spatz_req_illegal) || (!spatz_req_buffer_empty_q && !stall)) begin
      // Resolve and reset buffer
      if (!spatz_req_buffer_empty_q) begin
        spatz_req = spatz_req_buffer_q;
      end

      // Reset request buffer and accept new instructions
      spatz_req_buffer_empty_d = 1'b1;
      spatz_ready_d = 1'b1;
      spatz_req_valid = 1'b1;

      if (spatz_req.ex_unit != CON) begin
        // Overwrite csrs in request if it is not destined to csr regs
        spatz_req.vtype  = vtype_q;
        spatz_req.vl     = vl_q;
        spatz_req.vstart = vstart_q;
      end else begin
        retire_csr = 1'b1;
      end
    end
  end // proc_issue

  always_comb begin : proc_x_issue_resp
    x_issue_resp_o = '0;

    if (decoder_rsp_valid && !spatz_req_illegal) begin
      x_issue_resp_o.accept = 1'b1;

      case (spatz_req.ex_unit)
        CON: begin
          x_issue_resp_o.writeback = spatz_req.use_rd;
        end // CON
        VFU: begin
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
            x_issue_resp_o.accept = 1'b0;
          end
        end // VFU
        LSU: begin
          x_issue_resp_o.loadstore = 1'b1;
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
            x_issue_resp_o.accept = 1'b0;
          end
        end // LSU
        SLD: begin
          x_issue_resp_o.loadstore = 1'b1;
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
            x_issue_resp_o.accept = 1'b0;
          end
        end // SLD
      endcase // Operation type
    end else if (decoder_rsp_valid && spatz_req_illegal) begin
      x_issue_resp_o.accept = 1'b0;
    end
  end // proc_x_issue_resp

  //////////////
  // Retiring //
  //////////////

  always_comb begin : proc_retire
    x_result_o = '0;
    x_result_valid_o = '0;

    if (retire_csr) begin
      // Read CSR and write back to cpu
      if (spatz_req.op == VCSR) begin
        if (spatz_req.use_rd) begin
          unique case (spatz_req.op_csr.addr)
            riscv_instr::CSR_VSTART: x_result_o.data = elen_t'(vstart_q);
            riscv_instr::CSR_VL:     x_result_o.data = elen_t'(vl_q);
            riscv_instr::CSR_VTYPE:  x_result_o.data = elen_t'(vtype_q);
            riscv_instr::CSR_VLENB:  x_result_o.data = elen_t'(VLENB);
            riscv_instr::CSR_VXSAT:  x_result_o.data = '0;
            riscv_instr::CSR_VXRM:   x_result_o.data = '0;
            riscv_instr::CSR_VCSR:   x_result_o.data = '0;
            default:                 x_result_o.data = '0;
          endcase
        end
        x_result_o.id = spatz_req.id;
        x_result_o.rd = spatz_req.rd;
        x_result_valid_o = 1'b1;
        x_result_o.we = 1'b1;
      end else begin
        // Change configuration and send back vl
        x_result_o.id = spatz_req.id;
        x_result_o.rd = spatz_req.rd;
        x_result_o.data = elen_t'(vl_d);
        x_result_o.we = 1'b1;
        x_result_valid_o = 1'b1;
      end
    end
  end // proc_retire

  assign x_issue_ready_o = spatz_ready_q;

  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
