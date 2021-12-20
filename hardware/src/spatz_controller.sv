// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The controller contains all of the CSR registers, the instruction decoder,
// the operation issuer, the register scoreboard, and the result write back to
// the main core.

module spatz_controller
  import spatz_pkg::*;
  import rvv_pkg::*;
#(
  parameter int unsigned NrVregfilePorts  = 1,
  parameter type         x_issue_req_t    = logic,
  parameter type         x_issue_resp_t   = logic,
  parameter type         x_result_t       = logic
) (
  input  logic clk_i,
  input  logic rst_ni,
  // X-Interface Issue
  input  logic x_issue_valid_i,
  output logic x_issue_ready_o,
  input  x_issue_req_t  x_issue_req_i,
  output x_issue_resp_t x_issue_resp_o,
  // X-Interface Result
  output logic x_result_valid_o,
  input  logic x_result_ready_i,
  output x_result_t x_result_o,
  // Spatz req
  output logic spatz_req_valid_o,
  output spatz_req_t spatz_req_o,
  // VFU
  input  logic     vfu_req_ready_i,
  input  logic     vfu_rsp_valid_i,
  input  vfu_rsp_t vfu_rsp_i,
  // VLSU
  input  logic      vlsu_req_ready_i,
  input  logic      vlsu_rsp_valid_i,
  input  vlsu_rsp_t vlsu_rsp_i,
  // Vregfile Scoreboarding
  input  logic       [NrVregfilePorts-1:0] sb_enable_i,
  output logic       [NrVregfilePorts-1:0] sb_enable_o,
  input  vreg_addr_t [NrVregfilePorts-1:0] sb_addr_i
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

  /* verilator lint_off LATCH */
  always_comb begin : proc_vcsr
    vstart_d = vstart_q;
    vl_d     = vl_q;
    vtype_d  = vtype_q;

    if (spatz_req_valid) begin
      // Reset vstart to zero if we have a new non CSR operation
      if (spatz_req.op_cgf.reset_vstart) begin
        vstart_d = '0;
      end

      // Set new vstart if we have a VCSR instruction
      // that accesses the vstart register.
      if (spatz_req.op == VCSR) begin
        if (spatz_req.op_cgf.write_vstart) begin
          vstart_d = vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cgf.set_vstart) begin
          vstart_d = vstart_q | vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cgf.clear_vstart) begin
          vstart_d = vstart_q & ~vlen_t'(spatz_req.rs1);
        end
      end

      // Change vtype and vl if we have a config instruction
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
  /* verilator lint_on LATCH */

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

  ////////////////
  // Scoreboard //
  ////////////////

  // Extract element id from vrf address (is dependent on lmul)
  function automatic logic [$clog2(VELE*8)-1:0] vrf_element(vreg_addr_t addr, vlmul_e lmul);
    unique case (lmul)
      LMUL_F8,
      LMUL_F4,
      LMUL_F2,
      LMUL_1: vrf_element = addr[$clog2(VELE)-1:0];
      LMUL_2: vrf_element = addr[$clog2(VELE*2)-1:0];
      LMUL_4: vrf_element = addr[$clog2(VELE*4)-1:0];
      LMUL_8: vrf_element = addr[$clog2(VELE*8)-1:0];
      default: vrf_element = '0;
    endcase
  endfunction

  // Scoreboard metadata
  typedef struct packed {
    logic valid;
    logic [$clog2(NrVregfilePorts)-1:0] port;
  } sb_metadata_t;

  // Port scoreboard metadata
  typedef struct packed {
    logic valid;
    logic [$clog2(VELE*8)-1:0] element;
    logic deps_valid;
    logic [$clog2(NrVregfilePorts)-1:0] deps;
  } sb_port_metadata_t;

  // Register file scoreboard. Keeps track which vector is currently being
  // accessed by a vrf port of a unit (last port that has accessed the vector).
  sb_metadata_t [NRVREG-1:0] sb_q, sb_d;
  `FF(sb_q, sb_d, '0)

  // Port scoreboard. Keeps track of which element is currently being accessed,
  // and if the port access is dependent on another one (read and write).
  sb_port_metadata_t [NrVregfilePorts-1:0] sb_port_q, sb_port_d;
  `FF(sb_port_q, sb_port_d, '0)

  always_comb begin : score_board
    sb_d  = sb_q;
    sb_port_d = sb_port_q;

    sb_enable_o = '0;

    // A unit has finished its vrf access. Set the port scoreboard to invalid,
    // and if the vrf scoreboard still has the port listed as last accessed, set it
    // to invalid as well.
    if (vfu_rsp_valid_i) begin
      // VS2
      sb_port_d[0].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vs2].valid && sb_q[vfu_rsp_i.vs2].port == 'd0) sb_d[vfu_rsp_i.vs2].valid = 1'b0;
      // VS1
      sb_port_d[1].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vs1].valid && sb_q[vfu_rsp_i.vs1].port == 'd1) sb_d[vfu_rsp_i.vs1].valid = 1'b0;
      // VD (read and write)
      sb_port_d[2].valid = 1'b0;
      sb_port_d[5].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vd].valid && (sb_q[vfu_rsp_i.vd].port == 'd2 || sb_q[vfu_rsp_i.vd].port == 'd5)) sb_d[vfu_rsp_i.vd].valid = 1'b0;
    end
    if (vlsu_rsp_valid_i) begin
      // VD (read and write)
      sb_port_d[3].valid = 1'b0;
      sb_port_d[6].valid = 1'b0;
      if (sb_q[vlsu_rsp_i.vd].valid && (sb_q[vlsu_rsp_i.vd].port == 'd3 || sb_q[vlsu_rsp_i.vd].port == 'd6)) sb_d[vlsu_rsp_i.vd].valid = 1'b0;
    end

    // Initialize the scoreboard metadata if we have a new instruction issued.
    // Set the ports of the unit to valid if they are being used, reset the currently
    // accessed element to zero, and check if the port has a dependency. If the port is
    // used, then not this down in the vrf scoreboard as well.
    if (spatz_req_valid) begin
      if (spatz_req.ex_unit == VFU) begin
        // VS2
        sb_port_d[0].valid = spatz_req.use_vs2;
        sb_port_d[0].element = '0;
        sb_port_d[0].deps_valid = sb_q[spatz_req.vs2].valid;
        sb_port_d[0].deps = sb_q[spatz_req.vs2].port;

        sb_d[spatz_req.vs2].valid = spatz_req.use_vs2;
        if (spatz_req.use_vs2) sb_d[spatz_req.vs2].port = 'd0;

        // VS1
        sb_port_d[1].valid = spatz_req.use_vs1;
        sb_port_d[1].element = '0;
        sb_port_d[1].deps_valid = sb_q[spatz_req.vs1].valid;
        sb_port_d[1].deps = sb_q[spatz_req.vs1].port;

        sb_d[spatz_req.vs1].valid = spatz_req.use_vs1;
        if (spatz_req.use_vs1) sb_d[spatz_req.vs1].port = 'd1;

        // VD (read)
        sb_port_d[2].valid = spatz_req.use_vd & spatz_req.vd_is_src;
        sb_port_d[2].element = '0;
        sb_port_d[2].deps_valid = sb_q[spatz_req.vd].valid;
        sb_port_d[2].deps = sb_q[spatz_req.vd].port;

        sb_d[spatz_req.vd].valid = spatz_req.use_vd & spatz_req.vd_is_src;
        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_d[spatz_req.vd].port = 'd2;

        // VD (write)
        sb_port_d[5].valid = spatz_req.use_vd;
        sb_port_d[5].element = '0;
        sb_port_d[5].deps_valid = sb_q[spatz_req.vd].valid;
        sb_port_d[5].deps = sb_q[spatz_req.vd].port;

        sb_d[spatz_req.vd].valid = spatz_req.use_vd;
        if (spatz_req.use_vd) sb_d[spatz_req.vd].port = 'd5;
      end else if (spatz_req.ex_unit == LSU) begin
        // VD (read)
        sb_port_d[3].valid = spatz_req.use_vd & spatz_req.vd_is_src;
        sb_port_d[3].element = '0;
        sb_port_d[3].deps_valid = sb_q[spatz_req.vd].valid;
        sb_port_d[3].deps = sb_q[spatz_req.vd].port;

        sb_d[spatz_req.vd].valid = spatz_req.use_vd & spatz_req.vd_is_src;
        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_d[spatz_req.vd].port = 'd3;

        // VD (write)
        sb_port_d[6].valid = spatz_req.use_vd & ~spatz_req.vd_is_src;
        sb_port_d[6].element = '0;
        sb_port_d[6].deps_valid = sb_q[spatz_req.vd].valid;
        sb_port_d[6].deps = sb_q[spatz_req.vd].port;

        sb_d[spatz_req.vd].valid = spatz_req.use_vd & ~spatz_req.vd_is_src;
        if (spatz_req.use_vd & ~spatz_req.vd_is_src) sb_d[spatz_req.vd].port = 'd6;
      end else if (spatz_req.ex_unit == SLD) begin
        // VS2
        sb_port_d[4].valid = spatz_req.use_vs2;
        sb_port_d[4].element = '0;
        sb_port_d[4].deps_valid = sb_q[spatz_req.vs2].valid;
        sb_port_d[4].deps = sb_q[spatz_req.vs2].port;

        sb_d[spatz_req.vs2].valid = spatz_req.use_vs2;
        if (spatz_req.use_vs2) sb_d[spatz_req.vs2].port = 'd4;

        // VD (write)
        sb_port_d[7].valid = spatz_req.use_vd;
        sb_port_d[7].element = '0;
        sb_port_d[7].deps_valid = sb_q[spatz_req.vd].valid;
        sb_port_d[7].deps = sb_q[spatz_req.vd].port;

        sb_d[spatz_req.vd].valid = spatz_req.use_vd;
        if (spatz_req.use_vd) sb_d[spatz_req.vd].port = 'd7;
      end
    end

    // For every port, update the element that is currently being accessed.
    // If the desired element if lower than the one of the dependency, then
    // grant access to the register file.
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      automatic int unsigned element = vrf_element(sb_addr_i[port], vtype_q.vlmul);
      automatic sb_port_metadata_t deps = sb_port_q[sb_port_q[port].deps];

      if (sb_enable_i[port]) begin
        // Update id of accessed element
        sb_port_d[port].element = element;

        // Check if we have a dependency, and if so if we are accessing an element that has already been accessed by it.
        if ((sb_port_q[port].deps_valid && ((deps.valid && element != deps.element) || !deps.valid)) || !sb_port_q[port].deps_valid) begin
          // Grant port access to register file
          sb_enable_o[port] = 1'b1;
        end
      end
    end
  end

  /////////////
  // Issuing //
  /////////////

  // Retire CSR instruction and write back result to main core.
  logic retire_csr;

  // We stall issuing a new instruction if the corresponding execution unit is
  // not ready yet, or we have a change in LMUL, for which we need to let all the
  // units finish first before scheduling a new operation (to avoid running into
  // issues with the socreboard).
  logic stall, vfu_stall, vlsu_stall, vsld_stall, csr_stall;
  assign stall = vfu_stall | vlsu_stall | vsld_stall | csr_stall;
  assign vfu_stall  = ~vfu_req_ready_i  & (spatz_req.ex_unit == VFU) & (decoder_rsp_valid | ~spatz_req_buffer_empty_q);
  assign vlsu_stall = ~vlsu_req_ready_i & (spatz_req.ex_unit == LSU) & (decoder_rsp_valid | ~spatz_req_buffer_empty_q);
  assign vsld_stall = 1'b0;
  assign csr_stall  = (~vfu_req_ready_i | ~vlsu_req_ready_i) & (spatz_req.ex_unit == CON) & (spatz_req.vtype.vlmul != vtype_q.vlmul) & (decoder_rsp_valid | ~spatz_req_buffer_empty_q);

  // Issue not operation to execution units
  always_comb begin : ex_issue
    retire_csr = 1'b0;
    spatz_ready_d = spatz_ready_q;
    // If the request buffer is not empty, issue its contents for the next operation
    spatz_req = spatz_req_buffer_empty_q ? decoder_rsp.spatz_req : spatz_req_buffer_q;
    spatz_req.id = decoder_rsp_valid ? x_issue_req_i.id : spatz_req.id;
    spatz_req_illegal = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
    spatz_req_valid = 1'b0;

    spatz_req_buffer_d       = spatz_req_buffer_q;
    spatz_req_buffer_empty_d = spatz_req_buffer_empty_q;

    // New instruction wants to access busy resource. Store it in the request buffer.
    if (decoder_rsp_valid && stall) begin
      spatz_req_buffer_d = spatz_req;
      spatz_req_buffer_empty_d = 1'b0;
      spatz_ready_d = 1'b0;
    // We have a ew instruction and there is no stall.
    end else if ((decoder_rsp_valid && ~spatz_req_illegal) || (!spatz_req_buffer_empty_q && !stall)) begin
      // Reset request buffer and accept new instructions
      spatz_req_buffer_empty_d = 1'b1;
      spatz_ready_d = 1'b1;
      spatz_req_valid = 1'b1;

      if (spatz_req.ex_unit == VFU) begin
        // Overwrite all csrs in request
        spatz_req.vtype  = vtype_q;
        spatz_req.vl     = vl_q;
        spatz_req.vstart = vstart_q;
      end if (spatz_req.ex_unit == LSU) begin
        // Overwrite vl and vstart in request (preserve vtype with vsew)
        spatz_req.vl     = vl_q;
        spatz_req.vstart = vstart_q;
      end if (spatz_req.ex_unit == SLD) begin
        // Overwrite all csrs in request
        spatz_req.vtype  = vtype_q;
        spatz_req.vl     = vl_q;
        spatz_req.vstart = vstart_q;
      end else begin
        // Do not overwrite csrs, but retire new ones
        retire_csr = 1'b1;
      end
    end
  end // ex_issue

  // Respond to core about the decoded instruction.
  always_comb begin : x_issue_resp
    x_issue_resp_o = '0;

    // We have a new valid instruction
    if (decoder_rsp_valid && !spatz_req_illegal) begin
      // Accept the new instruction
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
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
            x_issue_resp_o.accept = 1'b0;
          end
        end // SLD
      endcase // Operation type
    // The decoding resulted in an illegal instruction
    end else if (decoder_rsp_valid && spatz_req_illegal) begin
      // Do not accept it
      x_issue_resp_o.accept = 1'b0;
    end
  end // x_issue_resp

  //////////////
  // Retiring //
  //////////////

  // Retire an operation/instruction and write back result to core
  // if necessary.
  always_comb begin : retire
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
  end // retire

  // Signal to core that Spatz is ready for a new instruction
  assign x_issue_ready_o = spatz_ready_q;

  // Send request off to execution units
  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
