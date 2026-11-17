// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The controller contains all of the CSR registers, the instruction decoder,
// the operation issuer, the register scoreboard, and the result write back to
// the main core.

module spatz_controller
  import spatz_pkg::*;
  import rvv_pkg::*;
`ifdef VENTAGLIO
  import vtl_pkg::*;
`endif
  import fpnew_pkg::roundmode_e;
  import fpnew_pkg::fmt_mode_t;
  #(
    parameter int  unsigned NrVregfilePorts   = 1,
    parameter int  unsigned NrWritePorts      = 1,
    parameter bit           RegisterRsp       = 0,
    parameter type          spatz_issue_req_t = logic,
    parameter type          spatz_issue_rsp_t = logic,
    parameter type          spatz_rsp_t       = logic,
    // implicit setting
    parameter int  unsigned NrReadPorts      = NrVregfilePorts - NrWritePorts
  ) (
    input  logic                                   clk_i,
    input  logic                                   rst_ni,
    // Snitch Issue
    input  logic                                   issue_valid_i,
    output logic                                   issue_ready_o,
    input  spatz_issue_req_t                       issue_req_i,
    output spatz_issue_rsp_t                       issue_rsp_o,
    output logic                                   rsp_valid_o,
    input  logic                                   rsp_ready_i,
    output spatz_rsp_t                             rsp_o,
    // FPU untimed sidechannel
    input  roundmode_e                             fpu_rnd_mode_i,
    input  fmt_mode_t                              fpu_fmt_mode_i,
    // Spatz request
    output logic                                   spatz_req_valid_o,
    output spatz_req_t                             spatz_req_o,
    // Per-instruction permission to start a queued matrix operation.
    output logic [NrParallelInstructions-1:0]        matrix_enable_o,
    // VFU
    input  logic                                   vfu_req_ready_i,
    input  logic                                   vfu_rsp_valid_i,
    output logic                                   vfu_rsp_ready_o,
    input  vfu_rsp_t                               vfu_rsp_i,
    // OPE
    input  logic                                   ope_req_ready_i,
    input  logic                                   ope_rsp_valid_i,
    output logic                                   ope_rsp_ready_o,
    input  ope_rsp_t                               ope_rsp_i,
    // VLSU
    input  logic                                   vlsu_req_ready_i,
    input  logic                                   vlsu_rsp_valid_i,
    input  vlsu_rsp_t                              vlsu_rsp_i,
    // VSLDU
    input  logic                                   vsldu_req_ready_i,
    input  logic                                   vsldu_rsp_valid_i,
    input  vsldu_rsp_t                             vsldu_rsp_i,
`ifdef VENTAGLIO
    // VTL (Ventaglio) — separate control path so VTL and VSLDU can share
    // the same physical VRF master port while keeping admit/retire
    // handshakes distinct. `vtl_rsp_*` only fires for vventclr; vfx ops
    // (ex_unit=VFU, op_vtl.use_vtl=1) retire via vfu_rsp.
    input  logic                                   vtl_req_ready_i,
    input  logic                                   vtl_rsp_valid_i,
    input  vsldu_rsp_t                             vtl_rsp_i,
`endif
    // VRF Scoreboard
    input  logic             [NrVregfilePorts-1:0]              sb_enable_i,
    input  logic             [NrWritePorts-1:0]                 sb_wrote_result_i,
    output logic             [NrVregfilePorts-1:0]              sb_enable_o,
    input  spatz_id_t        [NrVregfilePorts-1:0]              sb_id_i
`ifdef VENTAGLIO
    ,
    output logic             [NrVregfilePorts-NrWritePorts-1:0] sb_vtl_redirect_read_o,
    output logic             [NrWritePorts-1:0]                 sb_vtl_redirect_write_o,
    // Per-vreg "writer in flight" — combinational from write_table_q.valid
    // (which is cleared on retire). Exposed so the Ventaglio prefetch can
    // safely fire as soon as the next op's index vreg has no pending write.
    output logic             [NRVREG-1:0]                       vreg_write_pending_o
`endif
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
`ifdef VENTAGLIO
  logic       spatz_req_vtl_illegal;
`endif

  //////////
  // CSRs //
  //////////
  typedef logic [NRVREG-1:0] vid_t;

  // CSR registers
  vlen_t   vstart_d,  vstart_q;
  vlen_t   vl_d,      vl_q;
  vtype_t  vtype_d,   vtype_q;
`ifdef VENTAGLIO
  logic    vtl_en_d,  vtl_en_q;     // VTL extension enable (1: VTL enabled; 0: VTL disabled)
  vid_t    VTLVreg_d, VTLVreg_q;    // Bit mask for register mapping in VTL (1: Vreg mapped to VTL)
  sp_cfg_t VTL_cfg_d, VTL_cfg_q;    // Formats
`endif

  `FF(vstart_q,  vstart_d,  '0)
  `FF(vl_q,      vl_d,      '0)
  `FF(vtype_q,   vtype_d,   '{vill: 1'b1, altfmt: 1'b0, vsew: EW_8, vlmul: LMUL_1, default: '0})
`ifdef VENTAGLIO
  `FF(vtl_en_q,  vtl_en_d, 1'b0)     // VTL extension enable
  `FF(VTLVreg_q, VTLVreg_d, '0)     // VTL register setting
  `FF(VTL_cfg_q, VTL_cfg_d, '0)
`endif

  elen_t  tn_d, tn_q;         // tn: mirrors vl
  mtype_t mtype_d,  mtype_q;  // mtype CSR: mtwiden + tm + tk

  `FF(tn_q,     tn_d,     '0)
  `FF(mtype_q,  mtype_d,  '0)


  ////////////////////////////
  // msetmtype configuration //
  ////////////////////////////
  // Matrix LMUL and the tm bound only depend on SEW and the widening factor.
  // They are tabulated at elaboration time instead of being computed with
  // dividers and multipliers in the issue cycle.
  typedef struct packed {
    logic        valid;  // TEW matches the accumulator element and LMUL is legal
    vlmul_e      vlmul;  // matrix LMUL
    logic [13:0] tm_max; // min(LMUL * EVE, ETE)
  } mset_cfg_t;
  typedef mset_cfg_t [7:0][3:0] mset_lut_t;

  function automatic mset_lut_t gen_mset_lut();
    mset_lut_t lut;
    for (int unsigned sew = 0; sew < 8; sew++) begin
      for (int unsigned widen = 0; widen < 4; widen++) begin
        int unsigned twiden, tewb, kmax, ete, eve, lmul_req, lmul_k, lmul_widen, lmul_eve;
        lut[sew][widen] = '{valid: 1'b0, vlmul: LMUL_1, tm_max: '0};
        if (widen != 0) begin
          twiden     = 1 << (widen - 1);
          tewb       = (1 << sew) * twiden;
          kmax       = (sew == 0) ? 4 : (sew == 1) ? 2 : 1;
          ete        = (tewb < 8) ? TileEdge : (TileEdge >> 1);
          eve        = VLENB >> sew;
          if ((tewb == AccElemBytes) && (eve != 0)) begin
            lmul_req   = (ete + eve - 1) / eve;
            lmul_k     = 8 / kmax;
            lmul_widen = 8 / twiden;
            lmul_req   = (lmul_req < lmul_k) ? lmul_req : lmul_k;
            lmul_req   = (lmul_req < lmul_widen) ? lmul_req : lmul_widen;
            lut[sew][widen].valid = 1'b1;
            unique case (lmul_req)
              1:       lut[sew][widen].vlmul = LMUL_1;
              2:       lut[sew][widen].vlmul = LMUL_2;
              4:       lut[sew][widen].vlmul = LMUL_4;
              8:       lut[sew][widen].vlmul = LMUL_8;
              default: lut[sew][widen].valid = 1'b0;
            endcase
            lmul_eve = eve * lmul_req;
            lut[sew][widen].tm_max = 14'((lmul_eve < ete) ? lmul_eve : ete);
          end
        end
      end
    end
    return lut;
  endfunction

  localparam mset_lut_t MsetLut = gen_mset_lut();

  mset_cfg_t  mset_lut_entry;
  logic       mset_vtype_valid;
  logic [3:0] mset_kmax;

  // Legality of the requested vtype plus the tabulated matrix configuration
  assign mset_lut_entry   = MsetLut[spatz_req.vtype.vsew][spatz_req.mtype.mtwiden];
  assign mset_vtype_valid =
      !(spatz_req.vtype.vill ||
        (spatz_req.vtype.vsew > MAXEW) ||
        (spatz_req.vtype.vlmul inside {LMUL_RES, LMUL_F8}) ||
        (signed'(spatz_req.vtype.vlmul) + signed'($clog2(ELEN)) < signed'(spatz_req.vtype.vsew))) &&
      ((spatz_req.mtype.mtwiden == 2'b00) || mset_lut_entry.valid);
  assign mset_kmax = (spatz_req.vtype.vsew == EW_8)  ? 4'd4 :
                     (spatz_req.vtype.vsew == EW_16) ? 4'd2 : 4'd1;

  always_comb begin : proc_vcsr
    logic [$clog2(MAXVL):0] vlmax;

    vlmax = '0;

    vstart_d   = vstart_q;
    vl_d       = vl_q;
    vtype_d    = vtype_q;
`ifdef VENTAGLIO
    vtl_en_d   = vtl_en_q;   // VTL extension enable
    VTLVreg_d  = VTLVreg_q;
    VTL_cfg_d  = VTL_cfg_q;
`endif

    if (spatz_req_valid) begin
      // Reset vstart to zero if we have a new non CSR operation
      if (spatz_req.op_cfg.reset_vstart)
        vstart_d = '0;

      // Set new vstart if we have a VCSR instruction
      // that accesses the vstart register.
      if (spatz_req.op == VCSR) begin
        if (spatz_req.op_cfg.write_vstart) begin
          vstart_d = vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cfg.set_vstart) begin
          vstart_d = vstart_q | vlen_t'(spatz_req.rs1);
        end else if (spatz_req.op_cfg.clear_vstart) begin
          vstart_d = vstart_q & ~vlen_t'(spatz_req.rs1);
`ifdef VENTAGLIO
        end else if (spatz_req.op_cfg.vtl_redirect) begin // For the VTL extensions
          if (vtl_en_q && (VTLVreg_q == vid_t'(spatz_req.rs1))) begin
            vtl_en_d            = 1'b0;                   // Disable if the same register is written again
            VTLVreg_d           =   '0;
          end else begin
            vtl_en_d  = 1'b1;
            VTLVreg_d = vid_t'(spatz_req.rs1);            // Set which registers are mapped to VTL
          end
        end else if (spatz_req.op_cfg.set_vtl_index_width) begin
          VTL_cfg_d.sp_cfg_index_width = sp_idxw_e'(spatz_req.rs1);
        end else if (spatz_req.op_cfg.set_vtl_blk_size) begin
          VTL_cfg_d.sp_cfg_blk_size    = sp_blk_e'(spatz_req.rs1);
        end else if (spatz_req.op_cfg.set_vtl_ratio) begin
          VTL_cfg_d.sp_cfg_ratio       = sp_ratio_e'(spatz_req.rs1);
`endif
        end
      end // spatz_req.op == VCSR

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
          if (!spatz_req.op_cfg.keep_vl) begin
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

            vl_d = (int'(vlmax) < spatz_req.rs1) ? vlmax : spatz_req.rs1;
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

      // Matrix configuration instructions that modify vector CSR state.
      else if (spatz_req.op == VMCFG) begin
        unique case (spatz_req.op_ope.cfg_sel)

          // msetmtype / msetmtypei: vl = 0, update vtype, calculate matrix LMUL
          2'b00: begin : msetmtype_vcsr
            // Both configuration forms supply altfmt through the decoded vtype.
            // Keep it while deriving matrix LMUL and the agnostic policy bits.
            vtype_d = spatz_req.vtype;
            if (!mset_vtype_valid) begin
              vtype_d = '{vill: 1'b1, altfmt: 1'b0, vsew: EW_8, vlmul: LMUL_1, default: '0};
            end else if (spatz_req.mtype.mtwiden != 2'b00) begin
              vtype_d.vlmul = mset_lut_entry.vlmul;
              vtype_d.vma   = 1'b1;
              vtype_d.vta   = 1'b1;
            end

            vl_d = '0;  // always writes vl = 0.
          end
          // msettn:
          //   unconfigured: vl = min(rs1, LMUL * EVE)
          //   configured:   vl = min(rs1, LMUL * EVE, ETE)
          2'b01: begin : msettn_vcsr
            int unsigned lmul_eve, tmp_tn;
            int unsigned ete;
            int unsigned tewb;

            if (vtype_q.vill) begin
              vl_d = '0;
            end else begin
              lmul_eve = VLENB >> int'(vtype_q.vsew);
              unique case (vtype_q.vlmul)
                LMUL_F2: lmul_eve >>= 1;
                LMUL_F4: lmul_eve >>= 2;
                LMUL_1 : lmul_eve <<= 0;
                LMUL_2 : lmul_eve <<= 1;
                LMUL_4 : lmul_eve <<= 2;
                LMUL_8 : lmul_eve <<= 3;
                default: lmul_eve = '0;
              endcase

              if (mtype_q.mtwiden == 2'b00) begin
                vl_d = (spatz_req.rs1 > elen_t'(lmul_eve)) ? vlen_t'(lmul_eve) : vlen_t'(spatz_req.rs1);
              end else begin

                tewb = (1 << int'(vtype_q.vsew)) *
                       (1 << (mtype_q.mtwiden - 1));
                ete = (tewb < 8) ? TileEdge : TileEdge >> 1;

                tmp_tn  = (lmul_eve < ete) ? lmul_eve : ete;
                vl_d    = (elen_t'(tmp_tn) < spatz_req.rs1) ? vlen_t'(tmp_tn) : vlen_t'(spatz_req.rs1);
              end
            end
          end

          // msettm / msettk only update mtype in proc_mcsr. They do not modify vector CSR state.
          2'b10, 2'b11: begin
          end
        endcase
      end
    end // spatz_req_valid
  end

  always_comb begin : proc_mcsr
    tn_d = tn_q;
    mtype_d  = mtype_q;

    if (spatz_req_valid) begin
      // RVV vset[i]vl[i] leaves the matrix unit unconfigured. Software must
      // execute msetmtype[i] again before issuing another matrix operation.
      if (spatz_req.op == VCFG) begin
        mtype_d = '0;
        tn_d    = '0;
      end else if (spatz_req.op == VMCFG) begin
        unique case (spatz_req.op_ope.cfg_sel)

          // msetmtype / msetmtypei: write spatz_req.mtype with clamped tk/tm
          2'b00: begin : msetmtype_mcsr
              mtype_d = spatz_req.mtype;

              // Clamp tk to KMAX of the element width and tm to min(LMUL*EVE, ETE)
              if (mtype_d.tk > mset_kmax) begin
                mtype_d.tk = mset_kmax;
              end
              if (mtype_d.tm > mset_lut_entry.tm_max) begin
                mtype_d.tm = mset_lut_entry.tm_max;
              end

              // reset vl / tn
              tn_d = '0;

              mtype_d = (mtype_d.mtwiden == 2'b00 || !mset_vtype_valid) ? '0 : mtype_d;
          end
          // msettn: tn mirrors the new vl computed in proc_vcsr
          2'b01: begin
            tn_d = elen_t'(vl_d);
          end

          // msettm: mtype.tm = min(rs1, LMUL*EVE, ETE)   (0 if unconfigured)
          2'b10: begin
            if (mtype_q.mtwiden == 2'b00) begin
              mtype_d.tm = '0;
            end else begin
              int unsigned lmul_eve, requested_tm, tmp_tm;
              int unsigned ete;
              int unsigned tewb;

              lmul_eve = VLENB >> int'(vtype_q.vsew);
              unique case (vtype_q.vlmul)
                LMUL_F2: lmul_eve >>= 1;
                LMUL_F4: lmul_eve >>= 2;
                LMUL_1 : lmul_eve <<= 0;
                LMUL_2 : lmul_eve <<= 1;
                LMUL_4 : lmul_eve <<= 2;
                LMUL_8 : lmul_eve <<= 3;
                default: lmul_eve = '0;
              endcase

              tewb = (1 << int'(vtype_q.vsew)) *
                     (1 << (mtype_q.mtwiden - 1));
              ete = (tewb < 8) ? TileEdge : TileEdge >> 1;

              requested_tm = int'(spatz_req.rs1);
              tmp_tm = (lmul_eve < ete) ? lmul_eve : ete;
              mtype_d.tm = 14'((requested_tm < tmp_tm) ? requested_tm : tmp_tm);
            end
          end
          // msettk: mtype.tk = min(rs1, KMAX)   (0 if unconfigured)
          2'b11: begin
            if (mtype_q.mtwiden == 2'b0) begin
              mtype_d.tk = '0;
            end else begin
              logic [3:0] kmax_val;
              unique case (vtype_q.vsew)
                EW_8:    kmax_val = 4;
                EW_16:   kmax_val = 2;
                default: kmax_val = 1;
              endcase
              mtype_d.tk = (spatz_req.rs1 > elen_t'(kmax_val))
                           ? kmax_val : 4'(spatz_req.rs1);
            end
          end
        endcase
      end
    end
  end : proc_mcsr

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
    .fpu_rnd_mode_i     (fpu_rnd_mode_i   ),
    .fpu_fmt_mode_i     (fpu_fmt_mode_i   )
  );

  // Decode new instruction if new request arrives
  // The request data is decoded unconditionally; only the valid depends on the
  // core handshake, so the decoded fields do not wait for issue_valid_i.
  always_comb begin : proc_decode
    decoder_req       = '0;
    decoder_req.instr = issue_req_i.data_op;
    decoder_req.rs1   = issue_req_i.data_arga;
    decoder_req.rs2   = issue_req_i.data_argb;
    decoder_req.rsd   = issue_req_i.data_argc;
    decoder_req.rd    = issue_req_i.id;
    decoder_req.vtype = vtype_q;
    decoder_req_valid = issue_valid_i && issue_ready_o;
  end // proc_decode

  ////////////////////
  // Request Buffer //
  ////////////////////

  // Spatz request
  spatz_req_t buffer_spatz_req;
  // Buffer state signals
  logic       req_buffer_ready, req_buffer_valid, req_buffer_pop;

  // One element wide fall-through instruction buffer. The output data select
  // is the registered full flag (not the push handshake).
  logic       req_buffer_full_q;
  spatz_req_t req_buffer_q;
  logic       req_buffer_push;
  assign req_buffer_ready = !req_buffer_full_q;
  assign req_buffer_valid = req_buffer_full_q || decoder_rsp_valid;
  assign buffer_spatz_req = req_buffer_full_q ? req_buffer_q : decoder_rsp.spatz_req;
  assign req_buffer_push  = decoder_rsp_valid && !req_buffer_full_q && !req_buffer_pop;
  always_ff @(posedge clk_i or negedge rst_ni) begin : p_req_buffer
    if (!rst_ni) begin
      req_buffer_full_q <= 1'b0;
      req_buffer_q      <= '0;
    end else begin
      // The data register is only read when full: load it on every non-full
      // cycle (register-driven enable); only the flag sees the handshake.
      if (!req_buffer_full_q)
        req_buffer_q <= decoder_rsp.spatz_req;
      if (req_buffer_push)
        req_buffer_full_q <= 1'b1;
      else if (req_buffer_full_q && req_buffer_pop)
        req_buffer_full_q <= 1'b0;
    end
  end

  ////////////////
  // Scoreboard //
  ////////////////

  // Which instruction is reading and writing to each vector register?
  typedef struct packed {
    spatz_id_t id;
    logic valid;
  } [NRVREG-1:0] table_t;
  typedef logic [$clog2(NRVREG):0] vreg_group_idx_t;
  typedef struct packed {
    logic       valid;
    logic       aligned;
    logic [3:0] regs;
  } emul_group_t;
  typedef struct packed {
    vew_e        data_eew;
    emul_group_t data;
    emul_group_t index;
  } memory_emul_t;

  memory_emul_t request_memory_emul, issue_memory_emul;
  logic signed [4:0] request_data_emul_exp, request_index_emul_exp;
  logic signed [4:0] issue_data_emul_exp, issue_index_emul_exp;
  table_t read_table_d, read_table_q, write_table_d, write_table_q;

  `FF(read_table_q, read_table_d, '{default: '0})
  `FF(write_table_q, write_table_d, '{default: '0})

  // Port scoreboard. Keeps track of the dependencies of this instruction with other instructions
  typedef struct packed {
    logic [NrParallelInstructions-1:0] deps;
    logic prevent_chaining; // Prevent chaining with some "risky" instructions
  } scoreboard_metadata_t;

  scoreboard_metadata_t [NrParallelInstructions-1:0] scoreboard_q, scoreboard_d;
  `FF(scoreboard_q, scoreboard_d, '0)

`ifdef VENTAGLIO
  ///////////////
  // VTL Table //
  ///////////////

  // To track if the indices used for scatter/gather is already in the VTL buffer
  typedef struct packed {
    logic                          use_vtl;
    logic   [NrWritePorts-1:0]       write;
    logic   [NrReadPorts-1:0]         read;
  } vtl_table_t;
  vtl_table_t [NrParallelInstructions-1:0] vtl_table_d, vtl_table_q;
  `FF(vtl_table_q, vtl_table_d, '{default: '0})

  // For better wiring
  logic [NrParallelInstructions-1:0][NrVregfilePorts-1:0] vtl_table_wr;
  always_comb begin
    for (int id = 0; id < NrParallelInstructions; id++) begin
      vtl_table_wr[id] = {vtl_table_q[id].write, vtl_table_q[id].read};
    end
  end
`endif


  // Did the instruction write to the VRF in the previous cycle?
`ifdef DOUBLE_BW
  logic [NumVLSUInterfaces-1:0] [NrParallelInstructions-1:0] wrote_result_q, wrote_result_d;

  // Per-interface completion is sticky only after the complete half-vector
  // has reached the VRF. wrote_result_q remains the one-cycle chaining pulse.
  logic [NumVLSUInterfaces-1:0] [NrParallelInstructions-1:0] done_result_q, done_result_d;
  vlen_t [NumVLSUInterfaces-1:0] [NrParallelInstructions-1:0]
      vlsu_vl_cnt_q, vlsu_vl_cnt_d;

  // Counter to track the vlen completed for each instruction
  vlen_t [NrParallelInstructions-1:0] vl_cnt_d, vl_cnt_q, vl_max_d, vl_max_q;

  // Is this instruction a narrowing instruction?
  logic [NrParallelInstructions-1:0] narrow_q, narrow_d;

  `FF(done_result_q, done_result_d, '0)
  `FF(vlsu_vl_cnt_q, vlsu_vl_cnt_d, '0)
  `FF(vl_cnt_q, vl_cnt_d, '0)
  `FF(vl_max_q, vl_max_d, '0)
  `FF(narrow_q, narrow_d, '0)
`else
  logic [NrParallelInstructions-1:0] wrote_result_q, wrote_result_d;
`endif

  `FF(wrote_result_q, wrote_result_d, '0)

  // Is this instruction a narrowing or widening instruction?
  logic [NrParallelInstructions-1:0] narrow_wide_q, narrow_wide_d;
  `FF(narrow_wide_q, narrow_wide_d, '0)

  // Did this narrowing instruction write to the VRF in the previous cycle?
  logic [NrParallelInstructions-1:0] wrote_result_narrowing_q, wrote_result_narrowing_d;
  `FF(wrote_result_narrowing_q, wrote_result_narrowing_d, '0)

  logic [3:0]        vreg_group_count;
  logic [3:0]        mem_data_group_count;
  logic [3:0]        mem_index_group_count;

  // Register footprint of the instruction being issued (see issue_footprint)
  logic [1:0]                        sb_vs_row_shift;   // log2 of the MAC row stride
  logic [KMAX-1:0]                   sb_vs_row_en;      // active MAC rows
  logic [3:0]                        sb_vs_group_count;
  logic [NRVREG-1:0]                 issue_rd_vregs, issue_wr_vregs;
  logic [NrParallelInstructions-1:0] issue_deps;

  // Decode EMUL metadata without an automatic function.  The controller owns
  // the architectural footprint because issue legality and the centralized
  // scoreboard both need it before a request reaches the VLSU.
  always_comb begin : decode_memory_emul
    request_memory_emul = '0;
    issue_memory_emul = '0;

    request_memory_emul.data_eew = (spatz_req.op inside {VLXE, VSXE}) ?
        spatz_req.vtype.vsew : spatz_req.op_mem.ew;
    issue_memory_emul.data_eew = (decoder_rsp.spatz_req.op inside {VLXE, VSXE}) ?
        vtype_q.vsew : decoder_rsp.spatz_req.op_mem.ew;

    request_data_emul_exp = $signed(spatz_req.vtype.vlmul) +
        $signed({1'b0, request_memory_emul.data_eew}) -
        $signed({1'b0, spatz_req.vtype.vsew});
    request_index_emul_exp = $signed(spatz_req.vtype.vlmul) +
        $signed({1'b0, spatz_req.op_mem.ew}) -
        $signed({1'b0, spatz_req.vtype.vsew});
    issue_data_emul_exp = $signed(vtype_q.vlmul) +
        $signed({1'b0, issue_memory_emul.data_eew}) -
        $signed({1'b0, vtype_q.vsew});
    issue_index_emul_exp = $signed(vtype_q.vlmul) +
        $signed({1'b0, decoder_rsp.spatz_req.op_mem.ew}) -
        $signed({1'b0, vtype_q.vsew});

    unique case (request_data_emul_exp)
      -5'sd3, -5'sd2, -5'sd1, 5'sd0: request_memory_emul.data.regs = 4'd1;
      5'sd1: request_memory_emul.data.regs = 4'd2;
      5'sd2: request_memory_emul.data.regs = 4'd4;
      5'sd3: request_memory_emul.data.regs = 4'd8;
      default: request_memory_emul.data.regs = 4'd0;
    endcase
    unique case (request_index_emul_exp)
      -5'sd3, -5'sd2, -5'sd1, 5'sd0: request_memory_emul.index.regs = 4'd1;
      5'sd1: request_memory_emul.index.regs = 4'd2;
      5'sd2: request_memory_emul.index.regs = 4'd4;
      5'sd3: request_memory_emul.index.regs = 4'd8;
      default: request_memory_emul.index.regs = 4'd0;
    endcase
    unique case (issue_data_emul_exp)
      -5'sd3, -5'sd2, -5'sd1, 5'sd0: issue_memory_emul.data.regs = 4'd1;
      5'sd1: issue_memory_emul.data.regs = 4'd2;
      5'sd2: issue_memory_emul.data.regs = 4'd4;
      5'sd3: issue_memory_emul.data.regs = 4'd8;
      default: issue_memory_emul.data.regs = 4'd0;
    endcase
    unique case (issue_index_emul_exp)
      -5'sd3, -5'sd2, -5'sd1, 5'sd0: issue_memory_emul.index.regs = 4'd1;
      5'sd1: issue_memory_emul.index.regs = 4'd2;
      5'sd2: issue_memory_emul.index.regs = 4'd4;
      5'sd3: issue_memory_emul.index.regs = 4'd8;
      default: issue_memory_emul.index.regs = 4'd0;
    endcase

    request_memory_emul.data.valid = (spatz_req.vtype.vlmul != LMUL_RES) &&
        (request_memory_emul.data.regs != 0);
    request_memory_emul.index.valid = (spatz_req.vtype.vlmul != LMUL_RES) &&
        (request_memory_emul.index.regs != 0);
    issue_memory_emul.data.valid = (vtype_q.vlmul != LMUL_RES) &&
        (issue_memory_emul.data.regs != 0);
    issue_memory_emul.index.valid = (vtype_q.vlmul != LMUL_RES) &&
        (issue_memory_emul.index.regs != 0);

    request_memory_emul.data.aligned = request_memory_emul.data.valid &&
        ((spatz_req.vd & vreg_t'(request_memory_emul.data.regs - 1'b1)) == '0) &&
        (({1'b0, spatz_req.vd} + request_memory_emul.data.regs) <= NRVREG);
    request_memory_emul.index.aligned = request_memory_emul.index.valid &&
        ((spatz_req.vs2 & vreg_t'(request_memory_emul.index.regs - 1'b1)) == '0) &&
        (({1'b0, spatz_req.vs2} + request_memory_emul.index.regs) <= NRVREG);
    issue_memory_emul.data.aligned = issue_memory_emul.data.valid &&
        ((decoder_rsp.spatz_req.vd &
          vreg_t'(issue_memory_emul.data.regs - 1'b1)) == '0) &&
        (({1'b0, decoder_rsp.spatz_req.vd} +
          issue_memory_emul.data.regs) <= NRVREG);
    issue_memory_emul.index.aligned = issue_memory_emul.index.valid &&
        ((decoder_rsp.spatz_req.vs2 &
          vreg_t'(issue_memory_emul.index.regs - 1'b1)) == '0) &&
        (({1'b0, decoder_rsp.spatz_req.vs2} +
          issue_memory_emul.index.regs) <= NRVREG);
  end : decode_memory_emul

  // Register-group sizes of the footprint. OPE and LSU requests always use the
  // CSR state (vtype_q), so the sizes are derived from registers and only the
  // final selection depends on the decoded request.
  logic [3:0][3:0] emul_regs_q;  // EMUL register count per EEW, from vtype_q
  always_comb begin : decode_vreg_group_count
    unique case (vtype_q.vlmul)
      LMUL_2:  vreg_group_count = 4'd2;
      LMUL_4:  vreg_group_count = 4'd4;
      LMUL_8:  vreg_group_count = 4'd8;
      default: vreg_group_count = 4'd1;
    endcase

    for (int unsigned eew = 0; eew < 4; eew++) begin
      logic signed [4:0] emul_exp;
      emul_exp = $signed(vtype_q.vlmul) + $signed(5'(eew)) - $signed({1'b0, vtype_q.vsew});
      unique case (emul_exp)
        -5'sd3, -5'sd2, -5'sd1, 5'sd0: emul_regs_q[eew] = 4'd1;
        5'sd1:   emul_regs_q[eew] = 4'd2;
        5'sd2:   emul_regs_q[eew] = 4'd4;
        5'sd3:   emul_regs_q[eew] = 4'd8;
        default: emul_regs_q[eew] = 4'd0;
      endcase
    end

    // Indexed operations encode the index EEW in op_mem.ew; their data EEW
    // remains SEW.  Other memory operations encode the data EEW directly.
    mem_data_group_count  = emul_regs_q[(spatz_req.op inside {VLXE, VSXE}) ?
                                        vtype_q.vsew[1:0] : spatz_req.op_mem.ew[1:0]];
    mem_index_group_count = emul_regs_q[spatz_req.op_mem.ew[1:0]];
  end : decode_vreg_group_count

  ///////////////////////////
  // Issue footprint (RAW/WAW/WAR) //
  ///////////////////////////
  // The vector registers read and written by the instruction being issued are
  // computed in parallel, one membership test per register, from the request
  // fields only. The dependency mask is then an OR tree over the registers.
  // This replaces a chain of dynamic-index read-modify-write updates of the
  // scoreboard (one per operand register group element), which was too deep
  // for a single cycle.

  // Operand window of the source registers vs1/vs2
  always_comb begin : issue_window_cfg
    sb_vs_row_shift   = 2'd3;
    sb_vs_row_en      = KMAX'(1);
    sb_vs_group_count = 4'd1;

    if ((spatz_req.ex_unit == OPE) && spatz_req.op_ope.is_mac) begin
      unique case (vtype_q.vsew)
        EW_8:    sb_vs_row_shift = 2'd1;
        EW_16:   sb_vs_row_shift = 2'd2;
        default: sb_vs_row_shift = 2'd3;
      endcase

      // The supported FP16->FP32 SDOTP path packs both active K rows into
      // each 32-bit element of the explicit base register.  Its datapath
      // therefore reads no implicit base+row_stride register.  Tracking
      // tk=2 here would create false RAW/WAR dependencies on v4/v20.
      // Same-width MACs retain the architectural row-stride footprint.
      for (int unsigned row = 0; row < KMAX; row++)
        sb_vs_row_en[row] = (vtype_q.vsew != vew_e'(int'(vtype_q.vsew) + int'(mtype_q.mtwiden) - 1)) ?
                            (row == 0) : (mtype_q.tk > row);
      sb_vs_group_count = vreg_group_count;
    end else if ((spatz_req.ex_unit == OPE) &&
                 spatz_req.op_ope.tss.tile_valid && spatz_req.use_vs2) begin
      sb_vs_group_count = vreg_group_count;
    end else if ((spatz_req.ex_unit == LSU) &&
                 (spatz_req.op inside {VLXE, VSXE})) begin
      // Indexed memory operations read their index vector through vs2.
      sb_vs_group_count = mem_index_group_count;
    end
  end : issue_window_cfg

  logic issue_vd_is_store_data, issue_writes_vd, issue_vd_load_group, issue_vd_tile_group;

  // Vector stores encode their data source in vd (architectural vs3) and reserve
  // their full EMUL group; vector loads and OPE tile moves write a full group.
  assign issue_vd_is_store_data = (spatz_req.ex_unit == LSU) && !spatz_req.op_mem.is_load;
  assign issue_writes_vd        = spatz_req.use_vd && !issue_vd_is_store_data;
  assign issue_vd_load_group    = (spatz_req.ex_unit == LSU) && spatz_req.op_mem.is_load;
  assign issue_vd_tile_group    = (spatz_req.ex_unit == OPE) && spatz_req.op_ope.tss.tile_valid;

  // Footprint = window pattern (from CSR registers and a few decoded bits)
  // shifted to the base register. Bits shifted beyond v31 drop out, as before.
  function automatic logic [NRVREG-1:0] group_mask(input logic [3:0] count);
    return (NRVREG'(1) << count) - 1'b1;
  endfunction

  logic [NRVREG-1:0] issue_vs_pattern, issue_vd_src_pattern, issue_vd_dst_pattern;
  always_comb begin : issue_patterns
    issue_vs_pattern = '0;
    for (int unsigned row = 0; row < KMAX; row++)
      if (sb_vs_row_en[row])
        issue_vs_pattern |= group_mask(sb_vs_group_count) << (row << sb_vs_row_shift);
    issue_vd_src_pattern = issue_vd_is_store_data ? group_mask(mem_data_group_count) : NRVREG'(1);
    issue_vd_dst_pattern = issue_vd_load_group ? group_mask(mem_data_group_count) :
                           issue_vd_tile_group ? group_mask(vreg_group_count)     : NRVREG'(1);
  end : issue_patterns

  logic [NRVREG-1:0] issue_vtl_vregs;
`ifdef VENTAGLIO
  assign issue_vtl_vregs = spatz_req.op_vtl.use_vtl ? (NRVREG'(1) << spatz_req.op_vtl.idx_vreg) : '0;
`else
  assign issue_vtl_vregs = '0;
`endif
  assign issue_rd_vregs = (spatz_req.use_vs2   ? (issue_vs_pattern     << spatz_req.vs2) : '0) |
                          (spatz_req.use_vs1   ? (issue_vs_pattern     << spatz_req.vs1) : '0) |
                          (spatz_req.vd_is_src ? (issue_vd_src_pattern << spatz_req.vd)  : '0) |
                          issue_vtl_vregs;
  assign issue_wr_vregs = issue_writes_vd ? (issue_vd_dst_pattern << spatz_req.vd) : '0;

  // RAW/WAW: wait for the current writer of every register in the footprint.
  // WAR: a written register also waits for its current reader.
  // dep_matrix[i][v]: register v makes the new instruction depend on ID i.
  logic [NrParallelInstructions-1:0][NRVREG-1:0] issue_dep_matrix;
  logic                                          issue_clear_deps;

  for (genvar i = 0; i < NrParallelInstructions; i++) begin : gen_issue_deps
    for (genvar v = 0; v < NRVREG; v++) begin : gen_vreg
      assign issue_dep_matrix[i][v] =
          ((issue_rd_vregs[v] || issue_wr_vregs[v]) && write_table_q[v].valid &&
           (write_table_q[v].id == spatz_id_t'(i))) ||
          (issue_wr_vregs[v] && read_table_q[v].valid &&
           (read_table_q[v].id == spatz_id_t'(i)));
    end
    // Same dependencies, computed by shifting the register-derived "owned by i"
    // vectors down to the operand base (in parallel with the window patterns)
    // instead of shifting the pattern up: bit j <-> register base + j; registers
    // beyond v31 are zero-filled, as before.
    logic [NRVREG-1:0] wt_i, rt_i;
    for (genvar v = 0; v < NRVREG; v++) begin : gen_owned
      assign wt_i[v] = write_table_q[v].valid && (write_table_q[v].id == spatz_id_t'(i));
      assign rt_i[v] = read_table_q[v].valid && (read_table_q[v].id == spatz_id_t'(i));
    end
    logic dep_shifted;
    assign dep_shifted =
        (spatz_req.use_vs2   && |(issue_vs_pattern     & (wt_i >> spatz_req.vs2))) ||
        (spatz_req.use_vs1   && |(issue_vs_pattern     & (wt_i >> spatz_req.vs1))) ||
        (spatz_req.vd_is_src && |(issue_vd_src_pattern & (wt_i >> spatz_req.vd)))  ||
        (issue_writes_vd     && |(issue_vd_dst_pattern & ((wt_i | rt_i) >> spatz_req.vd))) ||
        (|(issue_vtl_vregs & wt_i));
    // vlx does not track any dependency
    assign issue_deps[i] = dep_shifted && !issue_clear_deps;
`ifndef SYNTHESIS
    always_ff @(posedge clk_i) begin : check_issue_deps
      if (rst_ni && spatz_req_valid)
        assert (dep_shifted == (|issue_dep_matrix[i]))
          else $error("Issue dependency computation mismatch");
    end
`endif
  end

  assign issue_clear_deps = issue_writes_vd && (spatz_req.op inside {VLX});

`ifdef DOUBLE_BW
  // Per instruction ID and load-store interface: may a VRF port of this
  // instruction proceed? Dependencies must have written in the previous cycle
  // (chaining) or be complete; when chaining is prevented, only a completed
  // dependency may release the port. Registers only.
  logic [NrParallelInstructions-1:0][1:0] sb_id_ok;
  logic [NrParallelInstructions-1:0]      sb_id_ok_auto;
  always_comb begin : sb_port_ok
    logic [NrParallelInstructions-1:0] chainable;
    for (int unsigned id = 0; id < NrParallelInstructions; id++)
      chainable[id] = !scoreboard_q[id].prevent_chaining;
    for (int unsigned id = 0; id < NrParallelInstructions; id++) begin
      for (int unsigned intf = 0; intf < 2; intf++) begin
        sb_id_ok[id][intf] =
            &(~scoreboard_q[id].deps | ((wrote_result_q[intf] | done_result_q[intf]) & chainable)) &&
            (!scoreboard_q[id].prevent_chaining || !(|scoreboard_q[id].deps) ||
             &(~scoreboard_q[id].deps | done_result_q[intf]));
      end
      // Non-VLSU ports: interface 0 until the first half of the vector is done
      sb_id_ok_auto[id] = (vl_cnt_q[id] < vl_max_q[id]) ? sb_id_ok[id][0] : sb_id_ok[id][1];
    end
  end : sb_port_ok
`endif

  always_comb begin : scoreboard
    logic [NrParallelInstructions-1:0] chainable_writer;
    logic [NrParallelInstructions-1:0] retired_mask;

    // Clear completed owners from every dependency row.
    retired_mask = '0;
    if (vfu_rsp_valid_i)   retired_mask[vfu_rsp_i.id] = 1'b1;
    if (ope_rsp_valid_i)   retired_mask[ope_rsp_i.id] = 1'b1;
    if (vlsu_rsp_valid_i)  retired_mask[vlsu_rsp_i.id] = 1'b1;
    if (vsldu_rsp_valid_i) retired_mask[vsldu_rsp_i.id] = 1'b1;
`ifdef VENTAGLIO
    if (vtl_rsp_valid_i)   retired_mask[vtl_rsp_i.id] = 1'b1;
`endif

    // Maintain stated
    read_table_d             = read_table_q;
    write_table_d            = write_table_q;
    scoreboard_d             = scoreboard_q;
    narrow_wide_d            = narrow_wide_q;
    wrote_result_narrowing_d = wrote_result_narrowing_q;
`ifdef VENTAGLIO
    vtl_table_d              = vtl_table_q;
    sb_vtl_redirect_read_o    = '0;
    sb_vtl_redirect_write_o   = '0;
`endif

    // Nobody wrote to the VRF yet
    wrote_result_d = '0;
    sb_enable_o    = '0;

    for (int unsigned id = 0; id < NrParallelInstructions; id++)
      chainable_writer[id] = !scoreboard_q[id].prevent_chaining;

`ifdef DOUBLE_BW
    done_result_d        = done_result_q;
    vlsu_vl_cnt_d        = vlsu_vl_cnt_q;
    narrow_d             = narrow_q;
    vl_cnt_d             = vl_cnt_q;
    vl_max_d             = vl_max_q;
`endif

    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
`ifdef DOUBLE_BW
      // VLSU ports use the write status of their own interface; the other ports
      // use the interface selected by the instruction's progress (sb_port_ok).
      // The per-ID conditions come from registers only, so the port enable is a
      // single-bit select by sb_id_i.
      if (port inside {SB_VLSU_VS2_RD0, SB_VLSU_VD_RD0, SB_VLSU_VD_WD0})
        sb_enable_o[port] = sb_enable_i[port] && sb_id_ok[sb_id_i[port]][0];
      else if (port inside {SB_VLSU_VS2_RD1, SB_VLSU_VD_RD1, SB_VLSU_VD_WD1})
        sb_enable_o[port] = sb_enable_i[port] && sb_id_ok[sb_id_i[port]][1];
      else if (port inside {SB_VFU_VS2_RD, SB_VFU_VS1_RD, SB_VFU_VD_RD})
        // VFU/OPE reads can reach the next word before this instruction writes
        // its first result. Write progress cannot identify the word being read.
        sb_enable_o[port] = sb_enable_i[port] && !(|scoreboard_q[sb_id_i[port]].deps);
      else
        sb_enable_o[port] = sb_enable_i[port] && sb_id_ok_auto[sb_id_i[port]];
`else
      // Enable the VRF port if the dependant instructions wrote in the previous cycle
      // sb_enable_o[port] - scoreboard check if you can use this vrf port
      // sb_enable_i[port] - some unit want to use this vrf port
      // sb_id_i[port] - the instruction ID that is using this port
      // &(~scoreboard_q[sb_id_i[port]].deps | wrote_result_q) - we either do not care about the other instructions, or they have wrote to vrf
      //                                                         in words: All dependencies must have written their results in the previous cycle
      sb_enable_o[port] = sb_enable_i[port] &&
                          &(~scoreboard_q[sb_id_i[port]].deps |
                            (wrote_result_q & chainable_writer)) &&
                          (!(|scoreboard_q[sb_id_i[port]].deps) ||
                           !scoreboard_q[sb_id_i[port]].prevent_chaining);
`endif

`ifdef VENTAGLIO
      if (sb_enable_o[port] && vtl_en_q) begin : proc_vtl_rw_o
        if (port < NrReadPorts) begin // read
          if (vtl_table_q[sb_id_i[port]].read[port]) begin
            sb_vtl_redirect_read_o[port] = 1'b1;
          end else begin
            sb_vtl_redirect_read_o[port] = 1'b0;
          end
        end else begin // write
          if (vtl_table_q[sb_id_i[port]].write[port - NrReadPorts]) begin
            sb_vtl_redirect_write_o[port - NrReadPorts] = 1'b1;
          end else begin
            sb_vtl_redirect_write_o[port - NrReadPorts] = 1'b0;
          end
        end
      end // proc_vtl_rw_o
`endif
    end

`ifdef DOUBLE_BW
    // Store the decisions
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      int unsigned port_idx;

      port_idx = port - SB_VFU_VD_WD;
      if (sb_enable_o[port]) begin
        // VFU and VSLDU: intID derived from vl_cnt progress, vl_cnt updated every successful write
        if (port inside {SB_VFU_VD_WD, SB_VSLDU_VD_WD}) begin
          logic intID;
          int VRFWriteSize;

          intID = (vl_cnt_q[sb_id_i[port]] < vl_max_q[sb_id_i[port]]) ? 0 : 1;
          VRFWriteSize = narrow_q[sb_id_i[port]] ? VRFWordBWidth >> 1 : VRFWordBWidth;

          // Update vl_cnt if actually written into the VRF
          if (sb_wrote_result_i[port_idx])
            vl_cnt_d[sb_id_i[port]] += VRFWriteSize;

          wrote_result_narrowing_d[sb_id_i[port]] = sb_wrote_result_i[port_idx] ^ narrow_wide_q[sb_id_i[port]];
          wrote_result_d[intID][sb_id_i[port]]    = sb_wrote_result_i[port_idx] && (!narrow_wide_q[sb_id_i[port]] || wrote_result_narrowing_q[sb_id_i[port]]);
          if (vl_cnt_q[sb_id_i[port]] >= ((intID ? (vl_max_q[sb_id_i[port]] << 1) : vl_max_q[sb_id_i[port]]) - VRFWriteSize))
            done_result_d[intID][sb_id_i[port]] = wrote_result_d[intID][sb_id_i[port]];
        end
        // VLSU: intID is fixed per interface (0 for WD0, 1 for WD1)
        if (port inside {SB_VLSU_VD_WD0, SB_VLSU_VD_WD1}) begin
          int unsigned intID;
          vlen_t next_vlsu_vl;

          intID = port - SB_VLSU_VD_WD0;
          wrote_result_narrowing_d[sb_id_i[port]] = sb_wrote_result_i[port_idx] ^ narrow_wide_q[sb_id_i[port]];
          wrote_result_d[intID][sb_id_i[port]]    = sb_wrote_result_i[port_idx] && (!narrow_wide_q[sb_id_i[port]] || wrote_result_narrowing_q[sb_id_i[port]]);
          next_vlsu_vl = vlsu_vl_cnt_q[intID][sb_id_i[port]];
          if (wrote_result_d[intID][sb_id_i[port]]) begin
            next_vlsu_vl += VRFWordBWidth;
            vlsu_vl_cnt_d[intID][sb_id_i[port]] = next_vlsu_vl;
            if (next_vlsu_vl >= vl_max_q[sb_id_i[port]])
              done_result_d[intID][sb_id_i[port]] = 1'b1;
          end
        end
      end
    end
`else
    // Store the decisions for all write-destination ports: VFU, VLSU, VSLDU
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      if (sb_enable_o[port] && (port inside {SB_VFU_VD_WD, SB_VLSU_VD_WD, SB_VSLDU_VD_WD})) begin
        int unsigned port_idx;

        port_idx = port - SB_VFU_VD_WD;
        wrote_result_narrowing_d[sb_id_i[port]] = sb_wrote_result_i[port_idx] ^ narrow_wide_q[sb_id_i[port]];
        wrote_result_d[sb_id_i[port]]           = sb_wrote_result_i[port_idx] && (!narrow_wide_q[sb_id_i[port]] || wrote_result_narrowing_q[sb_id_i[port]]);
      end
    end
`endif

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
`ifdef VENTAGLIO
      vtl_table_d[vfu_rsp_i.id]              = '0;
`endif
`ifdef DOUBLE_BW
      narrow_d[vfu_rsp_i.id]                 = 1'b0;
      wrote_result_d[0][vfu_rsp_i.id]        = 1'b0;
      wrote_result_d[1][vfu_rsp_i.id]        = 1'b0;
      done_result_d[0][vfu_rsp_i.id]         = 1'b0;
      done_result_d[1][vfu_rsp_i.id]         = 1'b0;
      vl_cnt_d[vfu_rsp_i.id]                 = '0;
`endif
    end
    if (ope_rsp_valid_i) begin
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (read_table_q[vreg].id == ope_rsp_i.id && read_table_q[vreg].valid)
          read_table_d[vreg] = '0;
        if (write_table_q[vreg].id == ope_rsp_i.id && write_table_q[vreg].valid)
          write_table_d[vreg] = '0;
      end

      scoreboard_d[ope_rsp_i.id] = '0;
      narrow_wide_d[ope_rsp_i.id] = 1'b0;
      wrote_result_narrowing_d[ope_rsp_i.id] = 1'b0;

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
`ifdef VENTAGLIO
      vtl_table_d[vlsu_rsp_i.id]              = '0;
`endif
`ifdef DOUBLE_BW
      narrow_d[vlsu_rsp_i.id]                 = 1'b0;
      wrote_result_d[0][vlsu_rsp_i.id]        = 1'b0;
      wrote_result_d[1][vlsu_rsp_i.id]        = 1'b0;
      done_result_d[0][vlsu_rsp_i.id]         = 1'b0;
      done_result_d[1][vlsu_rsp_i.id]         = 1'b0;
      vlsu_vl_cnt_d[0][vlsu_rsp_i.id]         = '0;
      vlsu_vl_cnt_d[1][vlsu_rsp_i.id]         = '0;
      vl_cnt_d[vlsu_rsp_i.id]                 = '0;
`endif
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
`ifdef VENTAGLIO
      vtl_table_d[vsldu_rsp_i.id]              = '0;
`endif
`ifdef DOUBLE_BW
      narrow_d[vsldu_rsp_i.id]                 = 1'b0;
      wrote_result_d[0][vsldu_rsp_i.id]        = 1'b0;
      wrote_result_d[1][vsldu_rsp_i.id]        = 1'b0;
      done_result_d[0][vsldu_rsp_i.id]         = 1'b0;
      done_result_d[1][vsldu_rsp_i.id]         = 1'b0;
      vl_cnt_d[vsldu_rsp_i.id]                 = '0;
`endif
    end
`ifdef VENTAGLIO
    if (vtl_rsp_valid_i) begin
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (read_table_q[vreg].id == vtl_rsp_i.id && read_table_q[vreg].valid)
          read_table_d[vreg] = '0;
        if (write_table_q[vreg].id == vtl_rsp_i.id && write_table_q[vreg].valid)
          write_table_d[vreg] = '0;
      end

      scoreboard_d[vtl_rsp_i.id]             = '0;
      narrow_wide_d[vtl_rsp_i.id]            = 1'b0;
      wrote_result_narrowing_d[vtl_rsp_i.id] = 1'b0;
      vtl_table_d[vtl_rsp_i.id]              = '0;
    end
`endif

    for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
      scoreboard_d[insn].deps &= ~retired_mask;

    // Initialize the scoreboard metadata if we have a new instruction issued.
    if (spatz_req_valid && spatz_req.ex_unit != CON) begin
      // Read registered ownership; mask same-cycle completions after collection.
      scoreboard_d[spatz_req.id] = '0;
      narrow_wide_d[spatz_req.id] = 1'b0;
      wrote_result_narrowing_d[spatz_req.id] = 1'b0;
`ifdef DOUBLE_BW
      narrow_d[spatz_req.id] = 1'b0;
      wrote_result_d[0][spatz_req.id] = 1'b0;
      wrote_result_d[1][spatz_req.id] = 1'b0;
      done_result_d[0][spatz_req.id] = 1'b0;
      done_result_d[1][spatz_req.id] = 1'b0;
      vlsu_vl_cnt_d[0][spatz_req.id] = '0;
      vlsu_vl_cnt_d[1][spatz_req.id] = '0;
      vl_cnt_d[spatz_req.id] = '0;
`endif

`ifdef VENTAGLIO
      // VTL forward extension
      vtl_table_d[spatz_req.id] = '{use_vtl: 1'b0, write: '0, read: '0}; // init a table entry
      if (vtl_en_q) begin
        case (spatz_req.ex_unit)
          VFU: begin
            // First mark this instruction with VTL usage
            if (spatz_req.op_vtl.use_vtl) begin
              vtl_table_d[spatz_req.id].use_vtl = 1'b1;
            end
            // Then mark the request from which port should be redirected to VTL
            // Let's say if v8 is mapped to VTL
            // for an instruction do not use VTL, v8 is still in Vregfile
            // This is distinguished from spatz_req.op_vtl settings
            if (spatz_req.use_vs1   && |((32'b1 << spatz_req.vs1) & VTLVreg_q) ) // VFU read: vs1
              vtl_table_d[spatz_req.id].read[SB_VFU_VS1_RD] = 1'b1;
            if (spatz_req.use_vs2   && |((32'b1 << spatz_req.vs2) & VTLVreg_q) ) // VFU read: vs2
              vtl_table_d[spatz_req.id].read[SB_VFU_VS2_RD] = 1'b1;
            if (spatz_req.vd_is_src && |((32'b1 << spatz_req.vd) & VTLVreg_q) ) // VFU read: vd
              vtl_table_d[spatz_req.id].read[SB_VFU_VD_RD] = 1'b1;
            if (spatz_req.use_vd    && |((32'b1 << spatz_req.vd) & VTLVreg_q) )   // VFU write: vd
              vtl_table_d[spatz_req.id].write[SB_VFU_VD_WD-NrReadPorts] = 1'b1;
          end
          LSU: begin
            // bowwang: we do not set ld/st instruction as `use_vtl`
            //          set 'use_vtl' is to add an addtional check for scoreboard, ensuring the index is ready to use
            //          which should be changed (?) if we want to support unstructured sparsity

            // Normal ld/st instructions are considered
            // `vlx` instructions never map to VTL
            if (spatz_req.use_vd && |((32'b1 << spatz_req.op_vtl.old_vd) & VTLVreg_q) && !spatz_req.op_vtl.is_load_idx && spatz_req.op_mem.is_load) begin
              vtl_table_d[spatz_req.id].write[SB_VLSU_VD_WD-NrReadPorts] = 1'b1; // w
            end
            if (spatz_req.use_vd && |((32'b1 << spatz_req.op_vtl.old_vd) & VTLVreg_q) && !spatz_req.op_vtl.is_load_idx && !spatz_req.op_mem.is_load) begin
              vtl_table_d[spatz_req.id].read[SB_VLSU_VD_RD] = 1'b1; // r
            end
          end
          default: begin
            vtl_table_d[spatz_req.id] = '{use_vtl: 1'b0, write: '0, read: '0};
          end
        endcase
      end
`endif

      // Claim the registers of the footprint: one fixed-index update per entry
      for (int unsigned vreg = 0; vreg < NRVREG; vreg++) begin
        if (issue_rd_vregs[vreg]) read_table_d[vreg]  = {spatz_req.id, 1'b1};
        if (issue_wr_vregs[vreg]) write_table_d[vreg] = {spatz_req.id, 1'b1};
      end
      scoreboard_d[spatz_req.id].deps = issue_deps;

      // Is this a risky instruction which should not chain?
      if (spatz_req.op inside {VSLIDEUP, VLSE, VLXE, VSSE, VSXE})
        scoreboard_d[spatz_req.id].prevent_chaining = 1'b1;

      // Is this a narrowing or widening instruction?
      if (spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2)
        narrow_wide_d[spatz_req.id] = 1'b1;

`ifdef DOUBLE_BW
      narrow_d[spatz_req.id] = spatz_req.op_arith.is_narrowing;

      // Track the byte extent of each half-vector. Vector memory operations use their encoded EEW, 
      // which may differ from SEW for both loads and stores; arithmetic operations use the configured SEW.
      vl_max_d[spatz_req.id] = (spatz_req.vl >> 1) << ((spatz_req.ex_unit == LSU) ? spatz_req.op_mem.ew : spatz_req.vtype.vsew);
      vl_cnt_d[spatz_req.id] = '0;
`endif
      // Do not retain a dependency on an owner that retires this cycle.
      scoreboard_d[spatz_req.id].deps &= ~retired_mask;
    end

    // An instruction never depends on itself
    for (int insn = 0; insn < NrParallelInstructions; insn++)
      scoreboard_d[insn].deps[insn] = 1'b0;
  end

  // Per-vreg "writer in flight" export. Sourced from `write_table_d` (the
  // about-to-register value) rather than `write_table_q` so the signal
  // falls the same cycle the writer's response fires, not one cycle
  // later. This lets Ventaglio's `fetch_for_next` prefetch a freshly-
  // written index vreg (e.g. sp-SpMV's v20 written by vlx) one cycle
  // sooner — saving one cycle of the inter-op bubble on `rvalid_o` at
  // the VFU boundary. Issue side is unaffected: write_table_d goes high
  // the cycle a writer is admitted, same as before. Safe because the
  // _d wire is a combinational fan-in of registered inputs only
  // (write_table_q, *_rsp_valid_i, *_rsp_i.id, and the issue claims),
  // and the prefetch consumer is itself fully combinational so cycle-
  // accurate visibility is what we want.
`ifdef VENTAGLIO
  for (genvar v = 0; v < NRVREG; v++) begin : gen_vreg_pending_export
    assign vreg_write_pending_o[v] = write_table_d[v].valid;
  end
`endif

  /////////////
  // Issuing //
  /////////////

  // Retire CSR instruction and write back result to main core.
  logic retire_csr;
  logic rsp_ready_d;  // response register ready (registered)

  // We stall issuing a new instruction if the corresponding execution unit is
  // not ready yet. Or we have a change in LMUL, for which we need to let all the
  // units finish first before scheduling a new operation (to avoid running into
  // issues with the socreboard).
  // Snapshot older matrix dependencies at dispatch, then grant execution by ID.
  // Keep every reader until retire; a later writer must wait for all of them.
  typedef logic [NrPhysicalTile-1:0] matrix_mask_t;
  typedef struct packed {
    matrix_mask_t mask;
    logic reads;
    logic writes;
    logic is_mac;
    logic is_lsu;
    logic [NrParallelInstructions-1:0] deps;
  } matrix_claim_t;
  matrix_claim_t [NrParallelInstructions-1:0] matrix_table_q, matrix_table_d;
  matrix_claim_t matrix_issue;
  matrix_mask_t matrix_issue_mask;
  mt_t matrix_issue_tile;
  logic [NrParallelInstructions-1:0] matrix_issue_deps, matrix_retire;
  `FF(matrix_table_q, matrix_table_d, '0)

  always_comb begin : matrix_decode
    matrix_issue = matrix_claim_t'(0);
    // Subset operations carry their tile in TSS; full-tile operations use mtd.
    matrix_issue_tile = buffer_spatz_req.op_ope.tss.tile_valid ?
                        buffer_spatz_req.op_ope.tss.tile_id : buffer_spatz_req.mtd;

    // mt0 with tew=32 overlaps both mt0 and mt2 with tew=16
    case (vew_e'(vtype_q.vsew + mtype_q.mtwiden - 1'b1))  // = tew
      EW_8: matrix_issue_mask  = matrix_mask_t'(1'b1) << matrix_issue_tile;
      EW_16: matrix_issue_mask = matrix_mask_t'(2'b11) << matrix_issue_tile;
      EW_32: matrix_issue_mask = matrix_mask_t'(4'b1111) << matrix_issue_tile;
      default: matrix_issue_mask = '1;
    endcase

    if (buffer_spatz_req.ex_unit == LSU && buffer_spatz_req.op_ope.is_mem) begin
      matrix_issue.mask = matrix_issue_mask;
      matrix_issue.is_lsu = 1'b1;
      if (buffer_spatz_req.op_mem.is_load)
        matrix_issue.writes = 1'b1;
      else
        matrix_issue.reads = 1'b1;
    end else if (buffer_spatz_req.ex_unit == OPE) begin
      matrix_issue.mask = matrix_issue_mask;
      matrix_issue.is_mac = buffer_spatz_req.op_ope.is_mac;
      matrix_issue.reads = buffer_spatz_req.op_ope.is_mac ||
                           (buffer_spatz_req.op_ope.tss.tile_valid &&
                            buffer_spatz_req.use_vd);
      matrix_issue.writes = buffer_spatz_req.op_ope.is_mac ||
                            buffer_spatz_req.op_ope.is_zero_tile ||
                            (buffer_spatz_req.op_ope.tss.tile_valid &&
                             buffer_spatz_req.use_vs2);

      if (buffer_spatz_req.op_ope.is_discard) begin
        // A global accumulator-state flush conflicts with every tile access.
        matrix_issue.mask = matrix_mask_t'({NrPhysicalTile{1'b1}});
        matrix_issue.writes = 1'b1;
      end
    end
  end

  always_comb begin : matrix_hazards
    matrix_issue_deps = '0;
    for (int unsigned insn = 0; insn < NrParallelInstructions; insn++) begin
      // skip data hazard checking for mac instructions if they store to the same tile
      // becuase OPE forwards their results internally to FMA (won't drain to accumulators)
      if (!(matrix_issue.is_mac && matrix_table_q[insn].is_mac)) begin
        matrix_issue_deps[insn] =
          (|(matrix_issue.mask & matrix_table_q[insn].mask)) &&
          // Read during Write
          ((matrix_issue.reads && matrix_table_q[insn].writes) ||
          // Write during Read or Write during Write
            (matrix_issue.writes && (matrix_table_q[insn].reads || matrix_table_q[insn].writes)));
      end
    end
  end

  always_comb begin : matrix_retirement
    matrix_retire = '0;
    if (vlsu_rsp_valid_i && matrix_table_q[vlsu_rsp_i.id].is_lsu)
      matrix_retire[vlsu_rsp_i.id] = 1'b1;
    if (ope_rsp_valid_i && !matrix_table_q[ope_rsp_i.id].is_lsu)
      matrix_retire[ope_rsp_i.id] = 1'b1;
  end

  always_comb begin : matrix_tracking
    matrix_table_d = matrix_table_q;
    for (int unsigned insn = 0; insn < NrParallelInstructions; insn++) begin
      matrix_table_d[insn].deps = matrix_table_q[insn].deps & ~matrix_retire;
      if (matrix_retire[insn]) matrix_table_d[insn] = '0;
    end
    if (spatz_req_valid && spatz_req.ex_unit != CON) begin
      matrix_table_d[spatz_req.id] = matrix_issue;
      matrix_table_d[spatz_req.id].deps = matrix_issue_deps & ~matrix_retire;
      matrix_table_d[spatz_req.id].deps[spatz_req.id] = 1'b0;
    end
  end

  // Registered dependencies avoid a completion -> ready -> issue loop.
  // Unit input queues are non-fall-through, so the entry exists before use.
  for (genvar insn = 0; insn < NrParallelInstructions; insn++) begin : gen_matrix_enable
    assign matrix_enable_o[insn] = !(|matrix_table_q[insn].deps);
  end

  logic stall, vfu_stall, vlsu_stall, vsldu_stall, vtl_stall, ope_stall, csr_stall;
  assign stall       = vfu_stall | vlsu_stall | vsldu_stall | vtl_stall | ope_stall | csr_stall;
  // A configuration/CSR instruction always returns a response: hold it while the
  // response register is full so the response cannot be dropped.
  assign csr_stall   = (spatz_req.ex_unit == CON) && !rsp_ready_d;
  assign vfu_stall   = ~vfu_req_ready_i  & (spatz_req.ex_unit == VFU);
  assign vlsu_stall  = ~vlsu_req_ready_i & (spatz_req.ex_unit == LSU);
`ifdef VENTAGLIO
  // VSLDU stalls only for true SLD-routed non-VTL ops (the VTL/VSLDU
  // shared-port arbiter routes use_vtl ops through ventaglio instead).
  assign vsldu_stall = ~vsldu_req_ready_i &
                       (spatz_req.ex_unit == SLD && !spatz_req.op_vtl.use_vtl);
  // VTL stalls for ANY use_vtl op (vventclr = SLD-routed, vfx = VFU-routed)
  // so vfxmacc/vfxmul wait for vventclr (and other in-flight VTL ops) to
  // drain ventaglio's spill register before admitting.
  assign vtl_stall   = ~vtl_req_ready_i & spatz_req.op_vtl.use_vtl;
`else
  assign vsldu_stall = ~vsldu_req_ready_i & (spatz_req.ex_unit == SLD);
  assign vtl_stall   = 1'b0;
`endif
  assign ope_stall   = ~ope_req_ready_i  & (spatz_req.ex_unit == OPE);

  // Running instructions
  logic      [NrParallelInstructions-1:0] running_insn_d, running_insn_q;
  spatz_id_t                              next_insn_id;
  logic                                   running_insn_full;
  `FF(running_insn_q, running_insn_d, '0)
  logic                                   insn_shortcut_en;
  spatz_id_t                              insn_shortcut_id;

  find_first_one #(
    .WIDTH(NrParallelInstructions)
  ) i_ffo_next_insn_id (
    .in_i       (~running_insn_q  ),
    .first_one_o(next_insn_id     ),
    .no_ones_o  (running_insn_full)
  );

  // Pop the buffer if we do not have a unit stall
  assign req_buffer_pop = !stall && req_buffer_valid && !running_insn_full;

  // Issue new operation to execution units
  always_comb begin : ex_issue
    retire_csr = 1'b0;

    // Define new spatz request
    spatz_req             = buffer_spatz_req;
    spatz_req.id          = next_insn_id;
`ifdef VENTAGLIO
    spatz_req_vtl_illegal = !vtl_en_q && decoder_rsp.spatz_req.op_vtl.use_vtl; // illegal if vtl is disabled but used
    spatz_req_illegal     = decoder_rsp_valid ? decoder_rsp.instr_illegal || spatz_req_vtl_illegal : 1'b0;
`else
    spatz_req_illegal     = decoder_rsp_valid ? decoder_rsp.instr_illegal : 1'b0;
`endif
    spatz_req_valid       = req_buffer_pop && !spatz_req_illegal && (!running_insn_full);

    // Complete the request with the CSR state of its unit. This does not depend
    // on spatz_req_valid, so the request fields (and the scoreboard footprint
    // derived from them) are not delayed by the unit ready/stall logic.
    // Consumers only use the request together with spatz_req_valid.
    begin
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

`ifdef VENTAGLIO
          // Is this a VTL related instruction?
          if (spatz_req.op_vtl.use_vtl) begin
            spatz_req.op_vtl.sp_cfg = VTL_cfg_q;
          end
`endif
        end

        OPE: begin
          // VME matrix ops: embed current tile dimensions into the request
          spatz_req.vtype      = vtype_q;
          spatz_req.op_ope.tew = vew_e'(
              int'(vtype_q.vsew) + int'(mtype_q.mtwiden) - 1);
          spatz_req.op_ope.tn = tn_q;
          spatz_req.op_ope.tm = elen_t'(mtype_q.tm);
          spatz_req.op_ope.tk = elen_t'(mtype_q.tk);
          spatz_req.vl         = vl_q;
          spatz_req.vstart     = '0;
        end

        LSU: begin
          // Overwrite vector CSRs in request. The scoreboard uses LMUL to
          // reserve all destination registers of grouped loads.
          spatz_req.vtype  = vtype_q;
          spatz_req.vl     = vl_q;
          spatz_req.vstart = vstart_q;
          if (spatz_req.op_ope.is_mem) begin
            spatz_req.op_ope.tew = vew_e'(
                int'(vtype_q.vsew) + int'(mtype_q.mtwiden) - 1);
            spatz_req.op_ope.tn = tn_q;
            spatz_req.op_ope.tm = elen_t'(mtype_q.tm);
            spatz_req.op_ope.tk = elen_t'(mtype_q.tk);
          end
          // The decoder's load/store branches set `op_vtl.old_vd = ls_vd`
          // unconditionally (the field stays defined regardless of
          // VENTAGLIO), but they comment out the direct `spatz_req.vd = ls_vd`
          // assignment. So we copy `op_vtl.old_vd -> vd` here in the
          // controller for ALL builds. When VENTAGLIO=off this is just
          // "use the decoded ls_vd"; when VENTAGLIO=on the VTL block below
          // may further remap `vd` based on the sparsity ratio.
          spatz_req.vd     = spatz_req.op_vtl.old_vd;
`ifdef VENTAGLIO
          // If VTL is enabled, we add vtl_cfg to support vlx instruction
          if (vtl_en_q) begin
            spatz_req.op_vtl.sp_cfg = VTL_cfg_q;
          end
          if (VTLVreg_q[spatz_req.op_vtl.old_vd] && vtl_en_q) begin
            // VTL extension is enabled, the target vreg is mapped to VTL
            spatz_req.vl = (VTL_cfg_q.sp_cfg_ratio == SP_RATIO_025) ? vl_q << 2 :
                           (VTL_cfg_q.sp_cfg_ratio == SP_RATIO_050) ? vl_q << 1 : vl_q;
            spatz_req.vd = (VTL_cfg_q.sp_cfg_ratio == SP_RATIO_025) ? spatz_req.op_vtl.old_vd << 2 :
                           (VTL_cfg_q.sp_cfg_ratio == SP_RATIO_050) ? spatz_req.op_vtl.old_vd << 1 : spatz_req.op_vtl.old_vd;
          end
`endif
        end

        SLD: begin
          // Overwrite all csrs in request
          spatz_req.vtype  = vtype_q;
          spatz_req.vl     = vl_q;
          spatz_req.vstart = vstart_q;

          // Is this a scalar request?
          if (spatz_req.op_arith.is_scalar) begin
            spatz_req.vl     = 1;
            spatz_req.vstart = '0;
          end
        end

        default: begin
          // Do not overwrite csrs, but retire new csr information
          retire_csr = spatz_req_valid;
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
    if (vfu_rsp_valid_i) begin
      running_insn_d[vfu_rsp_i.id] = 1'b0;
    end
    if (vlsu_rsp_valid_i) begin
      running_insn_d[vlsu_rsp_i.id] = 1'b0;
    end
    if (vsldu_rsp_valid_i) begin
      running_insn_d[vsldu_rsp_i.id] = 1'b0;
    end
    if (ope_rsp_valid_i) begin
      running_insn_d[ope_rsp_i.id] = 1'b0;
    end
`ifdef VENTAGLIO
    if (vtl_rsp_valid_i) begin
      running_insn_d[vtl_rsp_i.id] = 1'b0;
    end
`endif
  end: proc_next_insn_id

  // Respond to core about the decoded instruction.
  always_comb begin : acc_issue_resp
    mt_t  issue_tile_id;
    logic issue_is_tile_op;
    logic issue_tile_aligned;
    logic memory_groups_aligned;
    logic issue_uses_vreg_group;
    logic [3:0] issue_vreg_group_regs;
    vreg_t issue_vreg_group_base;
    logic issue_vreg_group_aligned;

    issue_rsp_o = '0;
    issue_tile_id = decoder_rsp.spatz_req.mtd;
    issue_is_tile_op = 1'b0;
    unique case (vtype_q.vlmul)
      LMUL_2:  issue_vreg_group_regs = 4'd2;
      LMUL_4:  issue_vreg_group_regs = 4'd4;
      LMUL_8:  issue_vreg_group_regs = 4'd8;
      default: issue_vreg_group_regs = 4'd1;
    endcase
    issue_uses_vreg_group = (decoder_rsp.spatz_req.ex_unit == OPE) &&
                            decoder_rsp.spatz_req.op_ope.tss.tile_valid &&
                            (decoder_rsp.spatz_req.use_vd || decoder_rsp.spatz_req.use_vs2);
    issue_vreg_group_base = decoder_rsp.spatz_req.use_vd ?
                            decoder_rsp.spatz_req.vd : decoder_rsp.spatz_req.vs2;
    issue_vreg_group_aligned =
        ((issue_vreg_group_base & vreg_t'(issue_vreg_group_regs - 1'b1)) == '0) &&
        (({1'b0, issue_vreg_group_base} + issue_vreg_group_regs) <= NRVREG);
    memory_groups_aligned = issue_memory_emul.data.aligned;
    if (decoder_rsp.spatz_req.op inside {VLXE, VSXE}) begin
      memory_groups_aligned &= issue_memory_emul.index.aligned;
    end

    if ((decoder_rsp.spatz_req.ex_unit == LSU) &&
        decoder_rsp.spatz_req.op_ope.is_mem) begin
      issue_tile_id = decoder_rsp.spatz_req.op_ope.tss.tile_id;
      issue_is_tile_op = 1'b1;
    end else if (decoder_rsp.spatz_req.ex_unit == OPE) begin
      issue_is_tile_op = 1'b1;
      if (decoder_rsp.spatz_req.op_ope.tss.tile_valid)
        issue_tile_id = decoder_rsp.spatz_req.op_ope.tss.tile_id;
    end

    issue_tile_aligned = 1'b1;
    if (issue_is_tile_op) begin
      if ((vtype_q.vsew == EW_16) && (mtype_q.mtwiden == 2'b01)) begin
        issue_tile_aligned = !issue_tile_id[0];
      end else if (((vtype_q.vsew == EW_32) && (mtype_q.mtwiden == 2'b01)) ||
                   ((vtype_q.vsew == EW_16) && (mtype_q.mtwiden == 2'b10))) begin
        issue_tile_aligned = !(|issue_tile_id[1:0]);
      end
    end

    // Is there something running on Spatz? If so, prevent Snitch from reading the fcsr register
    issue_rsp_o.isfloat = |running_insn_q;

    // We have a new valid instruction
    if (decoder_rsp_valid && !decoder_rsp.instr_illegal) begin
      // Accept the new instruction
      issue_rsp_o.accept = 1'b1;

      case (decoder_rsp.spatz_req.ex_unit)
        CON: begin
          issue_rsp_o.writeback = decoder_rsp.spatz_req.use_rd;
        end // CON
        VFU: begin
          // vtype is illegal -> illegal instruction (VME tile ops do not use vtype)
          if (vtype_q.vill) begin
            issue_rsp_o.accept = 1'b0;
          end
        end // VFU
        LSU: begin
          issue_rsp_o.loadstore = 1'b1;
          if (vtype_q.vill) begin
            issue_rsp_o.accept = 1'b0;
          end else if (decoder_rsp.spatz_req.op_ope.is_mem &&
                       ((mtype_q.mtwiden == 0) || !issue_tile_aligned)) begin
            // Tile loads/stores require an active matrix configuration.
            issue_rsp_o.accept = 1'b0;
          end else if (!decoder_rsp.spatz_req.op_ope.is_mem &&
                       !memory_groups_aligned) begin
            // Regular RVV memory operations use EMUL, not LMUL, for register
            // alignment and register-file bounds.
            issue_rsp_o.accept = 1'b0;
          end
        end // LSU
        SLD: begin
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
            issue_rsp_o.accept = 1'b0;
          end
        end // SLD
        OPE: begin
          // mtype is illegal -> illegal instruction
          if (mtype_q.mtwiden == 0) begin
            issue_rsp_o.accept = 1'b0;
          end else if (!issue_tile_aligned) begin
            issue_rsp_o.accept = 1'b0;
          end else if (decoder_rsp.spatz_req.op_ope.is_mac && (vtype_q.vsew == EW_8)) begin
            // No 8-bit MAC lane is instantiated; reject instead of waiting for a missing result.
            issue_rsp_o.accept = 1'b0;
          end else if (issue_uses_vreg_group &&
                       (mtype_q.mtwiden != 2'b01)) begin
            // Register-group tile transfers require a one-to-one SEW/TEW mapping.
            issue_rsp_o.accept = 1'b0;
          end else if (issue_uses_vreg_group && !issue_vreg_group_aligned) begin
            // Vector-register group specifiers must be divisible by LMUL
            // and the complete group must remain within the architectural VRF.
            issue_rsp_o.accept = 1'b0;
          end
        end
        default:;
      endcase // Operation type
    // The decoding resulted in an illegal instruction
    end else if (decoder_rsp_valid && decoder_rsp.instr_illegal) begin
      // Do not accept it
      issue_rsp_o.accept = 1'b0;
    end
  end // acc_issue_resp

  //////////////
  // Retiring //
  //////////////

  vfu_rsp_t vfu_rsp;
  logic     vfu_rsp_valid;
  logic     vfu_rsp_ready;

  spill_register #(
    .T(vfu_rsp_t)
  ) i_vfu_scalar_response (
    .clk_i  (clk_i                          ),
    .rst_ni (rst_ni                         ),
    .data_i (vfu_rsp_i                      ),
    .valid_i(vfu_rsp_valid_i && vfu_rsp_i.wb),
    .ready_o(vfu_rsp_ready_o                ),
    .data_o (vfu_rsp                        ),
    .valid_o(vfu_rsp_valid                  ),
    .ready_i(vfu_rsp_ready                  )
  );

  // OPE instructions only return a completion ID. OPE owns response buffering
  // and arbitration, while the controller consumes every completion directly
  // to release its scoreboard entry.
  assign ope_rsp_ready_o = 1'b1;

  logic       rsp_valid_d;
  spatz_rsp_t rsp_d;
  spill_register #(
    .T     (spatz_rsp_t ),
    .Bypass(!RegisterRsp)
  ) i_spatz_rsp_register (
    .clk_i  (clk_i       ),
    .rst_ni (rst_ni      ),
    .data_i (rsp_d       ),
    .valid_i(rsp_valid_d ),
    .ready_o(rsp_ready_d ),
    .data_o (rsp_o       ),
    .valid_o(rsp_valid_o ),
    .ready_i(rsp_ready_i )
  );

  // Retire an operation/instruction and write back result to core
  // if necessary.
  always_comb begin : retire
    rsp_d       = '0;
    rsp_valid_d = '0;

    vfu_rsp_ready = 1'b0;
    if (retire_csr) begin
`ifdef MEMPOOL_SPATZ
      rsp_d.write = 1'b1;
`endif
      // Read CSR and write back to cpu
      if (spatz_req.op == VCSR) begin
        if (spatz_req.use_rd) begin
          unique case (spatz_req.op_csr.addr)
            riscv_instr::CSR_VSTART: rsp_d.data = elen_t'(vstart_q);
            riscv_instr::CSR_VL    : rsp_d.data = elen_t'(vl_q);
            riscv_instr::CSR_VTYPE : rsp_d.data = elen_t'(vtype_q);
            riscv_instr::CSR_VLENB : rsp_d.data = elen_t'(VLENB);
            riscv_instr::CSR_MTYPE : rsp_d.data = elen_t'(mtype_q);
            riscv_instr::CSR_VXSAT : rsp_d.data = '0;
            riscv_instr::CSR_VXRM  : rsp_d.data = '0;
            riscv_instr::CSR_VCSR  : rsp_d.data = '0;
            default: rsp_d.data                 = '0;
          endcase
        end
        rsp_d.id    = spatz_req.rd;
        rsp_valid_d = 1'b1;
      end else if (spatz_req.op == VMCFG) begin
        if (spatz_req.use_rd) begin
          // VMCFG: return the newly configured tile dimension
          unique case (spatz_req.op_ope.cfg_sel)
            2'b01: rsp_d.data = elen_t'(tn_d);
            2'b10: rsp_d.data = elen_t'(mtype_d.tm);
            2'b11: rsp_d.data = elen_t'(mtype_d.tk);
            default: rsp_d.data = '0;
          endcase
        end
        rsp_d.id = spatz_req.rd;
        rsp_valid_d = 1'b1;
      end else begin
        // VCFG: change configuration and send back vl
        rsp_d.id    = spatz_req.rd;
        rsp_d.data  = elen_t'(vl_d);
        rsp_valid_d = 1'b1;
      end
    end else if (vfu_rsp_valid) begin
      rsp_d.id      = vfu_rsp.rd;
      rsp_d.data    = vfu_rsp.result;
`ifdef MEMPOOL_SPATZ
      rsp_d.write   = 1'b1;
`endif
      rsp_valid_d   = 1'b1;
      vfu_rsp_ready = rsp_ready_d;
    end
  end // retire

  // Signal to core that Spatz is ready for a new instruction
  assign issue_ready_o = req_buffer_ready;

  // Send request off to execution units
  assign spatz_req_o       = spatz_req;
  assign spatz_req_valid_o = spatz_req_valid;

endmodule : spatz_controller
