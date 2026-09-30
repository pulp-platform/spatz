// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Danilo Cammarata <dcammarata@iis.ee.ethz.ch>
// Pei-Yu Lin       <peilin@ethz.ch>

module opope_accumulator
#(
  parameter int unsigned DATA_WIDTH = 32,
  parameter int unsigned DEPTH      = 4,
  parameter int unsigned RD_PORTS   = 2,
  parameter int unsigned WR_PORTS   = 2
) (
  input  logic                                clk_i              ,
  input  logic                                rst_ni             ,
  input  logic                                flush_i            ,
  input  logic                                iteration_change_i ,
  // interface with FMA Unit within OPE
  input  logic [DATA_WIDTH-1:0]               fma_wdata_i        ,
  input  logic [(DATA_WIDTH >> 3)-1:0]        fma_wen_i          ,
  input  logic [$clog2(DEPTH)-1:0]            fma_waddr_i        ,
  input  logic [$clog2(DEPTH)-1:0]            fma_raddr_i        ,
  output logic [DATA_WIDTH-1:0]               fma_rdata_o        ,
  // interface with VRF
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] vrf_wdata_i        ,
  input  logic [(DATA_WIDTH >> 3)-1:0]        vrf_wen_i          ,
  input  logic [$clog2(DEPTH)-1:0]            vrf_waddr_i        ,
  input  logic [$clog2(DEPTH)-1:0]            vrf_raddr_i        ,
  output logic [RD_PORTS-1:0][DATA_WIDTH-1:0] vrf_rdata_o        ,
  // interface with VLSU
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] vlsu_wdata_i       ,
  input  logic [(DATA_WIDTH >> 3)-1:0]        vlsu_wen_i         ,
  input  logic [$clog2(DEPTH)-1:0]            vlsu_waddr_i       ,
  input  logic [$clog2(DEPTH)-1:0]            vlsu_raddr_i       ,
  output logic [RD_PORTS-1:0][DATA_WIDTH-1:0] vlsu_rdata_o
);

  localparam int unsigned AddrWidth      = $clog2(DEPTH);
  localparam int unsigned WrPortIdxWidth = $clog2(WR_PORTS);
  localparam int unsigned RdPortIdxWidth = $clog2(RD_PORTS);

  logic [DEPTH-1:0][DATA_WIDTH-1:0] mem;
  logic [AddrWidth-1:0] vrf_waddr_base, vrf_raddr_base;
  logic [AddrWidth-1:0] vlsu_waddr_base, vlsu_raddr_base;

  always_comb begin : align_vrf_addr
    if (WR_PORTS <= 1)
      vrf_waddr_base = vrf_waddr_i;
    else if (WrPortIdxWidth >= AddrWidth)
      vrf_waddr_base = AddrWidth'(0);
    else
      vrf_waddr_base = {vrf_waddr_i[AddrWidth-1:WrPortIdxWidth], {WrPortIdxWidth{1'b0}}};

    if (RD_PORTS <= 1)
      vrf_raddr_base = vrf_raddr_i;
    else if (RdPortIdxWidth >= AddrWidth)
      vrf_raddr_base = AddrWidth'(0);
    else
      vrf_raddr_base = {vrf_raddr_i[AddrWidth-1:RdPortIdxWidth], {RdPortIdxWidth{1'b0}}};
  end

  always_comb begin : align_vlsu_addr
    if (WR_PORTS <= 1)
      vlsu_waddr_base = vlsu_waddr_i;
    else if (WrPortIdxWidth >= AddrWidth)
      vlsu_waddr_base = AddrWidth'(0);
    else
      vlsu_waddr_base = {vlsu_waddr_i[AddrWidth-1:WrPortIdxWidth], {WrPortIdxWidth{1'b0}}};

    if (RD_PORTS <= 1)
      vlsu_raddr_base = vlsu_raddr_i;
    else if (RdPortIdxWidth >= AddrWidth)
      vlsu_raddr_base = AddrWidth'(0);
    else
      vlsu_raddr_base = {vlsu_raddr_i[AddrWidth-1:RdPortIdxWidth], {RdPortIdxWidth{1'b0}}};
  end

  assign fma_rdata_o  = mem[fma_raddr_i];
  assign vrf_rdata_o  = mem[vrf_raddr_base +: RD_PORTS];
  assign vlsu_rdata_o = mem[vlsu_raddr_base +: RD_PORTS];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      mem <= '0;
    end else if (flush_i || iteration_change_i) begin
      mem <= '0;
    end else begin
      for (int unsigned byte_idx = 0; byte_idx < DATA_WIDTH >> 3; byte_idx++) begin
        if (fma_wen_i[byte_idx])
          mem[fma_waddr_i][byte_idx << 3 +: 8] <= fma_wdata_i[byte_idx << 3 +: 8];
      end
      for (int unsigned port = 0; port < WR_PORTS; port++) begin
        for (int unsigned byte_idx = 0; byte_idx < DATA_WIDTH >> 3; byte_idx++) begin
          if (vrf_wen_i[byte_idx])
            mem[vrf_waddr_base + port][byte_idx << 3 +: 8] <= vrf_wdata_i[port][byte_idx << 3 +: 8];
          if (vlsu_wen_i[byte_idx])
            mem[vlsu_waddr_base + port][byte_idx << 3 +: 8] <= vlsu_wdata_i[port][byte_idx << 3 +: 8];
        end
      end
    end
  end

  if ((DEPTH    == 0) || ((DEPTH    & (DEPTH   -1)) != 0)) $error("[opope_accumulator] DEPTH must be power of 2.");
  if ((RD_PORTS == 0) || ((RD_PORTS & (RD_PORTS-1)) != 0)) $error("[opope_accumulator] RD_PORTS must be power of 2.");
  if ((WR_PORTS == 0) || ((WR_PORTS & (WR_PORTS-1)) != 0)) $error("[opope_accumulator] WR_PORTS must be power of 2.");
  if (RD_PORTS > DEPTH) $error("[opope_accumulator] RD_PORTS must be <= DEPTH.");
  if (WR_PORTS > DEPTH) $error("[opope_accumulator] WR_PORTS must be <= DEPTH.");

endmodule : opope_accumulator
