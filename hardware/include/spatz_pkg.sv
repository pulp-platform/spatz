// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

package spatz_pkg;

	//////////////////
  //  Parameters  //
  //////////////////

  // Maximum size of a single vector element in bits
  localparam int unsigned ELEN = 32;
  // Maximum siye of a single vector element in bytes
  localparam int unsigned ELENB = ELEN / 8;
  // Number of bits in a vector register
  localparam int unsigned VLEN = `ifdef VLEN `VLEN `else 0 `endif;
  // Number of bztes in a vector register
  localparam int unsigned VLENB = VLEN / 8;

 endpackage : spatz_pkg