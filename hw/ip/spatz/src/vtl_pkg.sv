// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich

package vtl_pkg;

  ///////////////////
  //  Definitions  //
  ///////////////////

  // index width in bit
  typedef enum logic [2:0] {
    IDXW_1  = 3'b000,
    IDXW_2 = 3'b001,
    IDXW_4 = 3'b010,
    IDXW_8 = 3'b011
  } sp_idxw_e;

  typedef enum logic [2:0] {
    SP_BLK_2   = 3'b000,
    SP_BLK_4   = 3'b001,
    SP_BLK_8   = 3'b010,
    SP_BLK_16  = 3'b011,
    SP_BLK_32  = 3'b100,
    SP_BLK_64  = 3'b101,
    SP_BLK_128 = 3'b110,
    SP_BLK_256 = 3'b111
  } sp_blk_e;

  typedef enum logic [2:0] {
  	SP_RATIO_075   = 3'b000,    // e.g., 3:4
    SP_RATIO_050   = 3'b001,    // e.g., 2:4
    SP_RATIO_025   = 3'b010,    // e.g., 1:4
    SP_RATIO_125   = 3'b011     // e.g., 1:8
  } sp_ratio_e;


endpackage : vtl_pkg