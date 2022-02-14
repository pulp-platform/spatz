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
  input  logic          x_issue_valid_i,
  output logic          x_issue_ready_o,
  input  x_issue_req_t  x_issue_req_i,
  output x_issue_resp_t x_issue_resp_o,

  // X-Interface Result
  output logic      x_result_valid_o,
  input  logic      x_result_ready_i,
  output x_result_t x_result_o,

  // Spatz req
  output logic       spatz_req_valid_o,
  output spatz_req_t spatz_req_o,

  // VFU
  input  logic     vfu_req_ready_i,
  input  logic     vfu_rsp_valid_i,
  input  vfu_rsp_t vfu_rsp_i,

  // VLSU
  input  logic      vlsu_req_ready_i,
  input  logic      vlsu_rsp_valid_i,
  input  vlsu_rsp_t vlsu_rsp_i,

  // VSLDU
  input  logic       vsldu_req_ready_i,
  input  logic       vsldu_rsp_valid_i,
  input  vsldu_rsp_t vsldu_rsp_i,

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
        if ((spatz_req.vtype.vsew > EW_32) || (spatz_req.vtype.vlmul inside {LMUL_RES, LMUL_F8}) || (signed'(spatz_req.vtype.vlmul) + signed'($clog2(ELEN)) < signed'(spatz_req.vtype.vsew))) begin
          // Invalid
          vtype_d = '{vill: 1'b1, default: '0};
          vl_d    = '0;
        end else begin
          // Valid

          // Set new vtype
          vtype_d = spatz_req.vtype;
          if (!spatz_req.op_cgf.keep_vl) begin
            // Normal stripmining mode or set to MAXVL
            automatic logic [MAXVL-1:0] vlmax = 0;
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
    if (x_issue_valid_i && x_issue_ready_o) begin
      decoder_req.instr     = x_issue_req_i.instr;
      decoder_req.rs1       = x_issue_req_i.rs[0];
      decoder_req.rs1_valid = x_issue_req_i.rs_valid[0];
      decoder_req.rs2       = x_issue_req_i.rs[1];
      decoder_req.rs2_valid = x_issue_req_i.rs_valid[1];
      decoder_req.id        = x_issue_req_i.id;
      decoder_req_valid     = 1'b1;
    end
  end // proc_decode

  ////////////////////
  // Request Buffer //
  ////////////////////

  // Spatz request
  spatz_req_t buffer_spatz_req;
  // Buffer state signals
  logic req_buffer_ready, req_buffer_valid, req_buffer_pop;

  // One element wide instruction buffer
  fall_through_register #(
    .T(spatz_req_t)
  ) i_req_buffer (
    .clk_i     (clk_i),
    .rst_ni    (rst_ni),
    .clr_i     (1'b0),
    .testmode_i(1'b0),
    .ready_o   (req_buffer_ready),
    .valid_o   (req_buffer_valid),
    .data_i    (decoder_rsp.spatz_req),
    .valid_i   (decoder_rsp_valid),
    .data_o    (buffer_spatz_req),
    .ready_i   (req_buffer_pop)
  );

  ////////////////
  // Scoreboard //
  ////////////////

  // Scoreboard metadata
  typedef struct packed {
    logic     valid;
    sb_port_e port;
  } sb_metadata_t;

  // Port scoreboard metadata
  typedef struct packed {
    logic     valid;
    logic     [$clog2(VELE*8)-1:0] element;
    logic     deps_valid;
    sb_port_e deps;
  } sb_port_metadata_t;

  // Register file scoreboard. Keeps track which vector is currently being
  // accessed by a vrf port of a unit (last port that has accessed the vector).
  sb_metadata_t [NRVREG-1:0] sb_q, sb_d, sb_reset;
  `FF(sb_q, sb_d, '0)

  // Port scoreboard. Keeps track of which element is currently being accessed,
  // and if the port access is dependent on another one (read and write).
  sb_port_metadata_t [NrVregfilePorts-1:0] sb_port_q, sb_port_d;
  `FF(sb_port_q, sb_port_d, '0)

  logic sb_deps_valid_vs1;
  logic sb_deps_valid_vs2;
  logic sb_deps_valid_vd;

  sb_port_e sb_port_vs1;
  sb_port_e sb_port_vs2;
  sb_port_e sb_port_vd;

  logic sb_deps_valid_vs1_update;
  logic sb_deps_valid_vs2_update;
  logic sb_deps_valid_vd_rd_update;
  logic sb_deps_valid_vd_wd_update;

  sb_port_e sb_port_vs1_update;
  sb_port_e sb_port_vs2_update;
  sb_port_e sb_port_vd_rd_update;
  sb_port_e sb_port_vd_wd_update;

  always_comb begin : score_board
    sb_d  = sb_q;
    sb_port_d = sb_port_q;

    sb_enable_o = '0;

    sb_deps_valid_vs1 = 1'b0;
    sb_deps_valid_vs2 = 1'b0;
    sb_deps_valid_vd  = 1'b0;

    sb_port_vs1 = SB_VFU_VS2_RD;
    sb_port_vs2 = SB_VFU_VS2_RD;
    sb_port_vd  = SB_VFU_VS2_RD;

    sb_deps_valid_vs1_update   = 1'b0;
    sb_deps_valid_vs2_update   = 1'b0;
    sb_deps_valid_vd_rd_update = 1'b0;
    sb_deps_valid_vd_wd_update = 1'b0;

    sb_port_vs1_update   = SB_VFU_VS2_RD;
    sb_port_vs2_update   = SB_VFU_VS2_RD;
    sb_port_vd_rd_update = SB_VFU_VS2_RD;
    sb_port_vd_wd_update = SB_VFU_VS2_RD;

    // For every port, update the element that is currently being accessed.
    // If the desired element is lower than the one of the dependency, then
    // grant access to the register file.
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      if (sb_enable_i[port]) begin
        // Update id of accessed element
        sb_port_d[port].element = sb_addr_i[port][$clog2(VELE*8)-1:0];
      end
    end

    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      for (int unsigned deps = 0; deps < NrVregfilePorts; deps++) begin
        if ((port inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD} && deps inside {SB_VLSU_VD_RD, SB_VSLDU_VS2_RD, SB_VLSU_VD_WD, SB_VSLDU_VD_WD}) ||
            (port inside {SB_VLSU_VD_RD, SB_VLSU_VD_WD} && deps inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VSLDU_VS2_RD, SB_VFU_VD_WD, SB_VSLDU_VD_WD}) ||
            (port inside {SB_VSLDU_VS2_RD, SB_VSLDU_VD_WD} && deps inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VLSU_VD_RD, SB_VFU_VD_WD, SB_VLSU_VD_WD})) begin
          if (sb_enable_i[port]) begin
            if (!sb_port_q[port].deps_valid) begin
              sb_enable_o[port] = 1'b1;
            end else if (sb_port_q[port].deps_valid && deps == sb_port_q[port].deps) begin
              if ((sb_port_q[deps].valid && (sb_port_d[port].element < sb_port_d[deps].element)) || !sb_port_q[deps].valid) begin
                // Grant port access to register file
                sb_enable_o[port] = 1'b1;
              end
            end
          end
        end
      end
    end

    // A unit has finished its vrf access. Set the port scoreboard to invalid,
    // and if the vrf scoreboard still has the port listed as last accessed, set it
    // to invalid as well.
    if (vfu_rsp_valid_i) begin
      // VS2
      sb_port_d[SB_VFU_VS2_RD].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vs2].valid && sb_q[vfu_rsp_i.vs2].port == SB_VFU_VS2_RD) sb_d[vfu_rsp_i.vs2].valid = 1'b0;
      // VS1
      sb_port_d[SB_VFU_VS1_RD].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vs1].valid && sb_q[vfu_rsp_i.vs1].port == SB_VFU_VS1_RD) sb_d[vfu_rsp_i.vs1].valid = 1'b0;
      // VD (read and write)
      sb_port_d[SB_VFU_VD_RD].valid = 1'b0;
      sb_port_d[SB_VFU_VD_WD].valid = 1'b0;
      if (sb_q[vfu_rsp_i.vd].valid && (sb_q[vfu_rsp_i.vd].port == SB_VFU_VD_RD || sb_q[vfu_rsp_i.vd].port == SB_VFU_VD_WD)) sb_d[vfu_rsp_i.vd].valid = 1'b0;
    end
    if (vlsu_rsp_valid_i) begin
      // VD (read and write)
      sb_port_d[SB_VLSU_VD_RD].valid = 1'b0;
      sb_port_d[SB_VLSU_VD_WD].valid = 1'b0;
      if (sb_q[vlsu_rsp_i.vd].valid && (sb_q[vlsu_rsp_i.vd].port == SB_VLSU_VD_RD || sb_q[vlsu_rsp_i.vd].port == SB_VLSU_VD_WD)) sb_d[vlsu_rsp_i.vd].valid = 1'b0;
    end
    if (vsldu_rsp_valid_i) begin
      // VS2
      sb_port_d[SB_VSLDU_VS2_RD].valid = 1'b0;
      if (sb_q[vsldu_rsp_i.vs2].valid && sb_q[vsldu_rsp_i.vs2].port == SB_VSLDU_VS2_RD) sb_d[vsldu_rsp_i.vs2].valid = 1'b0;
      // VD (write)
      sb_port_d[SB_VSLDU_VD_WD].valid = 1'b0;
      if (sb_q[vsldu_rsp_i.vd].valid && sb_q[vsldu_rsp_i.vd].port == SB_VSLDU_VD_WD) sb_d[vsldu_rsp_i.vd].valid = 1'b0;
    end

    sb_reset = sb_d;

    // Clear deps valid when dependency has finished
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      for (int unsigned deps = 0; deps < NrVregfilePorts; deps++) begin
        if ((port inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD} && deps inside {SB_VLSU_VD_RD, SB_VSLDU_VS2_RD, SB_VLSU_VD_WD, SB_VSLDU_VD_WD}) ||
            (port inside {SB_VLSU_VD_RD, SB_VLSU_VD_WD} && deps inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VSLDU_VS2_RD, SB_VFU_VD_WD, SB_VSLDU_VD_WD}) ||
            (port inside {SB_VSLDU_VS2_RD, SB_VSLDU_VD_WD} && deps inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VLSU_VD_RD, SB_VFU_VD_WD, SB_VLSU_VD_WD})) begin
          if (deps == sb_port_q[port].deps) begin
            if (sb_port_d[port].valid && sb_port_d[port].deps_valid && !sb_port_d[deps].valid) begin
              sb_port_d[port].deps_valid = 1'b0;
            end
          end
        end
      end
    end

    // Initialize the scoreboard metadata if we have a new instruction issued.
    // Set the ports of the unit to valid if they are being used, reset the currently
    // accessed element to zero, and check if the port has a dependency. If the port is
    // used, then note this down in the vrf scoreboard as well.
    if (spatz_req_valid) begin
      sb_deps_valid_vs1 = sb_reset[spatz_req.vs1].valid;
      sb_deps_valid_vs2 = sb_reset[spatz_req.vs2].valid;
      sb_deps_valid_vd  = sb_reset[spatz_req.vd].valid;

      sb_port_vs1 = sb_q[spatz_req.vs1].port;
      sb_port_vs2 = sb_q[spatz_req.vs2].port;
      sb_port_vd  = sb_q[spatz_req.vd].port;

      if (spatz_req.ex_unit == VFU) begin
        // VS2
        sb_port_d[SB_VFU_VS2_RD].valid      = spatz_req.use_vs2;
        sb_port_d[SB_VFU_VS2_RD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VFU_VS2_RD].deps_valid = sb_deps_valid_vs2 & ~(sb_port_vs2 inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD});
        sb_port_d[SB_VFU_VS2_RD].deps       = sb_port_vs2;

        if (spatz_req.use_vs2) sb_deps_valid_vs2_update = spatz_req.use_vs2;
        if (spatz_req.use_vs2) sb_port_vs2_update = SB_VFU_VS2_RD;

        // VS1
        sb_port_d[SB_VFU_VS1_RD].valid      = spatz_req.use_vs1;
        sb_port_d[SB_VFU_VS1_RD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VFU_VS1_RD].deps_valid = sb_deps_valid_vs1 & ~(sb_port_vs1 inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD});
        sb_port_d[SB_VFU_VS1_RD].deps       = sb_port_vs1;

        if (spatz_req.use_vs1) sb_deps_valid_vs1_update = spatz_req.use_vs1;
        if (spatz_req.use_vs1) sb_port_vs1_update = SB_VFU_VS1_RD;

        // VD (read)
        sb_port_d[SB_VFU_VD_RD].valid      = spatz_req.use_vd & spatz_req.vd_is_src;
        sb_port_d[SB_VFU_VD_RD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VFU_VD_RD].deps_valid = sb_deps_valid_vd & ~(sb_port_vd inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD});
        sb_port_d[SB_VFU_VD_RD].deps       = sb_port_vd;

        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_deps_valid_vd_rd_update = spatz_req.use_vd & spatz_req.vd_is_src;
        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_port_vd_rd_update = SB_VFU_VD_RD;

        // VD (write)
        sb_port_d[SB_VFU_VD_WD].valid      = spatz_req.use_vd;
        sb_port_d[SB_VFU_VD_WD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VFU_VD_WD].deps_valid = sb_deps_valid_vd & ~(sb_port_vd inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD, SB_VFU_VD_WD});
        sb_port_d[SB_VFU_VD_WD].deps       = sb_port_vd;

        if (spatz_req.use_vd) sb_deps_valid_vd_wd_update = spatz_req.use_vd;
        if (spatz_req.use_vd) sb_port_vd_wd_update = SB_VFU_VD_WD;
      end else if (spatz_req.ex_unit == LSU) begin
        // VD (read)
        sb_port_d[SB_VLSU_VD_RD].valid      = spatz_req.use_vd & spatz_req.vd_is_src;
        sb_port_d[SB_VLSU_VD_RD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VLSU_VD_RD].deps_valid = sb_deps_valid_vd & ~(sb_port_vd inside {SB_VLSU_VD_RD, SB_VLSU_VD_WD});
        sb_port_d[SB_VLSU_VD_RD].deps       = sb_port_vd;

        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_deps_valid_vd_rd_update = spatz_req.use_vd & spatz_req.vd_is_src;
        if (spatz_req.use_vd & spatz_req.vd_is_src) sb_port_vd_rd_update = SB_VLSU_VD_RD;

        // VD (write)
        sb_port_d[SB_VLSU_VD_WD].valid      = spatz_req.use_vd & ~spatz_req.vd_is_src;
        sb_port_d[SB_VLSU_VD_WD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VLSU_VD_WD].deps_valid = sb_deps_valid_vd & ~(sb_port_vd inside {SB_VLSU_VD_RD, SB_VLSU_VD_WD});
        sb_port_d[SB_VLSU_VD_WD].deps       = sb_port_vd;

        if (spatz_req.use_vd & ~spatz_req.vd_is_src) sb_deps_valid_vd_wd_update = spatz_req.use_vd & ~spatz_req.vd_is_src;
        if (spatz_req.use_vd & ~spatz_req.vd_is_src) sb_port_vd_wd_update = SB_VLSU_VD_WD;
      end else if (spatz_req.ex_unit == SLD) begin
        // VS2
        sb_port_d[SB_VSLDU_VS2_RD].valid      = spatz_req.use_vs2;
        sb_port_d[SB_VSLDU_VS2_RD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VSLDU_VS2_RD].deps_valid = sb_deps_valid_vs2 & ~(sb_port_vs2 inside {SB_VSLDU_VS2_RD, SB_VSLDU_VD_WD});
        sb_port_d[SB_VSLDU_VS2_RD].deps       = sb_port_vs2;

        if (spatz_req.use_vs2) sb_deps_valid_vs2_update = spatz_req.use_vs2;
        if (spatz_req.use_vs2) sb_port_vs2_update = SB_VSLDU_VS2_RD;

        // VD (write)
        sb_port_d[SB_VSLDU_VD_WD].valid      = spatz_req.use_vd;
        sb_port_d[SB_VSLDU_VD_WD].element    = spatz_req.vstart[$bits(vlen_t)-1:$clog2(VELEB)];
        sb_port_d[SB_VSLDU_VD_WD].deps_valid = sb_deps_valid_vd & ~(sb_port_vd inside {SB_VSLDU_VS2_RD, SB_VSLDU_VD_WD});
        sb_port_d[SB_VSLDU_VD_WD].deps       = sb_port_vd;

        if (spatz_req.use_vd) sb_deps_valid_vd_wd_update = spatz_req.use_vd;
        if (spatz_req.use_vd) sb_port_vd_wd_update = SB_VSLDU_VD_WD;
      end

      if (sb_deps_valid_vs1_update) sb_d[spatz_req.vs1].valid = sb_deps_valid_vs1_update;
      if (sb_deps_valid_vs2_update) sb_d[spatz_req.vs2].valid = sb_deps_valid_vs2_update;
      if (sb_deps_valid_vd_rd_update || sb_deps_valid_vd_wd_update)
        sb_d[spatz_req.vd].valid = sb_deps_valid_vd_rd_update | sb_deps_valid_vd_wd_update;

      if (sb_deps_valid_vs1_update) sb_d[spatz_req.vs1].port = sb_port_vs1_update;
      if (sb_deps_valid_vs2_update) sb_d[spatz_req.vs2].port = sb_port_vs2_update;
      if (sb_deps_valid_vd_rd_update || sb_deps_valid_vd_wd_update)
        sb_d[spatz_req.vd].port = sb_deps_valid_vd_wd_update ? sb_port_vd_wd_update : sb_port_vd_rd_update;
    end
  end

  /////////////
  // Issuing //
  /////////////

  // Retire CSR instruction and write back result to main core.
  logic retire_csr;

  // We stall issuing a new instruction if the corresponding execution unit is
  // not ready yet. Or we have a change in LMUL, for which we need to let all the
  // units finish first before scheduling a new operation (to avoid running into
  // issues with the socreboard).
  logic stall, vfu_stall, vlsu_stall, vsldu_stall;
  assign stall        = (vfu_stall | vlsu_stall | vsldu_stall) & req_buffer_valid;
  assign vfu_stall    = ~vfu_req_ready_i   & (spatz_req.ex_unit == VFU);
  assign vlsu_stall   = ~vlsu_req_ready_i  & (spatz_req.ex_unit == LSU);
  assign vsldu_stall  = ~vsldu_req_ready_i & (spatz_req.ex_unit == SLD);

  // Pop the buffer if we do not have a unit stall
  assign req_buffer_pop = ~stall & req_buffer_valid;

  // Issue new operation to execution units
  always_comb begin : ex_issue
    retire_csr = 1'b0;

    // Define new spatz request
    spatz_req         = buffer_spatz_req;
    spatz_req_illegal = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
    spatz_req_valid   = req_buffer_pop & ~spatz_req_illegal;

    // We have a new instruction and there is no stall.
    if (spatz_req_valid) begin
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
        // Do not overwrite csrs, but retire new csr information
        retire_csr = 1'b1;
      end
    end
  end // ex_issue

  // Respond to core about the decoded instruction.
  always_comb begin : x_issue_resp
    x_issue_resp_o = '0;

    // We have a new valid instruction
    if (decoder_rsp_valid && !decoder_rsp.instr_illegal) begin
      // Accept the new instruction
      x_issue_resp_o.accept = 1'b1;

      case (decoder_rsp.spatz_req.ex_unit)
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
    end else if (decoder_rsp_valid && decoder_rsp.instr_illegal) begin
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
  assign x_issue_ready_o = req_buffer_ready;

  // Send request off to execution units
  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
