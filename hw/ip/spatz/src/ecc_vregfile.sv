// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// Generic vector register file that makes use of latches to store data.

module vregfile import spatz_pkg::*; #(
    parameter int  unsigned NrReadPorts = 0,
    parameter int  unsigned NrWords     = NRVREG,
    parameter int  unsigned WordWidth   = VRFWordWidth,
    // Derived parameters.  Do not change!
    parameter type          addr_t      = logic[$clog2(NrWords)-1:0],
    parameter type          data_t      = logic [WordWidth-1:0],
    parameter type          strb_t      = logic [WordWidth/8-1:0]
    // Reliability parameters
    parameter  int unsigned UnprotectedWidth = 32,
    parameter  int unsigned ProtectedWidth   = 39,
    parameter  bit          InputECC         = 0, // 0: no ECC on input
                                                // 1: SECDED on input
    localparam int unsigned DataInWidth      = InputECC ? ProtectedWidth : UnprotectedWidth
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

  // Include FF
  `include "common_cells/registers.svh"

  /////////////
  // Signals //
  /////////////

  // Gated clock
  logic clk;

  // Register file memory
  logic [NrWords-1:0][WordWidth/8-1:0][7:0] mem;

  // Write data sampling
  data_t wdata_q, wdata_d;

  ///////////////////
  // Register File //
  ///////////////////
  typedef enum logic { NORMAL, READ_MODIFY_WRITE } store_state_e;
  store_state_e store_state_d, store_state_q;

    always_comb begin
    store_state_d  = NORMAL;
    gnt_o          = 1'b1;
    bank_addr      = addr_i;
    bank_we        = internal_we;
    input_buffer_d = wdata_i;
    addr_buffer_d  = addr_i;
    be_buffer_d    = be_i;
    bank_req       = req_i;
    rmw_count_d    = rmw_count_q;
    if (store_state_q == NORMAL) begin
      if (req_i & (be_i != {ByteEnWidth{1'b1}}) & internal_we) begin
        store_state_d = READ_MODIFY_WRITE;
        bank_we       = 1'b0;
        rmw_count_d   = rmw_count_t'(NumRMWCuts);
      end
    end else begin
      gnt_o           = 1'b0;
      bank_addr       = addr_buffer_q;
      bank_we         = 1'b1;
      input_buffer_d  = input_buffer_q;
      addr_buffer_d   = addr_buffer_q;
      be_buffer_d     = be_buffer_q;
      if (rmw_count_q == '0) begin
        bank_req      = 1'b1;
      end else begin
        bank_req      = 1'b0;
        rmw_count_d   = rmw_count_q - 1;
        store_state_d = READ_MODIFY_WRITE;
      end
    end
  end


// VRF ECC encoding and decoding-----------------------
  hsiao_ecc_dec #(
      .DataWidth (UnprotectedWidth),
      .ProtWidth (ProtectedWidth - UnprotectedWidth)
    ) ecc_decode (
      .in        ( decoder_in ),
      .out       ( loaded ),
      .syndrome_o(),
      .err_o     ( ecc_error )
    );

    hsiao_ecc_enc #(
      .DataWidth (UnprotectedWidth),
      .ProtWidth (ProtectedWidth - UnprotectedWidth)
    ) ecc_encode (
      .in  ( to_store   ),
      .out ( wdata_d )
    );
//---------------------------------------------------------

  // assign wdata_d = wdata_i;


  // Row decoder. Create a clock for each SCM row
  // logic [NrWords-1:0] row_clk;
  // for (genvar row = 0; row < NrWords; row++) begin: gen_row_decoder
  //   // Create latch clock signal
  //   logic row_onehot;
  //   assign row_onehot = (waddr_i == row);

  //   // Create a clock for each SCM row
  //   tc_clk_gating i_waddr_cg (
  //     .clk_i    (clk         ),
  //     .en_i     (row_onehot  ),
  //     .test_en_i(testmode_i  ),
  //     .clk_o    (row_clk[row])
  //   );
  // end: gen_row_decoder

  // // Column decoder. Create a clock for each SCM column
  // logic [WordWidth/8-1:0] col_clk;
  // for (genvar b = 0; b < WordWidth/8; b++) begin: gen_col_decoder
  //   tc_clk_gating i_wbe_cg (
  //     .clk_i    (clk       ),
  //     .en_i     (wbe_i[b]  ),
  //     .test_en_i(testmode_i),
  //     .clk_o    (col_clk[b])
  //   );
  // end: gen_col_decoder

  // Select which destination bytes to write into

  // Store new data to memory
  /* verilator lint_off NOLATCH */
  for (genvar vreg = 0; vreg < NrWords; vreg++) begin: gen_write_mem
    for (genvar b = 0; b < WordWidth/8; b++) begin: gen_word
      // logic clk_latch;
      // tc_clk_and2 i_clk_and (
      //   .clk0_i(row_clk[vreg]),
      //   .clk1_i(col_clk[b]   ),
      //   .clk_o (clk_latch    )
      // );

      // always_latch begin
      //   if (clk_latch)
      //     mem[vreg][b] <= wdata_q[b*8 +: 8];
      // end

      logic wr_en;
      assign wr_en = we_i & (waddr_i == vreg) & wbe_i[b];

      always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni)
          mem[vreg][b] <= '0;
        else if(wr_en)
          mem[vreg][b] <= wdata_d[b*8 +: 8];
      end // CMY: FF-based VRF
    end: gen_word
  end: gen_write_mem
  /* verilator lint_on NOLATCH */

  // Read data from memory
  for (genvar port = 0; port < NrReadPorts; port++) begin: gen_read_mem
    // Reuse the decision tree from the RR arbiter
    rr_arb_tree #(
      .AxiVldRdy(1'b1     ),
      .ExtPrio  (1'b1     ),
      .DataWidth(WordWidth),
      .NumIn    (NrWords  )
    ) i_read_tree (
      .clk_i  (clk_i        ),
      .rst_ni (rst_ni       ),
      .flush_i(1'b0         ),
      .idx_o  (/* Unused */ ),
      .data_i (mem          ),
      .rr_i   (raddr_i[port]),
      .data_o (rdata_o[port]),
      .req_i  ('1           ),
      .gnt_o  (/* Unused */ ),
      .req_o  (/* Unused */ ),
      .gnt_i  (1'b1         )
    );
  end: gen_read_mem

endmodule : vregfile
