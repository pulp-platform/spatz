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
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] wdata_i            ,
  input  logic [(DATA_WIDTH >> 3)-1:0]        wen_i              ,
  input  logic [$clog2(DEPTH)-1:0]            waddr_i            ,
  input  logic [$clog2(DEPTH)-1:0]            raddr_i            ,
  input  logic [$clog2(DEPTH)-1:0]            scalar_raddr_i     ,
  input  logic                                ext_ld_i           ,
  output logic [RD_PORTS-1:0][DATA_WIDTH-1:0] rdata_o            ,
  output logic [DATA_WIDTH-1:0]               scalar_rdata_o
);

  logic [DEPTH-1:0][DATA_WIDTH-1:0] mem;
  logic [$clog2(DEPTH)-1:0] waddr_base;
  logic [$clog2(DEPTH)-1:0] raddr_base;

  // External accesses align to a power-of-two accumulator-bank group.
  always_comb begin : gen_waddr
    if (WR_PORTS <= 1)                      // single
      waddr_base = waddr_i;
    else if ($clog2(WR_PORTS) >= $clog2(DEPTH)) // full
      waddr_base = '0;
    else                                    // align
      waddr_base = {waddr_i[$clog2(DEPTH)-1:$clog2(WR_PORTS)], {$clog2(WR_PORTS){1'b0}}};
  end

  always_comb begin : gen_raddr
    if (RD_PORTS <= 1)                      // single             
      raddr_base = raddr_i;
    else if ($clog2(RD_PORTS) >= $clog2(DEPTH)) // full
      raddr_base = '0;
    else                                    // align
      raddr_base = {raddr_i[$clog2(DEPTH)-1:$clog2(RD_PORTS)], {$clog2(RD_PORTS){1'b0}}};
  end

  assign rdata_o    = mem[raddr_base +: RD_PORTS];
  assign scalar_rdata_o = mem[scalar_raddr_i];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      mem <= '0;
    end else if (flush_i || iteration_change_i) begin
      mem <= '0;
    end else if (|wen_i) begin
      if (ext_ld_i) begin
        for (int unsigned port = 0; port < WR_PORTS; port++) begin
          for (int unsigned byte_idx = 0; byte_idx < DATA_WIDTH >> 3; byte_idx++) begin
            if (wen_i[byte_idx])
              mem[waddr_base + port][byte_idx << 3 +: 8] <= wdata_i[port][byte_idx << 3 +: 8];
          end
        end
      end else begin
        for (int unsigned byte_idx = 0; byte_idx < DATA_WIDTH >> 3; byte_idx++) begin
          if (wen_i[byte_idx])
            mem[waddr_i][byte_idx << 3 +: 8] <= wdata_i[0][byte_idx << 3 +: 8];
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
