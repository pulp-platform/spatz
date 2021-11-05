// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz 
	import spatz_pkg::*; 
	import rvv_pkg::*; 
(
	input 	logic	clk_i,
	input 	logic rst_ni,

	input riscv_pkg::instr_t instr_i,
	input logic 	instr_valid_i,
	output logic 	instr_illegal_o,
	input elen_t rs1_i,
	input elen_t rs2_i,

	output elen_t rd_o
);

	// Include FF
	`include "common_cells/registers.svh"

	/////////////
	// Signals //
	/////////////

	riscv_pkg::instr_t instr;
	logic 	instr_valid;
	logic 	instr_illegal;
	elen_t rs1;
	elen_t rs2;
	elen_t rd;

	spatz_req_t decoded_data;

	assign instr = instr_i;
	assign instr_valid = instr_valid_i;
	assign instr_illegal_o = instr_illegal;
	assign rs1 = rs1_i;
	assign rs2 = rs2_i;
	assign rd_o = rd;

	//////////
	// CSRs //
	//////////

	vlen_t 	vstart;
	vlen_t 	vl;
	vtype_t vtype;

	spatz_vcsr i_vcsr (
		.clk_i     (clk_i),
		.rst_ni    (rst_ni),
		.vcsr_req_i(decoded_data),
		.vtype_o   (vtype),
		.vl_o      (vl),
		.vstart_o  (vstart)
	);

  ////////////////
  // Controller //
  ////////////////

  always_comb begin : proc_controller
  	rd = '0;
  	case (decoded_data.op)
  		VCSR: begin
  			if (decoded_data.use_rd) begin
	  			unique case (decoded_data.op_csr.addr)
	  				CSR_VSTART: rd = 32'(vstart);
	  				CSR_VL: 		rd = 32'(vl);
	  				CSR_VTYPE: 	rd = 32'(vtype);
	  				CSR_VLENB: 	rd = 32'(VLENB);
	  			endcase
	  		end
  		end
  	endcase // Operation type
  end

  /////////////
  // Decoder //
  /////////////

	spatz_decoder i_decoder (
		.clk_i (clk_i),
		.rst_ni(rst_ni),

		.instr_i        (instr),
		.instr_valid_i  (instr_valid),
		.instr_illegal_o(instr_illegal),

		.rs1_i          (rs1),
		.rs2_i          (rs2),

		.decoded_data_o (decoded_data)
	);

endmodule : spatz
