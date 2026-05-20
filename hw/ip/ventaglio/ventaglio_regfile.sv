// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Bowen Wang, ETH Zurich
//
// Generic register file that makes use of flip-flops to store data.

module ventaglio_regfile import spatz_pkg::*; #(
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

  /////////////
  // Signals //
  /////////////

  // Gated clock
  logic clk_cg;

  // Register file memory
  logic [NrWords-1:0][WordWidth/8-1:0][7:0] mem_d, mem_q;

  ///////////////////
  // Register File //
  ///////////////////

  // Clock gate
  tc_clk_gating i_cg (
    .clk_i    (clk_i          ),
    .en_i     (we_i | ~rst_ni ),
    .test_en_i(testmode_i     ),
    .clk_o    (clk_cg         )
  );

  // Mem
  for (genvar row = 0; row < NrWords; row++) begin: gen_mem_row
    for (genvar b = 0; b < WordWidth/8; b++) begin: gen_word

      logic row_sel;
      assign row_sel = (waddr_i == addr_t'(row));

      always_comb begin
      	mem_d[row][b] = mem_q[row][b];
      	if (we_i && row_sel && wbe_i[b]) begin
      		mem_d[row][b] = wdata_i[b*8 +: 8];
  		end
      end

      always_ff @ (posedge clk_cg or negedge rst_ni) begin
        if (!rst_ni)
          mem_q[row][b] <= '0;
        else
          mem_q[row][b] <= mem_d[row][b];
      end
    end: gen_word
  end: gen_mem_row

  // Combinational read ports
  // BOWWANG: might not need arbiter here if we only need one read port
  if (NrReadPorts > 0) begin : gen_reads
    for (genvar p = 0; p < NrReadPorts; p++) begin : gen_rp
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
	      .data_i (mem_q        ),
	      .rr_i   (raddr_i[p]),
	      .data_o (rdata_o[p]),
	      .req_i  ('1           ),
	      .gnt_o  (/* Unused */ ),
	      .req_o  (/* Unused */ ),
	      .gnt_i  (1'b1         )
	    );
    end
  end

endmodule : ventaglio_regfile