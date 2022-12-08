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
  import fpnew_pkg::roundmode_e;
  #(
    parameter int  unsigned NrVregfilePorts = 1,
    parameter int  unsigned NrWritePorts    = 1,
    parameter type          x_issue_req_t   = logic,
    parameter type          x_issue_resp_t  = logic,
    parameter type          x_result_t      = logic
  ) (
    input  logic                                clk_i,
    input  logic                                rst_ni,
    // X-Interface Issue
    input  logic                                x_issue_valid_i,
    output logic                                x_issue_ready_o,
    input  x_issue_req_t                        x_issue_req_i,
    output x_issue_resp_t                       x_issue_resp_o,
    // X-Interface Result
    output logic                                x_result_valid_o,
    input  logic                                x_result_ready_i,
    output x_result_t                           x_result_o,
    // FPU untimed sidechannel
    input  roundmode_e                          fpu_rnd_mode_i,
    // Spatz request
    output logic                                spatz_req_valid_o,
    output spatz_req_t                          spatz_req_o,
    // VFU
    input  logic                                vfu_req_ready_i,
    input  logic                                vfu_rsp_valid_i,
    output logic                                vfu_rsp_ready_o,
    input  vfu_rsp_t                            vfu_rsp_i,
    // VLSU
    input  logic                                vlsu_req_ready_i,
    input  logic                                vlsu_rsp_valid_i,
    input  vlsu_rsp_t                           vlsu_rsp_i,
    // VSLDU
    input  logic                                vsldu_req_ready_i,
    input  logic                                vsldu_rsp_valid_i,
    input  vsldu_rsp_t                          vsldu_rsp_i,
    // VRF Scoreboard
    input  logic          [NrVregfilePorts-1:0] sb_enable_i,
    input  logic          [NrWritePorts-1:0]    sb_wrote_result_i,
    output logic          [NrVregfilePorts-1:0] sb_enable_o,
    input  spatz_id_t     [NrVregfilePorts-1:0] sb_id_i
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
  `FF(vtype_q, vtype_d, '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0})

  always_comb begin : proc_vcsr
    automatic logic [$clog2(MAXVL)-1:0] vlmax = 0;

    vstart_d = vstart_q;
    vl_d     = vl_q;
    vtype_d  = vtype_q;

    if (spatz_req_valid) begin
      // Reset vstart to zero if we have a new non CSR operation
      if (spatz_req.op_cgf.reset_vstart)
        vstart_d = '0;

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
        if ((spatz_req.vtype.vsew > MAXEW) || (spatz_req.vtype.vlmul inside {LMUL_RES, LMUL_F8}) || (signed'(spatz_req.vtype.vlmul) + signed'($clog2(ELEN)) < signed'(spatz_req.vtype.vsew))) begin
          // Invalid
          vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
          vl_d    = '0;
        end else begin
          // Valid

          // Set new vtype
          vtype_d = spatz_req.vtype;
          if (!spatz_req.op_cgf.keep_vl) begin
            // Normal stripmining mode or set to MAXVL
            vlmax = VLENB >> spatz_req.vtype.vsew;

            unique case (spatz_req.vtype.vlmul)
              LMUL_F2: vlmax >>= 1;
              LMUL_F4: vlmax >>= 2;
              LMUL_1 : vlmax <<= 0;
              LMUL_2 : vlmax <<= 1;
              LMUL_4 : vlmax <<= 2;
              LMUL_8 : vlmax <<= 3;
              default: vlmax = vlmax;
            endcase

            vl_d = (spatz_req.rs1 == '1) ? (MAXVL >> spatz_req.vtype.vsew) : (int'(vlmax) < spatz_req.rs1) ? vlmax : spatz_req.rs1;
          end else begin
            // Keep vl mode

            // If new vtype changes ration, mark as illegal
            if (($signed(spatz_req.vtype.vsew) - $signed(vtype_q.vsew)) != ($signed(spatz_req.vtype.vlmul) - $signed(vtype_q.vlmul))) begin
              vtype_d = '{vill: 1'b1, vsew: EW_8, vlmul: LMUL_1, default: '0};
              vl_d    = '0;
            end
          end
        end
      end // spatz_req.op == VCFG
    end // spatz_req_valid
  end

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
    .clk_i              (clk_i            ),
    .rst_ni             (rst_ni           ),
    // Request
    .decoder_req_i      (decoder_req      ),
    .decoder_req_valid_i(decoder_req_valid),
    // Response
    .decoder_rsp_o      (decoder_rsp      ),
    .decoder_rsp_valid_o(decoder_rsp_valid),
    // FPU untimed sidechannel
    .fpu_rnd_mode_i     (fpu_rnd_mode_i   )
  );

  // Decode new instruction if new request arrives
  always_comb begin : proc_decode
    decoder_req       = '0;
    decoder_req_valid = 1'b0;

    // Decode new instruction if one is received and spatz is ready
    if (x_issue_valid_i && x_issue_ready_o) begin
      decoder_req.instr     = x_issue_req_i.instr;
      decoder_req.rs1       = x_issue_req_i.rs[0];
      decoder_req.rs1_valid = x_issue_req_i.rs_valid[0];
      decoder_req.rs2       = x_issue_req_i.rs[1];
      decoder_req.rs2_valid = x_issue_req_i.rs_valid[1];
      decoder_req.rsd       = x_issue_req_i.rs[2];
      decoder_req.rsd_valid = x_issue_req_i.rs_valid[2];
      decoder_req.rd        = x_issue_req_i.id;
      decoder_req_valid     = 1'b1;
    end
  end // proc_decode

  ////////////////////
  // Request Buffer //
  ////////////////////

  // Spatz request
  spatz_req_t buffer_spatz_req;
  // Buffer state signals
  logic       req_buffer_ready, req_buffer_valid, req_buffer_pop;

  // One element wide instruction buffer
  fall_through_register #(
    .T(spatz_req_t)
  ) i_req_buffer (
    .clk_i     (clk_i                ),
    .rst_ni    (rst_ni               ),
    .clr_i     (1'b0                 ),
    .testmode_i(1'b0                 ),
    .ready_o   (req_buffer_ready     ),
    .valid_o   (req_buffer_valid     ),
    .data_i    (decoder_rsp.spatz_req),
    .valid_i   (decoder_rsp_valid    ),
    .data_o    (buffer_spatz_req     ),
    .ready_i   (req_buffer_pop       )
  );

  ////////////////
  // Scoreboard //
  ////////////////

  // Which instruction is reading and writing to each vector register?
  struct packed {
    spatz_id_t id;
    logic valid;
  } [NRVREG-1:0] read_table_d, read_table_q, write_table_d, write_table_q;

  `FF(read_table_q, read_table_d, '{default: '0})
  `FF(write_table_q, write_table_d, '{default: '0})

  // Port scoreboard. Keeps track of the dependencies of this instruction with other instructions
  typedef struct packed {
    logic [NrParallelInstructions-1:0] deps;
    logic prevent_chaining; // Prevent chaining with some "risky" instructions
  } scoreboard_metadata_t;

  scoreboard_metadata_t [NrParallelInstructions-1:0] scoreboard_q, scoreboard_d;
  `FF(scoreboard_q, scoreboard_d, '0)

  // Did the instruction write to the VRF in the previous cycle?
  logic [NrParallelInstructions-1:0] wrote_result_q, wrote_result_d;
  `FF(wrote_result_q, wrote_result_d, '0)

  // Is this instruction a narrowing or widening instruction?
  logic [NrParallelInstructions-1:0] narrow_wide_q, narrow_wide_d;
  `FF(narrow_wide_q, narrow_wide_d, '0)

  // Did this narrowing instruction write to the VRF in the previous cycle?
  logic [NrParallelInstructions-1:0] wrote_result_narrowing_q, wrote_result_narrowing_d;
  `FF(wrote_result_narrowing_q, wrote_result_narrowing_d, '0)

  always_comb begin : scoreboard
    // Maintain stated
    read_table_d             = read_table_q;
    write_table_d            = write_table_q;
    scoreboard_d             = scoreboard_q;
    narrow_wide_d            = narrow_wide_q;
    wrote_result_narrowing_d = wrote_result_narrowing_q;

    // Nobody wrote to the VRF yet
    wrote_result_d = '0;
    sb_enable_o    = '0;

    for (int unsigned port = 0; port < NrVregfilePorts; port++)
      // Enable the VRF port if the dependant instructions wrote in the previous cycle
      sb_enable_o[port] = sb_enable_i[port] && &(~scoreboard_q[sb_id_i[port]].deps | wrote_result_q) && (!(|scoreboard_q[sb_id_i[port]].deps) || !scoreboard_q[sb_id_i[port]].prevent_chaining);

    // Store the decisions
    if (sb_enable_o[SB_VFU_VD_WD]) begin
      wrote_result_narrowing_d[sb_id_i[SB_VFU_VD_WD]] = sb_wrote_result_i[SB_VFU_VD_WD - SB_VFU_VD_WD] ^ narrow_wide_q[sb_id_i[SB_VFU_VD_WD]];
      wrote_result_d[sb_id_i[SB_VFU_VD_WD]]           = sb_wrote_result_i[SB_VFU_VD_WD - SB_VFU_VD_WD] && (!narrow_wide_q[sb_id_i[SB_VFU_VD_WD] || wrote_result_narrowing_q[sb_id_i[SB_VFU_VD_WD]]]);
    end
    if (sb_enable_o[SB_VLSU_VD_WD]) begin
      wrote_result_narrowing_d[sb_id_i[SB_VLSU_VD_WD]] = sb_wrote_result_i[SB_VLSU_VD_WD - SB_VFU_VD_WD] ^ narrow_wide_q[sb_id_i[SB_VLSU_VD_WD]];
      wrote_result_d[sb_id_i[SB_VLSU_VD_WD]]           = sb_wrote_result_i[SB_VLSU_VD_WD - SB_VFU_VD_WD] && (!narrow_wide_q[sb_id_i[SB_VLSU_VD_WD] || wrote_result_narrowing_q[sb_id_i[SB_VLSU_VD_WD]]]);
    end
    if (sb_enable_o[SB_VSLDU_VD_WD]) begin
      wrote_result_narrowing_d[sb_id_i[SB_VSLDU_VD_WD]] = sb_wrote_result_i[SB_VSLDU_VD_WD - SB_VFU_VD_WD] ^ narrow_wide_q[sb_id_i[SB_VSLDU_VD_WD]];
      wrote_result_d[sb_id_i[SB_VSLDU_VD_WD]]           = sb_wrote_result_i[SB_VSLDU_VD_WD - SB_VFU_VD_WD] && (!narrow_wide_q[sb_id_i[SB_VSLDU_VD_WD] || wrote_result_narrowing_q[sb_id_i[SB_VSLDU_VD_WD]]]);
    end

    // A unit has finished its VRF access. Reset the scoreboard. For each instruction, check
    // if a dependency existed. If so, invalidate it.
    if (vfu_rsp_valid_i) begin
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (read_table_q[vreg].id == vfu_rsp_i.id && read_table_q[vreg].valid)
          read_table_d[vreg] = '0;
        if (write_table_q[vreg].id == vfu_rsp_i.id && write_table_q[vreg].valid)
          write_table_d[vreg] = '0;
      end

      scoreboard_d[vfu_rsp_i.id]             = '0;
      narrow_wide_d[vfu_rsp_i.id]            = 1'b0;
      wrote_result_narrowing_d[vfu_rsp_i.id] = 1'b0;
      for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
        scoreboard_d[insn].deps[vfu_rsp_i.id] = 1'b0;
    end
    if (vlsu_rsp_valid_i) begin
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (read_table_q[vreg].id == vlsu_rsp_i.id && read_table_q[vreg].valid)
          read_table_d[vreg] = '0;
        if (write_table_q[vreg].id == vlsu_rsp_i.id && write_table_q[vreg].valid)
          write_table_d[vreg] = '0;
      end

      scoreboard_d[vlsu_rsp_i.id]             = '0;
      narrow_wide_d[vlsu_rsp_i.id]            = 1'b0;
      wrote_result_narrowing_d[vlsu_rsp_i.id] = 1'b0;
      for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
        scoreboard_d[insn].deps[vlsu_rsp_i.id] = 1'b0;
    end
    if (vsldu_rsp_valid_i) begin
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (read_table_q[vreg].id == vsldu_rsp_i.id && read_table_q[vreg].valid)
          read_table_d[vreg] = '0;
        if (write_table_q[vreg].id == vsldu_rsp_i.id && write_table_q[vreg].valid)
          write_table_d[vreg] = '0;
      end

      scoreboard_d[vsldu_rsp_i.id]             = '0;
      narrow_wide_d[vsldu_rsp_i.id]            = 1'b0;
      wrote_result_narrowing_d[vsldu_rsp_i.id] = 1'b0;
      for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
        scoreboard_d[insn].deps[vsldu_rsp_i.id] = 1'b0;
    end

    // Initialize the scoreboard metadata if we have a new instruction issued.
    if (spatz_req_valid && spatz_req.ex_unit != CON) begin
      // RAW hazard
      if (spatz_req.use_vs2) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.vs2].id] |= write_table_d[spatz_req.vs2].valid;
        read_table_d[spatz_req.vs2] = {spatz_req.id, 1'b1};
      end
      if (spatz_req.use_vs1) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.vs1].id] |= write_table_d[spatz_req.vs1].valid;
        read_table_d[spatz_req.vs1] = {spatz_req.id, 1'b1};
      end
      if (spatz_req.vd_is_src) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.vd].id] |= write_table_d[spatz_req.vd].valid;
        read_table_d[spatz_req.vd] = {spatz_req.id, 1'b1};
      end

      // WAW and WAR hazards
      if (spatz_req.use_vd) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.vd].id] |= write_table_d[spatz_req.vd].valid;
        scoreboard_d[spatz_req.id].deps[read_table_d[spatz_req.vd].id] |= read_table_d[spatz_req.vd].valid;
        write_table_d[spatz_req.vd] = {spatz_req.id, 1'b1};
      end

      // Is this a risky instruction which should not chain?
      if (spatz_req.op == VSLIDEUP)
        scoreboard_d[spatz_req.id].prevent_chaining = 1'b1;

      // Is this a narrowing or widening instruction?
      if (spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2)
        narrow_wide_d[spatz_req.id] = 1'b1;
    end

    // An instruction never depends on itself
    for (int insn = 0; insn < NrParallelInstructions; insn++)
      scoreboard_d[insn].deps[insn] = 1'b0;
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
  assign stall       = (vfu_stall | vlsu_stall | vsldu_stall) & req_buffer_valid;
  assign vfu_stall   = ~vfu_req_ready_i & (spatz_req.ex_unit == VFU);
  assign vlsu_stall  = ~vlsu_req_ready_i & (spatz_req.ex_unit == LSU);
  assign vsldu_stall = ~vsldu_req_ready_i & (spatz_req.ex_unit == SLD);

  // Running instructions
  logic      [NrParallelInstructions-1:0] running_insn_d, running_insn_q;
  spatz_id_t                              next_insn_id;
  logic                                   running_insn_full;
  `FF(running_insn_q, running_insn_d, '0)

  find_first_one #(
    .WIDTH(NrParallelInstructions)
  ) i_ffo_next_insn_id (
    .in_i       (~running_insn_q  ),
    .first_one_o(next_insn_id     ),
    .no_ones_o  (running_insn_full)
  );

  // Pop the buffer if we do not have a unit stall
  assign req_buffer_pop = ~stall & req_buffer_valid && !running_insn_full;

  // Issue new operation to execution units
  always_comb begin : ex_issue
    retire_csr = 1'b0;

    // Define new spatz request
    spatz_req         = buffer_spatz_req;
    spatz_req.id      = next_insn_id;
    spatz_req_illegal = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
    spatz_req_valid   = req_buffer_pop && !spatz_req_illegal && !running_insn_full;

    // We have a new instruction and there is no stall.
    if (spatz_req_valid) begin
      case (spatz_req.ex_unit)
        VFU: begin
          // Overwrite all csrs in request
          spatz_req.vtype  = vtype_q;
          spatz_req.vl     = spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2 ? vl_q * 2 : vl_q;
          spatz_req.vstart = vstart_q;

          // Is this a scalar request?
          if (spatz_req.op_arith.is_scalar) begin
            spatz_req.vtype  = buffer_spatz_req.vtype;
            spatz_req.vl     = 1;
            spatz_req.vstart = '0;
          end
        end

        LSU: begin
          // Overwrite vl and vstart in request (preserve vtype with vsew)
          spatz_req.vl     = vl_q;
          spatz_req.vstart = vstart_q;
        end

        SLD: begin
          // Overwrite all csrs in request
          spatz_req.vtype  = vtype_q;
          spatz_req.vl     = vl_q;
          spatz_req.vstart = vstart_q;
        end

        default: begin
          // Do not overwrite csrs, but retire new csr information
          retire_csr = 1'b1;
        end
      endcase
    end
  end // ex_issue

  always_comb begin: proc_next_insn_id
    // Maintain state
    running_insn_d = running_insn_q;

    // New instruction!
    if (spatz_req_valid && spatz_req.ex_unit != CON)
      running_insn_d[next_insn_id] = 1'b1;

    // Finished a instruction
    if (vfu_rsp_valid_i)
      running_insn_d[vfu_rsp_i.id] = 1'b0;
    if (vlsu_rsp_valid_i)
      running_insn_d[vlsu_rsp_i.id] = 1'b0;
    if (vsldu_rsp_valid_i)
      running_insn_d[vsldu_rsp_i.id] = 1'b0;
  end: proc_next_insn_id

  // Respond to core about the decoded instruction.
  always_comb begin : x_issue_resp
    x_issue_resp_o = '0;

    // Is there something running on Spatz? If so, prevent Snitch from reading the fcsr register
    x_issue_resp_o.float = |running_insn_q;

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

  vfu_rsp_t vfu_rsp;
  logic     vfu_rsp_valid;
  logic     vfu_rsp_ready;

  stream_register #(
    .T(vfu_rsp_t)
  ) i_vfu_scalar_response (
    .clk_i     (clk_i                          ),
    .rst_ni    (rst_ni                         ),
    .clr_i     (1'b0                           ),
    .testmode_i(1'b0                           ),
    .data_i    (vfu_rsp_i                      ),
    .valid_i   (vfu_rsp_valid_i && vfu_rsp_i.wb),
    .ready_o   (vfu_rsp_ready_o                ),
    .data_o    (vfu_rsp                        ),
    .valid_o   (vfu_rsp_valid                  ),
    .ready_i   (vfu_rsp_ready                  )
  );

  // Retire an operation/instruction and write back result to core
  // if necessary.
  always_comb begin : retire
    x_result_o       = '0;
    x_result_valid_o = '0;

    vfu_rsp_ready = 1'b0;

    if (retire_csr) begin
      // Read CSR and write back to cpu
      if (spatz_req.op == VCSR) begin
        if (spatz_req.use_rd) begin
          unique case (spatz_req.op_csr.addr)
            riscv_instr::CSR_VSTART: x_result_o.data = elen_t'(vstart_q);
            riscv_instr::CSR_VL    : x_result_o.data = elen_t'(vl_q);
            riscv_instr::CSR_VTYPE : x_result_o.data = elen_t'(vtype_q);
            riscv_instr::CSR_VLENB : x_result_o.data = elen_t'(VLENB);
            riscv_instr::CSR_VXSAT : x_result_o.data = '0;
            riscv_instr::CSR_VXRM  : x_result_o.data = '0;
            riscv_instr::CSR_VCSR  : x_result_o.data = '0;
            default: x_result_o.data                 = '0;
          endcase
        end
        x_result_o.id    = spatz_req.rd;
        x_result_o.we    = 1'b1;
        x_result_valid_o = 1'b1;
      end else begin
        // Change configuration and send back vl
        x_result_o.id    = spatz_req.rd;
        x_result_o.data  = elen_t'(vl_d);
        x_result_o.we    = 1'b1;
        x_result_valid_o = 1'b1;
      end
    end else if (vfu_rsp_valid) begin
      x_result_o.id    = vfu_rsp.rd;
      x_result_o.data  = vfu_rsp.result;
      x_result_o.we    = 1'b1;
      x_result_valid_o = 1'b1;
      vfu_rsp_ready    = 1'b1;
    end
  end // retire

  // Signal to core that Spatz is ready for a new instruction
  assign x_issue_ready_o = req_buffer_ready;

  // Send request off to execution units
  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
