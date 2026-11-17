// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Danilo Cammarata <dcammarata@iis.ee.ethz.ch>
// Pei-Yu Lin       <peilin@ethz.ch>

module opope_accumulator
#(
  parameter int unsigned DATA_WIDTH = 32,
  parameter int unsigned MASK_GRANULE_BYTES = 1,
  parameter int unsigned DEPTH      = 4,
  parameter int unsigned RD_PORTS   = 2,
  parameter int unsigned WR_PORTS   = 2
) (
  input  logic                                clk_i              ,
  // interface with FPU Array within OPE
  input  logic [DATA_WIDTH-1:0]               fpu_wdata_i        ,
  input  logic [(DATA_WIDTH >> 3)/MASK_GRANULE_BYTES-1:0] fpu_wen_i,
  input  logic [$clog2(DEPTH)-1:0]            fpu_waddr_i        ,
  input  logic [$clog2(DEPTH)-1:0]            fpu_raddr_i        ,
  output logic [DATA_WIDTH-1:0]               fpu_rdata_o        ,
  // interface with VRF
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] vrf_wdata_i        ,
  input  logic [(DATA_WIDTH >> 3)/MASK_GRANULE_BYTES-1:0] vrf_wen_i,
  input  logic [$clog2(DEPTH)-1:0]            vrf_waddr_i        ,
  input  logic [$clog2(DEPTH)-1:0]            vrf_raddr_i        ,
  output logic [RD_PORTS-1:0][DATA_WIDTH-1:0] vrf_rdata_o        ,
  // interface with VLSU
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] vlsu_wdata_i       ,
  input  logic [(DATA_WIDTH >> 3)/MASK_GRANULE_BYTES-1:0] vlsu_wen_i,
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

  assign fpu_rdata_o  = mem[fpu_raddr_i];
  assign vrf_rdata_o  = mem[vrf_raddr_base +: RD_PORTS];
  assign vlsu_rdata_o = mem[vlsu_raddr_base +: RD_PORTS];

  // The data array deliberately has no reset or global clear.  Reset/discard/VTZERO
  // are represented by granule-level zero metadata in spatz_ope.  Physically
  // clearing this array created a single control net driving every accumulator bit
  // and its clock gate, which is both unnecessary and a severe timing bottleneck.
  always_ff @(posedge clk_i) begin
    for (int unsigned mask_idx = 0;
         mask_idx < (DATA_WIDTH >> 3) / MASK_GRANULE_BYTES; mask_idx++) begin
      if (fpu_wen_i[mask_idx])
        mem[fpu_waddr_i][mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8] <=
            fpu_wdata_i[mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8];
    end
    for (int unsigned port = 0; port < WR_PORTS; port++) begin
      for (int unsigned mask_idx = 0;
           mask_idx < (DATA_WIDTH >> 3) / MASK_GRANULE_BYTES; mask_idx++) begin
        if (vrf_wen_i[mask_idx])
          mem[vrf_waddr_base + port]
              [mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8] <=
              vrf_wdata_i[port][mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8];
        if (vlsu_wen_i[mask_idx])
          mem[vlsu_waddr_base + port]
              [mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8] <=
              vlsu_wdata_i[port][mask_idx*MASK_GRANULE_BYTES*8 +: MASK_GRANULE_BYTES*8];
      end
    end
  end

  if ((DEPTH    == 0) || ((DEPTH    & (DEPTH   -1)) != 0)) $error("[opope_accumulator] DEPTH must be power of 2.");
  if ((RD_PORTS == 0) || ((RD_PORTS & (RD_PORTS-1)) != 0)) $error("[opope_accumulator] RD_PORTS must be power of 2.");
  if ((WR_PORTS == 0) || ((WR_PORTS & (WR_PORTS-1)) != 0)) $error("[opope_accumulator] WR_PORTS must be power of 2.");
  if (RD_PORTS > DEPTH) $error("[opope_accumulator] RD_PORTS must be <= DEPTH.");
  if (WR_PORTS > DEPTH) $error("[opope_accumulator] WR_PORTS must be <= DEPTH.");
  if ((MASK_GRANULE_BYTES == 0) ||
      (((DATA_WIDTH >> 3) % MASK_GRANULE_BYTES) != 0))
    $error("[opope_accumulator] MASK_GRANULE_BYTES must divide DATA_WIDTH/8.");

endmodule : opope_accumulator
