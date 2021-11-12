// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vrf import spatz_pkg::*; #(
  parameter int unsigned DATA_WIDTH = 32,
  parameter int unsigned NR_READ_PORTS = 0,
  parameter int unsigned NR_WRITE_PORTS = 0
) (
  input  logic clk_i,
  // Write ports
  input  vreg_addr_t [NR_WRITE_PORTS-1:0] waddr_i,
  input  vreg_data_t [NR_WRITE_PORTS-1:0] wdata_i,
  input  logic       [NR_WRITE_PORTS-1:0] we_i,
  input  vreg_be_t   [NR_WRITE_PORTS-1:0] wbe_i,
  output logic       [NR_WRITE_PORTS-1:0] wvalid_o,
  // Read ports
  input  vreg_addr_t [NR_READ_PORTS-1:0] raddr_i,
  input  logic       [NR_READ_PORTS-1:0] re_i,
  output vreg_data_t [NR_READ_PORTS-1:0] rdata_o,
  output logic       [NR_READ_PORTS-1:0] rvalid_o
);

endmodule : spatz_vrf
