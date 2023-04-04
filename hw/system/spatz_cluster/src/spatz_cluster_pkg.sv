// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich

// AXI structs
`include "axi/typedef.svh"

package spatz_cluster_pkg;

  ///////////
  //  AXI  //
  ///////////

  // AXI Data Width
  localparam int unsigned AxiDataWidth = `ifdef SPATZ_AXI_DW `SPATZ_AXI_DW `else 0 `endif;
  localparam int unsigned AxiStrbWidth = AxiDataWidth / 8;
  // AXI Address Width
  localparam int unsigned AxiAddrWidth = `ifdef SPATZ_AXI_AW `SPATZ_AXI_AW `else 0 `endif;
  // AXI ID Width
  localparam int unsigned AxiIdWidth   = `ifdef SPATZ_AXI_IW `SPATZ_AXI_IW `else 0 `endif;
  // AXI User Width
  localparam int unsigned AxiUserWidth = `ifdef SPATZ_AXI_UW `SPATZ_AXI_UW `else 0 `endif;

  typedef logic [AxiDataWidth-1:0] axi_data_t;
  typedef logic [AxiStrbWidth-1:0] axi_strb_t;
  typedef logic [AxiAddrWidth-1:0] axi_addr_t;
  typedef logic [AxiIdWidth-1:0] axi_id_t;
  typedef logic [AxiUserWidth-1:0] axi_user_t;
  `AXI_TYPEDEF_ALL(spatz_axi, axi_addr_t, axi_id_t, axi_data_t, axi_strb_t, axi_user_t)

  ////////////////////
  //  Spatz Cluster //
  ////////////////////

  localparam int unsigned NumCores = `ifdef NUM_CORES `NUM_CORES `else 0 `endif;
  localparam int unsigned NumBanks = `ifdef NUM_BANKS `NUM_BANKS `else 0 `endif;

  localparam int unsigned TCDMDepth = `ifdef TCDM_BANK_DEPTH `TCDM_BANK_DEPTH `else 0 `endif;

  localparam int unsigned DataWidth      = 64;
  localparam integer unsigned BeWidth    = DataWidth / 8;
  localparam integer unsigned ByteOffset = $clog2(BeWidth);

endpackage: spatz_cluster_pkg
