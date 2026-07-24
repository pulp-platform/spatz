// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich
//
// Ventaglio Gather datapath

module ventaglio_gather
  import spatz_pkg::*;
  import   vtl_pkg::*;
  #(
    parameter int unsigned NarrowDataWidth = VTGChannelWidth,
    parameter int unsigned WideDataWidth   = VTGChannelWidth * VENTAGLIO_WFACTOR

  ) (
    input  logic                                clk_i,
    input  logic                                rst_ni,
    input  logic                                testmode_i,
    // Read ports -- to VFU    (narrow)
    input  vrf_addr_t                           raddr_i,
    input  logic                                re_i,
    output vrf_data_t                           rdata_o,
    output logic                                rvalid_o,
    // Read ports -- from Buffer (wide)
    output vrf_addr_t  [VENTAGLIO_WFACTOR-1:0]  raddr_o,
    output logic       [VENTAGLIO_WFACTOR-1:0]  re_o,
    input  vrf_data_t  [VENTAGLIO_WFACTOR-1:0]  rdata_i,
    input  logic       [VENTAGLIO_WFACTOR-1:0]  rvalid_i,
    // control
    input  logic                                gather_done_i,
    // index
    input  vrf_data_t                           index_i,
    input  sp_cfg_t                             vtl_cfg_i,
    output logic                                load_index_o
  );

  // Include FF
  `include "common_cells/registers.svh"

  ////////////////////////////
  //     request logics     //
  ////////////////////////////
  vrf_addr_t raddr;

  always_comb begin : proc_read_addr
    raddr    = '0;
    raddr_o  = '0;
    re_o     = '0;
    case (vtl_cfg_i.sp_cfg_ratio)
        SP_RATIO_050: begin
            // one read request will map to 2 channels
            raddr = raddr_i << 1;
                raddr_o[0] = {raddr[$clog2(NrVRFWords)-1:1], 1'b0};
                raddr_o[1] = {raddr[$clog2(NrVRFWords)-1:1], 1'b1};
                if (re_i)
                    re_o[1:0] = 2'b11;
        end
        SP_RATIO_025: begin
            raddr = raddr_i << 2;
            raddr_o[0] = {raddr[$clog2(NrVRFWords)-1:2], 2'b00};
                raddr_o[1] = {raddr[$clog2(NrVRFWords)-1:2], 2'b01};
                raddr_o[2] = {raddr[$clog2(NrVRFWords)-1:2], 2'b10};
                raddr_o[3] = {raddr[$clog2(NrVRFWords)-1:2], 2'b11};
            if (re_i)
                re_o = '1;
        end
        default: raddr = raddr_i;      // need to consider more cased
    endcase // vtl_cfg_i.sp_cfg_ratio
  end // proc_read_addr

  ////////////////////////////
  //      gather logic      //
  ////////////////////////////

  // a 4-bit counter to track the requests
  // We support LMUL=8, so maximum we can have 16 beats
  logic [3:0] beat_cnt_d, beat_cnt_q;
  `FF(beat_cnt_q, beat_cnt_d, '0)

  // we check the last bit of req addr
  // if it changes, it's a new beat
  logic addr_last_bit_d, addr_last_bit_q;
  `FF(addr_last_bit_q, addr_last_bit_d, '0)

  logic beat_cnt_en;
  assign addr_last_bit_d = raddr_i[0];
  assign beat_cnt_en = (!gather_done_i) && (addr_last_bit_d ^ addr_last_bit_q) && re_i;

  vrf_data_t rdata_050, rdata_025;
  logic      rvalid_050, rvalid_025;
  logic      load_index_050, load_index_025;

  ventaglio_gather_datapath #(
    .NrEffElePerBlk(2),
    .NrElePerBlk   (4),
    .NrCh          (2),
    .IdxWidth      (2),
    .EleWidth      (32)
  ) i_core_2of4 (
    .clk_i,
    .rst_ni,
    .gather_done_i,
    .re_i,
    .raddr_i,
    .index_i,
    .rdata_i,
    .rvalid_i,
    .rdata_o      (rdata_050),
    .rvalid_o     (rvalid_050),
    .load_index_o (load_index_050)
  );

  ventaglio_gather_datapath #(
    .NrEffElePerBlk(1),  // <-- key change for 1:4
    .NrElePerBlk   (4),
    .NrCh          (4),  // <-- key change for 1:4
    .IdxWidth      (2),
    .EleWidth      (32)
  ) i_core_1of4 (
    .clk_i, .rst_ni,
    .gather_done_i,
    .re_i, .raddr_i,
    .index_i,
    .rdata_i, .rvalid_i,
    .rdata_o      (rdata_025),
    .rvalid_o     (rvalid_025),
    .load_index_o (load_index_025)
  );


  // Select active format
  always_comb begin
    unique case (vtl_cfg_i.sp_cfg_ratio)
      SP_RATIO_050: begin
        rdata_o      = rdata_050;
        rvalid_o     = rvalid_050;
        load_index_o = load_index_050;
      end
      SP_RATIO_025: begin
        rdata_o      = rdata_025;
        rvalid_o     = rvalid_025;
        load_index_o = load_index_025;
      end
      default: begin
        rdata_o      = '0;
        rvalid_o     = 1'b0;
        load_index_o = 1'b0;
      end
    endcase
  end


endmodule : ventaglio_gather
