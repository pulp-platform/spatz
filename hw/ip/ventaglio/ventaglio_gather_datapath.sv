// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich

module ventaglio_gather_datapath
  import spatz_pkg::*;
  import   vtl_pkg::*; #(
  parameter int unsigned NrEffElePerBlk = 2,
  parameter int unsigned NrElePerBlk    = 4,
  parameter int unsigned NrCh           = 2,
  parameter int unsigned IdxWidth       = 2,
  parameter int unsigned EleWidth       = 32
) (
  input  logic                               clk_i,
  input  logic                               rst_ni,
  input  logic                               gather_done_i,
  input  logic                               re_i,
  input  vrf_addr_t                          raddr_i,
  input  vrf_data_t                          index_i,
  input  vrf_data_t [VENTAGLIO_WFACTOR-1:0]  rdata_i,
  input  logic      [VENTAGLIO_WFACTOR-1:0]  rvalid_i,

  output vrf_data_t                          rdata_o,
  output logic                               rvalid_o,
  output logic                               load_index_o
);

  `include "common_cells/registers.svh"

  // Beat tracking (unchanged)
  logic [3:0] beat_cnt_d, beat_cnt_q;
  `FF(beat_cnt_q, beat_cnt_d, '0)

  logic addr_last_bit_d, addr_last_bit_q;
  `FF(addr_last_bit_q, addr_last_bit_d, '0)

  logic beat_cnt_en;
  assign addr_last_bit_d = raddr_i[0];
  assign beat_cnt_en     = (!gather_done_i) && (addr_last_bit_d ^ addr_last_bit_q) && re_i;

  // Derived constants for THIS core instance
  localparam int unsigned NrBlksPerBeat   = (VRFWordWidth * NrCh) / (NrElePerBlk * EleWidth);
  localparam int unsigned NrBeatsPerInput = EleWidth / IdxWidth;

  // index loading / beat counter (same structure as yours)
  always_comb begin
    load_index_o = 1'b0;
    beat_cnt_d   = beat_cnt_q;

    if (beat_cnt_en) begin
      if (beat_cnt_q == NrBeatsPerInput-1) begin
        beat_cnt_d   = '0;
        load_index_o = 1'b1;
      end else begin
        beat_cnt_d = beat_cnt_q + 1'b1;
      end
    end

    if (gather_done_i) beat_cnt_d = '0;
  end

  // Unpack indices (same style as yours)
  logic [NrBeatsPerInput-1:0][NrBlksPerBeat-1:0][NrEffElePerBlk-1:0][IdxWidth-1:0] idx;
  for (genvar beat = 0; beat < NrBeatsPerInput; beat++) begin : gen_unpack_idx_beat
    for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_unpack_idx_blk
      for (genvar ele = 0; ele < NrEffElePerBlk; ele++) begin : gen_unpack_idx_ele
        assign idx[beat][blk][ele] =
          index_i[beat*NrBlksPerBeat*NrEffElePerBlk*IdxWidth +
                  blk*NrEffElePerBlk*IdxWidth +
                  ele*IdxWidth +: IdxWidth];
      end
    end
  end

  // Flatten read data for THIS core instance’s channel count
  logic [VRFWordWidth*NrCh-1:0] flatten_rdata;
  for (genvar ch = 0; ch < NrCh; ch++) begin : gen_flatten_rdata
    assign flatten_rdata[ch*VRFWordWidth +: VRFWordWidth] = rdata_i[ch];
  end

  // Pre-gather blocks
  logic [NrBlksPerBeat-1:0][NrElePerBlk-1:0][EleWidth-1:0] rdata_pre_gather;
  for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_pre_gather_blk
    for (genvar ele = 0; ele < NrElePerBlk; ele++) begin : gen_pre_gather_ele
      assign rdata_pre_gather[blk][ele] =
        flatten_rdata[blk*NrElePerBlk*EleWidth + ele*EleWidth +: EleWidth];
    end
  end

  // Post-gather blocks + pack output
  for (genvar blk = 0; blk < NrBlksPerBeat; blk++) begin : gen_post_gather_blk
    for (genvar ele = 0; ele < NrEffElePerBlk; ele++) begin : gen_post_gather_ele
      wire [EleWidth-1:0] sel =
        rdata_pre_gather[blk][ idx[beat_cnt_d][blk][ele] ];
      assign rdata_o[blk*NrEffElePerBlk*EleWidth + ele*EleWidth +: EleWidth] = sel;
    end
  end

  // Valid: AND only the channels this core uses
  assign rvalid_o = &rvalid_i[NrCh-1:0];

endmodule
