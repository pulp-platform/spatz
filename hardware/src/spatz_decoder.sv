// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_decoder import spatz_pkg::*; import rvv_pkg::*; (
	input  logic clk_i,
	input  logic rst_ni,
	// Instruction
	input  riscv_pkg::instr_t instr_i,
	input  logic 	 instr_valid_i,
	output logic 	 instr_illegal_o,
	// X-registers input values
	input  elen_t rs1_i,
	input  elen_t rs2_i,
	// Result
	output spatz_req_t decoded_data_o,
	output logic 			 valid_o
);

	// Include FF
	`include "common_cells/registers.svh"

	/////////////
	// Signals //
	/////////////

	logic illegal_instr;

  /////////////
  // Decoder //
  /////////////

  always_comb begin : proc_decoder
  	illegal_instr = 1'b0;

  	decoded_data_o = '0;

  	if (instr_valid_i) begin
	  	unique case (instr_i[6:0])
	  		riscv_pkg::OpcodeLoadFP: begin
	  			$error("Unsupported Instruction (OpcodeLoadFP).");
	  		end

	  		riscv_pkg::OpcodeStoreFP: begin
	  			$error("Unsupported Instruction (OpcodeStoreFP).");
	  		end

	  		riscv_pkg::OpcodeVec: begin
	  			unique case (instr_i[14:12])
	  				OPCFG: begin
	  					decoded_data_o.rd = instr_i[11:7];
	  					decoded_data_o.use_rd = 1'b1;
	  					decoded_data_o.op = VCFG;

	  					// Extract vtype
	  					if (instr_i[31] == 1'b0) begin
	  						decoded_data_o.vtype = {1'b0, instr_i[27:20]};
	  						decoded_data_o.rs1 	 = rs1_i;
	  					end else if (instr_i == 2'b11) begin
								decoded_data_o.vtype = {1'b0, instr_i[27:20]};
								decoded_data_o.rs1 	 = {{27{1'b0}},instr_i[19:15]};
	  					end else if (instr_i == 7'b1000000) begin
	  						decoded_data_o.vtype = {1'b0, rs2_i[7:0]};
	  						decoded_data_o.rs1 	 = rs1_i;
	  					end

	  					// Set to maxvl or new desired value
	  					decoded_data_o.rs1 = (instr_i[19:15] == '0 && instr_i[11:7] != '0) ? '1 : decoded_data_o.rs1;
	  					// Keep vl
	  					decoded_data_o.op_cgf.keep_vl = instr_i[19:15] == '0 && instr_i[11:7] == '0;
	  				end // OPCFG
	  				default: $error("Unsupported func3.");
	  			endcase
	  		end

	  		riscv_pkg::OpcodeSystem: begin
	  			$error("Unsupported Instruction (OpcodeSystem).");
	  		end

	  		default: begin
	  			illegal_instr = 1'b1;
	  			$error("Unsupported Instruction.");
	  		end
	  	endcase // instr_i[6:0]
	  end
  end

  assign instr_illegal_o = illegal_instr;

endmodule : spatz_decoder