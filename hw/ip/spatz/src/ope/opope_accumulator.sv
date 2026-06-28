// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Danilo Cammarata <dcammarata@iis.ee.ethz.ch>

// TODO: Review flush and iteration_change
// TODO: Review ifdef for assertions

module opope_accumulator
  import opope_pkg::*;
#(
  parameter int unsigned   DATA_WIDTH   = BITW,
  parameter int unsigned   DEPTH        = REG_PER_CE,
  parameter int unsigned   RD_PORTS     = ACC_RD_PORTS,
  parameter int unsigned   WR_PORTS     = ACC_WR_PORTS
)(
  input  logic                                clk_i              ,
  input  logic                                rst_ni             ,
  input  logic                                flush_i            ,
  input  logic                                iteration_change_i ,
  input  logic [WR_PORTS-1:0][DATA_WIDTH-1:0] wdata_i            ,
  input  logic                                wen_i              ,
  input  logic [$clog2(DEPTH)-1:0]            waddr_i            ,
  input  logic [$clog2(DEPTH)-1:0]            raddr_i            ,
  input  logic                                ext_ld_i           ,
  output logic [RD_PORTS-1:0][DATA_WIDTH-1:0] rdata_o            
);

  logic [DEPTH-1:0][DATA_WIDTH-1:0] mem_d, mem_q;
  logic [$clog2(DEPTH)-1:0] waddr_mask;
  logic [$clog2(DEPTH)-1:0] raddr_mask;
  
  assign waddr_mask = {$clog2(DEPTH){1'b1}} << $clog2(WR_PORTS);
  assign raddr_mask = {$clog2(DEPTH){1'b1}} << $clog2(RD_PORTS);
  assign rdata_o  = mem_q[(raddr_i & raddr_mask) +: RD_PORTS];
  
  always_comb begin 
    mem_d = mem_q;
    if (flush_i || iteration_change_i ) mem_d = '0;
    else if (wen_i && ext_ld_i) mem_d[(waddr_i & waddr_mask) +: WR_PORTS] = wdata_i;
    else if (wen_i            ) mem_d[waddr_i] = wdata_i[0];
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (~rst_ni) mem_q <= '0;
    else         mem_q <= mem_d;
  end

  // Assertions
  if ((RD_PORTS == 0) || ((RD_PORTS & (RD_PORTS - 1)) != 0)) $error("[opope_accumulator] RD_PORTS must be a power of 2.\n");
  if ((WR_PORTS == 0) || ((WR_PORTS & (WR_PORTS - 1)) != 0)) $error("[opope_accumulator] WR_PORTS must be a power of 2.\n");
  if ((DEPTH    == 0) || ((DEPTH    & (DEPTH - 1   )) != 0)) $error("[opope_accumulator] DEPTH    must be a power of 2.\n");

  if (RD_PORTS > DEPTH) $error("[opope_accumulator] RD_PORTS must be smaller than or equal to DEPTH.\n");
  if (WR_PORTS > DEPTH) $error("[opope_accumulator] RD_PORTS must be smaller than or equal to DEPTH.\n");
  
endmodule : opope_accumulator  