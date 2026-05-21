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
    // VFU
    input  logic                                   vfu_req_ready_i,
    input  logic                                   vfu_rsp_valid_i,
    output logic                                   vfu_rsp_ready_o,
    input  vfu_rsp_t                               vfu_rsp_i,
    // OPE
    input  logic                                   ope_req_ready_i,
    input  logic                                   ope_rsp_valid_i,
    input  vfu_rsp_t                               ope_rsp_i,
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

  logic spatz_req_is_tile;
  logic buffer_req_is_tile;

  assign spatz_req_is_tile =
      spatz_req.op inside {VTLE, VTSE, VTMV_VT, VTMV_TV, VTZERO, VTDISCARD,
                          VTFMM, VTFMM_ALT, VTMMU, VTMMS};

  assign buffer_req_is_tile =
      buffer_spatz_req.op inside {VTLE, VTSE, VTMV_VT, VTMV_TV, VTZERO, VTDISCARD,
                                  VTFMM, VTFMM_ALT, VTMMU, VTMMS};

  logic [NrParallelInstructions-1:0] tile_running_d, tile_running_q;

  `FF(tile_running_q, tile_running_d, '0)

  always_comb begin
    tile_running_d = tile_running_q;

    if (spatz_req_valid && spatz_req.ex_unit != CON && spatz_req_is_tile) begin
      tile_running_d[spatz_req.id] = 1'b1;
    end

    if (vfu_rsp_valid_i)
      tile_running_d[vfu_rsp_i.id] = 1'b0;

    if (vlsu_rsp_valid_i)
      tile_running_d[vlsu_rsp_i.id] = 1'b0;

    if (vsldu_rsp_valid_i)
      tile_running_d[vsldu_rsp_i.id] = 1'b0;

    if (ope_rsp_valid_i)
      tile_running_d[ope_rsp_i.id] = 1'b0;
  end

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


  always_comb begin : proc_vcsr
    automatic logic [$clog2(MAXVL):0] vlmax = 0;

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
        unique case (spatz_req.op_tile.cfg_sel)

          // msetmtype / msetmtypei: vl = 0, update vtype, calculate matrix LMUL
          2'b00: begin : msetmtype_vcsr
            logic        invalid_vtype;
            int unsigned sew;
            int unsigned twiden;
            int unsigned tew;
            int unsigned kmax;
            int unsigned ete;
            int unsigned eve;
            int unsigned req_lmul;
            int unsigned sel_lmul;
            int unsigned max_lmul_k;
            int unsigned max_lmul_widen;

            vtype_d = spatz_req.vtype;

            invalid_vtype =
                vtype_d.vill
                || (vtype_d.vsew > MAXEW)
                || (vtype_d.vlmul inside {LMUL_RES, LMUL_F8})
                || (signed'(vtype_d.vlmul) + signed'($clog2(ELEN)) < signed'(vtype_d.vsew));

            if (!invalid_vtype && spatz_req.mtype.mtwiden != 2'b00) begin
              sew = 8 << int'(vtype_d.vsew);

              unique case (spatz_req.mtype.mtwiden)
                2'b01: twiden = 1;
                2'b10: twiden = 2;
                2'b11: twiden = 4;
                default: twiden = 0;
              endcase

              tew = sew * twiden;

              if (tew > ELEN) begin
                invalid_vtype = 1'b1;
              end else begin
                kmax = (vtype_d.vsew == EW_8 ) ? 4 :
                       (vtype_d.vsew == EW_16) ? 2 : 1;

                ete = (tew < 64) ? TE : (TE >> 1);
                eve = VLENB >> int'(vtype_d.vsew);

                req_lmul = (ete + eve - 1) / eve;

                max_lmul_k     = 8 / kmax;
                max_lmul_widen = 8 / twiden;

                sel_lmul = req_lmul;

                if (max_lmul_k < sel_lmul) begin
                  sel_lmul = max_lmul_k;
                end

                if (max_lmul_widen < sel_lmul) begin
                  sel_lmul = max_lmul_widen;
                end

                unique case (sel_lmul)
                  8:       vtype_d.vlmul = LMUL_8;
                  4:       vtype_d.vlmul = LMUL_4;
                  2:       vtype_d.vlmul = LMUL_2;
                  default: vtype_d.vlmul = LMUL_1;
                endcase

                vtype_d.vma = 1'b1;
                vtype_d.vta = 1'b1;
              end
            end

            vtype_d = (invalid_vtype) ?
                      '{vill: 1'b1, altfmt: 1'b0, vsew: EW_8, vlmul: LMUL_1, default: '0}
                      : vtype_d;

            vl_d = '0;  // always writes vl = 0.
          end
          // msettn:
          //   unconfigured: vl = min(rs1, LMUL * EVE)
          //   configured:   vl = min(rs1, LMUL * EVE, ETE)
          2'b01: begin : msettn_vcsr
            logic [$clog2(MAXVL):0] lmul_eve;
            elen_t                  ete;
            int unsigned            sew;
            int unsigned            twiden;
            int unsigned            tew;

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
                vl_d = (spatz_req.rs1 > elen_t'(lmul_eve))
                       ? vlen_t'(lmul_eve) : vlen_t'(spatz_req.rs1);
              end else begin
                sew = 8 << int'(vtype_q.vsew);

                unique case (mtype_q.mtwiden)
                  2'b01: twiden = 1;
                  2'b10: twiden = 2;
                  2'b11: twiden = 4;
                  default: twiden = 0;
                endcase

                tew = sew * twiden;
                ete = (tew < 64) ? elen_t'(TE) : elen_t'(TE >> 1);

                vl_d = vlen_t'(min3(elen_t'(spatz_req.rs1), elen_t'(lmul_eve), ete));
              end
            end
          end
          default: ;
        endcase
      end
    end // spatz_req_valid
  end

  always_comb begin : proc_mcsr
    tn_d = tn_q;
    mtype_d  = mtype_q;

    if (spatz_req_valid) begin
      if (spatz_req.op == VCFG)
        mtype_d = '0;

      if (spatz_req.op == VMCFG) begin
        unique case (spatz_req.op_tile.cfg_sel)

          // msetmtype / msetmtypei: write spatz_req.mtype with clamped tk/tm
          2'b00: begin : msetmtype_mcsr
              logic [2:0]             kmax_val;
              elen_t                  ete;
              logic [$clog2(MAXVL):0] lmul_eve;
              logic                   is_ill;
              int unsigned            sew;
              int unsigned            twiden;
              int unsigned            tew;

              mtype_d = spatz_req.mtype;

              unique case (mtype_d.mtwiden)
                2'b01: twiden = 1;
                2'b10: twiden = 2;
                2'b11: twiden = 4;
                default: twiden = 0;
              endcase

              sew = 8 << int'(vtype_d.vsew);
              tew = sew * twiden;

              // clamp tk
              kmax_val  = (vtype_d.vsew == EW_8 ) ? 3'd4 :
                          (vtype_d.vsew == EW_16) ? 3'd2 : 3'd1;
              if (mtype_d.tk > kmax_val) begin
                mtype_d.tk = kmax_val;
              end

              // clamp tm using the newly selected matrix LMUL in vtype_d.
              ete = (tew < 64) ? elen_t'(TE) : elen_t'(TE >> 1);

              lmul_eve = VLENB >> int'(vtype_d.vsew);
              unique case (vtype_d.vlmul)
                LMUL_F2: lmul_eve >>= 1;
                LMUL_F4: lmul_eve >>= 2;
                LMUL_1 : lmul_eve <<= 0;
                LMUL_2 : lmul_eve <<= 1;
                LMUL_4 : lmul_eve <<= 2;
                LMUL_8 : lmul_eve <<= 3;
                default: lmul_eve = '0;
              endcase

              mtype_d.tm = 14'(min3(elen_t'(mtype_d.tm), elen_t'(lmul_eve), ete));

              // reset vl / tn
              tn_d = '0;

              is_ill = (mtype_d.mtwiden == 2'b00) || vtype_d.vill || (tew > ELEN);
              mtype_d = is_ill ? '0 : mtype_d;
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
              logic [$clog2(MAXVL):0] lmul_eve;
              elen_t                  ete;
              int unsigned            sew;
              int unsigned            twiden;
              int unsigned            tew;

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

              sew = 8 << int'(vtype_q.vsew);

              unique case (mtype_q.mtwiden)
                2'b01: twiden = 1;
                2'b10: twiden = 2;
                2'b11: twiden = 4;
                default: twiden = 0;
              endcase

              tew = sew * twiden;
              ete = (tew < 64) ? elen_t'(TE) : elen_t'(TE >> 1);

              mtype_d.tm = 14'(min3(elen_t'(spatz_req.rs1), elen_t'(lmul_eve), ete));
            end
          end
          // msettk: mtype.tk = min(rs1, KMAX)   (0 if unconfigured)
          2'b11: begin
            if (mtype_q.mtwiden == 2'b0) begin
              mtype_d.tk = '0;
            end else begin
              logic [2:0] kmax_val;
              kmax_val   = (vtype_q.vsew == EW_8)  ? 3'd4 :
                           (vtype_q.vsew == EW_16) ? 3'd2 : 3'd1;
              mtype_d.tk = (spatz_req.rs1 > elen_t'(kmax_val))
                           ? kmax_val : 3'(spatz_req.rs1);
            end
          end

          default: ;
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
  always_comb begin : proc_decode
    decoder_req       = '0;
    decoder_req_valid = 1'b0;

    // Decode new instruction if one is received and spatz is ready
    if (issue_valid_i && issue_ready_o) begin
      decoder_req.instr = issue_req_i.data_op;
      decoder_req.rs1   = issue_req_i.data_arga;
      decoder_req.rs2   = issue_req_i.data_argb;
      decoder_req.rsd   = issue_req_i.data_argc;
      decoder_req.rd    = issue_req_i.id;
      decoder_req.vtype = vtype_q;
      decoder_req_valid = 1'b1;
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
  typedef struct packed {
    spatz_id_t id;
    logic valid;
  } [NRVREG-1:0] table_t;
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

  // Following counters are used only by DOUBLE_BW for tracking
  logic [NumVLSUInterfaces-1:0] [NrParallelInstructions-1:0] done_result_q, done_result_d;

  // Counter to track the vlen completed for each instruction
  vlen_t [NrParallelInstructions-1:0] vl_cnt_d, vl_cnt_q, vl_max_d, vl_max_q;

  // Is this instruction a narrowing instruction?
  logic [NrParallelInstructions-1:0] narrow_q, narrow_d;

  `FF(done_result_q, done_result_d, '0)
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

  always_comb begin : scoreboard
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

`ifdef DOUBLE_BW
    done_result_d        = done_result_q;
    narrow_d             = narrow_q;
    vl_cnt_d             = vl_cnt_q;
    vl_max_d             = vl_max_q;
`endif

    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
`ifdef DOUBLE_BW
      // Calculate the load-store interface id to use here for chaining
      automatic logic intID;

      // For vlsu ports use the write status of the corresponding interface
      if (port inside {SB_VLSU_VS2_RD0, SB_VLSU_VD_RD0, SB_VLSU_VD_WD0}) begin
        intID = 0;
      end else if (port inside {SB_VLSU_VS2_RD1, SB_VLSU_VD_RD1, SB_VLSU_VD_WD1}) begin
        intID = 1;
      // For non vlsu ports, use the vector length to find the interface id for write status checks
      end else begin
        intID = (vl_cnt_q[sb_id_i[port]] < vl_max_d[sb_id_i[port]]) ? 0 : 1;
      end

      // Enable the VRF port if the dependant instructions wrote in the previous cycle
      sb_enable_o[port] = sb_enable_i[port] && &(~scoreboard_q[sb_id_i[port]].deps | wrote_result_q[intID] | done_result_q[intID]) && (!(|scoreboard_q[sb_id_i[port]].deps) || !scoreboard_q[sb_id_i[port]].prevent_chaining);
`else
      // Enable the VRF port if the dependant instructions wrote in the previous cycle
      // sb_enable_o[port] - scoreboard check if you can use this vrf port
      // sb_enable_i[port] - some unit want to use this vrf port
      // sb_id_i[port] - the instruction ID that is using this port
      // &(~scoreboard_q[sb_id_i[port]].deps | wrote_result_q) - we either do not care about the other instructions, or they have wrote to vrf
      //                                                         in words: All dependencies must have written their results in the previous cycle
      sb_enable_o[port] = sb_enable_i[port] && &(~scoreboard_q[sb_id_i[port]].deps | wrote_result_q) && (!(|scoreboard_q[sb_id_i[port]].deps) || !scoreboard_q[sb_id_i[port]].prevent_chaining);
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
      automatic int unsigned port_idx = port - SB_VFU_VD_WD;
      if (sb_enable_o[port]) begin
        // VFU and VSLDU: intID derived from vl_cnt progress, vl_cnt updated every successful write
        if (port inside {SB_VFU_VD_WD, SB_VSLDU_VD_WD}) begin
          automatic logic  intID  = (vl_cnt_q[sb_id_i[port]] < vl_max_d[sb_id_i[port]]) ? 0 : 1;
          automatic int VRFWriteSize = narrow_q[sb_id_i[port]] ? VRFWordBWidth >> 1 : VRFWordBWidth;

          // Update vl_cnt if actually written into the VRF
          if (sb_wrote_result_i[port_idx])
            vl_cnt_d[sb_id_i[port]] += VRFWriteSize;

          wrote_result_narrowing_d[sb_id_i[port]] = sb_wrote_result_i[port_idx] ^ narrow_wide_q[sb_id_i[port]];
          wrote_result_d[intID][sb_id_i[port]]    = sb_wrote_result_i[port_idx] && (!narrow_wide_q[sb_id_i[port]] || wrote_result_narrowing_q[sb_id_i[port]]);
          if (vl_cnt_q[sb_id_i[port]] >= (vl_max_d[sb_id_i[port]] * (intID + 1) - VRFWriteSize))
            done_result_d[intID][sb_id_i[port]] = wrote_result_d[intID][sb_id_i[port]];
        end
        // VLSU: intID is fixed per interface (0 for WD0, 1 for WD1)
        if (port inside {SB_VLSU_VD_WD0, SB_VLSU_VD_WD1}) begin
          automatic int unsigned intID  = port - SB_VLSU_VD_WD0;
          wrote_result_narrowing_d[sb_id_i[port]] = sb_wrote_result_i[port_idx] ^ narrow_wide_q[sb_id_i[port]];
          wrote_result_d[intID][sb_id_i[port]]    = sb_wrote_result_i[port_idx] && (!narrow_wide_q[sb_id_i[port]] || wrote_result_narrowing_q[sb_id_i[port]]);
        end
      end
    end
`else
    // Store the decisions for all write-destination ports: VFU, VLSU, VSLDU
    for (int unsigned port = 0; port < NrVregfilePorts; port++) begin
      if (sb_enable_o[port] && (port inside {SB_VFU_VD_WD, SB_VLSU_VD_WD, SB_VSLDU_VD_WD})) begin
        automatic int unsigned port_idx = port - SB_VFU_VD_WD;
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
`ifdef VENTAGLIO
      vtl_table_d[vlsu_rsp_i.id]              = '0;
`endif
`ifdef DOUBLE_BW
      narrow_d[vlsu_rsp_i.id]                 = 1'b0;
      wrote_result_d[0][vlsu_rsp_i.id]        = 1'b0;
      wrote_result_d[1][vlsu_rsp_i.id]        = 1'b0;
      done_result_d[0][vlsu_rsp_i.id]         = 1'b0;
      done_result_d[1][vlsu_rsp_i.id]         = 1'b0;
      vl_cnt_d[vlsu_rsp_i.id]                 = '0;
`endif
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
      for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
        scoreboard_d[insn].deps[vsldu_rsp_i.id] = 1'b0;
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
      for (int unsigned insn = 0; insn < NrParallelInstructions; insn++)
        scoreboard_d[insn].deps[vtl_rsp_i.id] = 1'b0;
    end
`endif

    // Initialize the scoreboard metadata if we have a new instruction issued.
    if (spatz_req_valid && spatz_req.ex_unit != CON) begin
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

`ifdef VENTAGLIO
      // RAW hazard on the explicit index vreg used by vfxmacc.vrf
      if (spatz_req.op_vtl.use_vtl) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.op_vtl.idx_vreg].id] |=
            write_table_d[spatz_req.op_vtl.idx_vreg].valid;
        read_table_d[spatz_req.op_vtl.idx_vreg] = {spatz_req.id, 1'b1};
      end
`endif

      // WAW and WAR hazards
      if (spatz_req.use_vd) begin
        scoreboard_d[spatz_req.id].deps[write_table_d[spatz_req.vd].id] |= write_table_d[spatz_req.vd].valid;
        scoreboard_d[spatz_req.id].deps[read_table_d[spatz_req.vd].id] |= read_table_d[spatz_req.vd].valid;
        if (spatz_req.op inside {VLX}) begin
          scoreboard_d[spatz_req.id].deps = '0;
        end
        write_table_d[spatz_req.vd] = {spatz_req.id, 1'b1};
      end

      // Is this a risky instruction which should not chain?
      if (spatz_req.op inside {VSLIDEUP, VLSE, VLXE, VSSE, VSXE})
        scoreboard_d[spatz_req.id].prevent_chaining = 1'b1;

      // Is this a narrowing or widening instruction?
      if (spatz_req.op_arith.is_narrowing || spatz_req.op_arith.widen_vs1 || spatz_req.op_arith.widen_vs2)
        narrow_wide_d[spatz_req.id] = 1'b1;

`ifdef DOUBLE_BW
      narrow_d[spatz_req.id] = spatz_req.op_arith.is_narrowing;

      // Track request vl for vector chaining, used only for DOUBLE_BW
      // Default spatz uses 1-bit credit counter using wrote_result_q for chaining
      vl_max_d[spatz_req.id] = (spatz_req.vl >> 1) << spatz_req.vtype.vsew;
      vl_cnt_d[spatz_req.id] = '0;
`endif
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

  // We stall issuing a new instruction if the corresponding execution unit is
  // not ready yet. Or we have a change in LMUL, for which we need to let all the
  // units finish first before scheduling a new operation (to avoid running into
  // issues with the socreboard).
  logic stall, vfu_stall, vlsu_stall, vsldu_stall, vtl_stall, ope_stall, tile_stall;
  assign stall       = ((vfu_stall | vlsu_stall | vsldu_stall | vtl_stall | ope_stall) & req_buffer_valid) || tile_stall;
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
  assign tile_stall = req_buffer_valid && buffer_req_is_tile && (|tile_running_q);

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

`ifdef VENTAGLIO
          // Is this a VTL related instruction?
          if (spatz_req.op_vtl.use_vtl) begin
            spatz_req.op_vtl.sp_cfg = VTL_cfg_q;
          end
`endif
        end

        OPE: begin
          // VME matrix ops: embed current tile dimensions into the request
          spatz_req.op_tile.tn = tn_q;
          spatz_req.op_tile.tm = elen_t'(mtype_q.tm);
          spatz_req.op_tile.tk = elen_t'(mtype_q.tk);
          spatz_req.vstart     = '0;
        end

        LSU: begin
          // Overwrite vl and vstart in request (preserve vtype with vsew)
          spatz_req.vl     = vl_q;
          spatz_req.vstart = vstart_q;
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
    if (vfu_rsp_valid_i) begin
      running_insn_d[vfu_rsp_i.id] = 1'b0;
    end
    if (vlsu_rsp_valid_i) begin
      running_insn_d[vlsu_rsp_i.id] = 1'b0;
    end
    if (vsldu_rsp_valid_i) begin
      running_insn_d[vsldu_rsp_i.id] = 1'b0;
    end
`ifdef VENTAGLIO
    if (vtl_rsp_valid_i) begin
      running_insn_d[vtl_rsp_i.id] = 1'b0;
    end
`endif
  end: proc_next_insn_id

  // Respond to core about the decoded instruction.
  always_comb begin : acc_issue_resp
    issue_rsp_o = '0;

    // Is there something running on Spatz? If so, prevent Snitch from reading the fcsr register
    issue_rsp_o.isfloat = |running_insn_q;

    // We have a new valid instruction
    if (decoder_rsp_valid && !decoder_rsp.instr_illegal) begin
      // Accept the new instruction
      issue_rsp_o.accept = 1'b1;

      case (decoder_rsp.spatz_req.ex_unit)
        CON: begin
          issue_rsp_o.writeback = spatz_req.use_rd;
        end // CON
        VFU: begin
          // vtype is illegal -> illegal instruction (VME tile ops do not use vtype)
          if (vtype_q.vill && !(decoder_rsp.spatz_req.op inside
              {VTFMM, VTFMM_ALT, VTMMU, VTMMS, VTMV_VT, VTMV_TV, VTZERO, VTDISCARD})) begin
            issue_rsp_o.accept = 1'b0;
          end
        end // VFU
        LSU: begin
          issue_rsp_o.loadstore = 1'b1;
          // vtype is illegal -> illegal instruction
          if (vtype_q.vill) begin
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

  logic       rsp_valid_d;
  logic       rsp_ready_d;
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
      end else if (spatz_req.op == VMCFG && spatz_req.use_rd) begin
        // VMCFG: return the newly configured tile dimension
        rsp_d.id = spatz_req.rd;
        unique case (spatz_req.op_tile.cfg_sel)
          2'b01: rsp_d.data = elen_t'(tn_d);
          2'b10: rsp_d.data = elen_t'(mtype_d.tm);
          2'b11: rsp_d.data = elen_t'(mtype_d.tk);
          default: rsp_d.data = '0;
        endcase
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
