// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

package riscv_pkg;

  ///////////////
  //  Opcodes  //
  ///////////////

  localparam logic [6:0] OpcodeVec      = 7'b101_0111;
  localparam logic [6:0] OpcodeStoreFP  = 7'b010_0111;
  localparam logic [6:0] OpcodeLoadFP   = 7'b000_0111;
  localparam logic [6:0] OpcodeSystem   = 7'b111_0011;

  //////////////
  // Typedefs //
  //////////////

  typedef logic [31:0] instr_t;

 endpackage : riscv_pkg
