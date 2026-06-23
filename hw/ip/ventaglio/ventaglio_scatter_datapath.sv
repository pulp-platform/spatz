// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich

module ventaglio_scatter_datapath
  import spatz_pkg::*;
  import vtl_pkg::*;
#(
  parameter int unsigned NrEffElePerBlk = 2,
  parameter int unsigned NrElePerBlk    = 4,
  parameter int unsigned NrCh           = 2,
  parameter int unsigned IdxWidth       = 2,
  parameter int unsigned EleWidth       = 32
) (
  input  logic                               clk_i,
  input  logic                               rst_ni,

  // NEW: gate activity (only active ratio should advance state / request loads)
  input  logic                               active_i,

  input  logic                               scatter_done_i,
  input  logic                               we_i,
  input  vrf_addr_t                          waddr_i,

  input  vrf_data_t                          wdata_i,
  input  vrf_be_t                            wbe_i,

  input  vrf_data_t                          index_i,
  input  logic                               index_valid_i,

  // NEW: shared registered index from top
  input  vrf_data_t                          index_q_i,

  // NEW: request to load the shared index register in top
  output logic                               index_load_req_o,
  input  logic                        [4:0]  num_beats_per_op_i,

  input  logic      [VENTAGLIO_WFACTOR-1:0]  wvalid_i,

  output vrf_data_t [VENTAGLIO_WFACTOR-1:0]  wdata_o,
  output vrf_be_t   [VENTAGLIO_WFACTOR-1:0]  wbe_o,
  output logic                               wvalid_o
);

  `include "common_cells/registers.svh"

  ////////////////////////////
  //     beat tracking      //
  ////////////////////////////
  logic [3:0] beat_cnt_d, beat_cnt_q;
  `FF(beat_cnt_q, beat_cnt_d, '0)

  logic addr_last_bit_d, addr_last_bit_q;
  `FF(addr_last_bit_q, addr_last_bit_d, '0)

  logic beat_cnt_en;

  // Freeze addr_last_bit when inactive so it doesn't drift
  assign addr_last_bit_d = active_i ? waddr_i[0] : addr_last_bit_q;

  // Only advance on traffic when active
  assign beat_cnt_en = active_i
                    && (!scatter_done_i)
                    && (addr_last_bit_d ^ addr_last_bit_q)
                    && we_i;

  localparam int unsigned EleWidthB       = EleWidth / 8;
  localparam int unsigned NrBlksPerBeat   = (VRFWordWidth * NrCh) / (NrElePerBlk * EleWidth);
  localparam int unsigned NrBeatsPerInput = EleWidth / IdxWidth;

  always_comb begin
    // Default hold
    beat_cnt_d = beat_cnt_q;

    // Keep counters quiescent when inactive (safe re-entry)
    if (!active_i) begin
      beat_cnt_d = '0;
    end else begin
      if (beat_cnt_en) begin
        if (beat_cnt_q == num_beats_per_op_i-1) begin
          beat_cnt_d = '0;
        end else begin
          beat_cnt_d = beat_cnt_q + 1'b1;
        end
      end

      if (scatter_done_i) beat_cnt_d = '0;
    end
  end

  ////////////////////////////
  //   shared index request //
  ////////////////////////////
  // Preserve your original “refresh” conditions as a request to top-level FF
  always_comb begin
    index_load_req_o = 1'b0;

    if (active_i
        && index_valid_i
        && ( /*(beat_cnt_q == '0)
             || */scatter_done_i
             || (beat_cnt_q == (num_beats_per_op_i-1)-1) ) ) begin
      index_load_req_o = 1'b1;
    end
  end

  ////////////////////////////
  //     scatter logic      //
  ////////////////////////////

  // Unpack indices from shared registered index
  logic [NrBeatsPerInput-1:0][NrBlksPerBeat-1:0][NrEffElePerBlk-1:0][IdxWidth-1:0] idx;
  for (genvar beat = 0; beat < NrBeatsPerInput; beat++) begin : gen_unpack_idx_beat
    for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_unpack_idx_blk
      for (genvar ele = 0; ele < NrEffElePerBlk; ele++) begin : gen_unpack_idx_ele
        assign idx[beat][blk][ele] =
          index_q_i[beat*NrBlksPerBeat*NrEffElePerBlk*IdxWidth +
                    blk*NrEffElePerBlk*IdxWidth +
                    ele*IdxWidth +: IdxWidth];
      end
    end
  end

  // Pre-scatter: slice incoming narrow data into [blk][eff-ele]
  logic [NrBlksPerBeat-1:0][NrEffElePerBlk-1:0][EleWidth-1:0]  wdata_pre_scatter;
  logic [NrBlksPerBeat-1:0][NrEffElePerBlk-1:0][EleWidthB-1:0] wbe_pre_scatter;

  for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_pre_scatter_blk
    for (genvar ele = 0; ele < NrEffElePerBlk; ele++) begin : gen_pre_scatter_ele
      assign wdata_pre_scatter[blk][ele] =
        wdata_i[blk*NrEffElePerBlk*EleWidth + ele*EleWidth +: EleWidth];
      assign wbe_pre_scatter[blk][ele] =
        wbe_i[blk*NrEffElePerBlk*EleWidthB + ele*EleWidthB +: EleWidthB];
    end
  end

  // Post-scatter: place effective elements into full block slots [0..NrElePerBlk-1]
  logic [NrBlksPerBeat-1:0][NrElePerBlk-1:0][EleWidth-1:0]  wdata_post_scatter;
  logic [NrBlksPerBeat-1:0][NrElePerBlk-1:0][EleWidthB-1:0] wbe_post_scatter;

  always_comb begin
    wdata_post_scatter = '0;
    wbe_post_scatter   = '0;

    if (active_i) begin
      for (int blk = 0; blk < NrBlksPerBeat; blk++) begin
        for (int ele = 0; ele < NrEffElePerBlk; ele++) begin
          wdata_post_scatter[blk][ idx[beat_cnt_d][blk][ele] ] = wdata_pre_scatter[blk][ele];
          wbe_post_scatter[blk][ idx[beat_cnt_d][blk][ele] ]   = wbe_pre_scatter[blk][ele];
        end
      end
    end
  end

  // Flatten to per-channel wide words
  logic [VRFWordWidth*NrCh-1:0]   flatten_wdata;
  logic [VRFWordBWidth*NrCh-1:0] flatten_wbe;

  for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_flatten_blk
    for (genvar ele = 0; ele < NrElePerBlk; ele++) begin : gen_flatten_ele
      assign flatten_wdata[blk*NrElePerBlk*EleWidth + ele*EleWidth +: EleWidth] =
        wdata_post_scatter[blk][ele];
      assign flatten_wbe[blk*NrElePerBlk*EleWidthB + ele*EleWidthB +: EleWidthB] =
        wbe_post_scatter[blk][ele];
    end
  end

  // Drive output arrays: first NrCh channels get data, rest are zero
  for (genvar ch = 0; ch < VENTAGLIO_WFACTOR; ch++) begin : gen_out
    if (ch < NrCh) begin : gen_out_active
      assign wdata_o[ch] = active_i ? flatten_wdata[ch*VRFWordWidth +: VRFWordWidth] : '0;
      assign wbe_o[ch]   = active_i ? flatten_wbe  [ch*VRFWordBWidth +: VRFWordBWidth] : '0;
    end else begin : gen_out_zero
      assign wdata_o[ch] = '0;
      assign wbe_o[ch]   = '0;
    end
  end

  // Valid: AND only the channels this core uses (and gate when inactive)
  assign wvalid_o = active_i ? (&wvalid_i[NrCh-1:0]) : 1'b0;

endmodule : ventaglio_scatter_datapath
