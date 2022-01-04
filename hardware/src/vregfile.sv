// Copyright 2022 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// Generic vector register file that makes use of latches to store data.

module vregfile #(
  parameter int unsigned NrReadPorts  = 0,
  parameter int unsigned NrWritePorts = 0,
  parameter int unsigned NrRegs       = 32,
  parameter logic        EnableWBE    = 1,
  parameter int unsigned RegWidth     = 32,
  parameter int unsigned ElemWidth    = 32,
  parameter int unsigned VAddrWidth   = 5
) (
  input  logic clk_i,
  input  logic rst_ni,
  // Write ports
  input  logic [NrWritePorts-1:0][VAddrWidth-1:0]  waddr_i,
  input  logic [NrWritePorts-1:0][ElemWidth-1:0]   wdata_i,
  input  logic [NrWritePorts-1:0]                  we_i,
  input  logic [NrWritePorts-1:0][ElemWidth/8-1:0] wbe_i,
  // Read ports
  input  logic [NrReadPorts-1:0][VAddrWidth-1:0]   raddr_i,
  output logic [NrReadPorts-1:0][ElemWidth-1:0]    rdata_o
);

  /////////////////
  // Localparams //
  /////////////////

  localparam int unsigned NumElemPerReg       = RegWidth/ElemWidth;
  localparam int unsigned NumBytesPerElem     = EnableWBE ? ElemWidth/8 : 1;
  localparam int unsigned NumBytesPerElemSize = EnableWBE ? 8 : ElemWidth;

  /////////////
  // Signals //
  /////////////

  // Gated clock
  logic clk;

  // Register file memory
  logic [NrRegs-1:0][NumElemPerReg-1:0][NumBytesPerElem-1:0][NumBytesPerElemSize-1:0] mem;

  // Write data sampling
  logic [NrWritePorts-1:0][NumBytesPerElem-1:0][NumBytesPerElemSize-1:0]       wdata_q;
  logic [NrRegs-1:0][NumElemPerReg-1:0][NumBytesPerElem-1:0][NrWritePorts-1:0] waddr_onehot;

  ///////////////////
  // Register File //
  ///////////////////

  // Main vregfile clock gate
  tc_clk_gating i_regfile_cg (
    .clk_i    (clk_i),
    .en_i     (|we_i),
    .test_en_i(1'b0),
    .clk_o    (clk)
  );

  // Sample Input Data
  for (genvar i = 0; i < NrWritePorts; i++) begin
    always_ff @(posedge clk) begin
      wdata_q[i] <= wdata_i[i];
    end
  end

  // Create latch clock signal
  for (genvar i = 0; i < NrWritePorts; i++) begin
    // Select which destination bytes to write into
    for (genvar j = 0; j < NrRegs; j++) begin
      for (genvar k = 0; k < NumElemPerReg; k++) begin
        for (genvar l = 0; l < NumBytesPerElem; l++) begin
          if (NumElemPerReg == 'd1) begin
            assign waddr_onehot[j][k][l][i] = (we_i[i] & ((wbe_i[i][l] & EnableWBE) | ~EnableWBE)
                                                       & waddr_i[i][VAddrWidth-1:0] == j);
          end else begin
            assign waddr_onehot[j][k][l][i] = (we_i[i] & ((wbe_i[i][l] & EnableWBE) | ~EnableWBE)
                                                       & waddr_i[i][VAddrWidth-1:$clog2(NumElemPerReg)] == j
                                                       & waddr_i[i][$clog2(NumElemPerReg)-1:0] == k);
          end
        end
      end
    end
  end

  // Store new data to memory
  for (genvar i = 0; i < NrRegs; i++) begin
    for (genvar j = 0; j < NumElemPerReg; j++) begin
      for (genvar k = 0; k < NumBytesPerElem; k++) begin
        logic clk_latch;

        tc_clk_gating i_regfile_cg (
          .clk_i    (clk),
          .en_i     (|waddr_onehot[i][j][k]),
          .test_en_i(1'b0),
          .clk_o    (clk_latch)
        );

        /* verilator lint_off NOLATCH */
        always_latch begin
          // TODO: Generalize for more than one write port
          if (clk_latch) begin
            mem[i][j][k] <= wdata_q[0][k];
          end
        end
        /* verilator lint_on NOLATCH */
      end
    end
  end

  // Read data from memory
  for (genvar i = 0; i < NrReadPorts; i++) begin
    if (NumElemPerReg == 'd1) begin
      assign rdata_o[i] = mem[raddr_i[i]];
    end else begin
      assign rdata_o[i] = mem[raddr_i[i][VAddrWidth-1:$clog2(NumElemPerReg)]][raddr_i[i][$clog2(NumElemPerReg)-1:0]];
    end
  end

endmodule : vregfile
