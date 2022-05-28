// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// Generic vector register file that makes use of latches to store data.

module vregfile import spatz_pkg::*; #(
    parameter int  unsigned NrReadPorts = 0,
    // Derived parameters.  Do not change!
    parameter type          addr_t      = logic[$clog2(NRVREG)-1:0],
    parameter type          data_t      = logic [VRFWordWidth-1:0],
    parameter type          strb_t      = logic [VRFWordWidth/8-1:0]
  ) (
    input  logic                    clk_i,
    input  logic                    rst_ni,
    // Write ports
    input  addr_t                   waddr_i,
    input  data_t                   wdata_i,
    input  logic                    we_i,
    input  strb_t                   wbe_i,
    // Read ports
    input  addr_t [NrReadPorts-1:0] raddr_i,
    input  logic  [NrReadPorts-1:0] re_i,
    output data_t [NrReadPorts-1:0] rdata_o
  );

  /////////////
  // Signals //
  /////////////

  // Gated clock
  logic clk;

  // Register file memory
  logic [NRVREG-1:0][VRFWordWidth/8-1:0][7:0] mem;

  // Write data sampling
  data_t                                  wdata_q;
  logic  [NRVREG-1:0][VRFWordWidth/8-1:0] waddr_onehot;

  ///////////////////
  // Register File //
  ///////////////////

  // Main vregfile clock gate
  tc_clk_gating i_regfile_cg (
    .clk_i    (clk_i),
    .en_i     (|we_i),
    .test_en_i(1'b0 ),
    .clk_o    (clk  )
  );

  // Sample Input Data
  always_ff @(posedge clk) begin: proc_wdata_q
    wdata_q <= wdata_i;
  end: proc_wdata_q

  // Select which destination bytes to write into
  for (genvar vreg = 0; vreg < NRVREG; vreg++) begin: gen_waddr_onehot
    // Create latch clock signal
    for (genvar b = 0; b < VRFWordWidth/8; b++) begin
      assign waddr_onehot[vreg][b] = we_i && wbe_i[b] && waddr_i == vreg;
    end
  end: gen_waddr_onehot

  // Store new data to memory
  for (genvar vreg = 0; vreg < NRVREG; vreg++) begin: gen_write_mem
    for (genvar b = 0; b < VRFWordBWidth; b++) begin
      logic clk_latch;

      tc_clk_gating i_regfile_cg (
        .clk_i    (clk                  ),
        .en_i     (waddr_onehot[vreg][b]),
        .test_en_i(1'b0                 ),
        .clk_o    (clk_latch            )
      );

      /* verilator lint_off NOLATCH */
      always_latch begin
        if (clk_latch)
          mem[vreg][b] <= wdata_q[b*8 +: 8];
      end
    /* verilator lint_on NOLATCH */
    end
  end: gen_write_mem

  // Read data from memory
  for (genvar port = 0; port < NrReadPorts; port++) begin: gen_read_mem
    assign rdata_o[port] = re_i[port] ? mem[raddr_i[port]] : 'x;
  end: gen_read_mem

endmodule : vregfile
