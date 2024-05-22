// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Cyril Koenig, ETH Zurich

module vregfile import spatz_pkg::*; #(
    parameter int  unsigned NrReadPorts = 0,
    parameter int  unsigned NrWords     = NRVREG,
    parameter int  unsigned WordWidth   = VRFWordWidth,
    // Derived parameters.  Do not change!
    parameter type          addr_t      = logic[$clog2(NrWords)-1:0],
    parameter type          data_t      = logic [WordWidth-1:0],
    parameter type          strb_t      = logic [WordWidth/8-1:0]
  ) (
    input  logic                    clk_i,
    input  logic                    rst_ni,
    input  logic                    testmode_i,
    // Write ports
    input  addr_t                   waddr_i,
    input  data_t                   wdata_i,
    input  logic                    we_i,
    input  strb_t                   wbe_i,
    // Read ports
    input  addr_t [NrReadPorts-1:0] raddr_i,
    output data_t [NrReadPorts-1:0] rdata_o
  );

  // Just reuse snitch_regfile that has a FPGA implementation

  snitch_regfile #(
    .DATA_WIDTH(WordWidth),
    .NR_READ_PORTS(NrReadPorts),
    .NR_WRITE_PORTS(1),
    .ZERO_REG_ZERO(0),
    .ADDR_WIDTH($bits(addr_t))
  ) i_snitch_regfile (
    .clk_i,
    .raddr_i,
    .rdata_o,
    .waddr_i,
    .wdata_i,
    .we_i
  );

endmodule : vregfile
