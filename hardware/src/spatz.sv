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

	// CSR registers
	vlen_t  vstart_d, vstart_q;
  vlen_t  vl_d, vl_q;
  vtype_t vtype_d, vtype_q;

  `FF(vstart_q, vstart_d, '0)
  `FF(vl_q, vl_d, '0)
  `FF(vtype_q, vtype_d, '{vill: 1'b1, default: '0})

  always_comb begin : proc_vcsr
  	vstart_d = vstart_q;
  	vl_d 		 = vl_q;
  	vtype_d  = vtype_q;

  	if (decoded_data.op == VCFG) begin
  		// Check if vtype is valid
			if ((vtype_d.vsew > EW_32) || (vtype_d.vlmul == LMUL_RES) || (vtype_d.vlmul + $clog2(ELEN) - 1 < vtype_d.vsew)) begin
				// Invalid
				vtype_d = '{vill: 1'b1, default: '0};
				vl_d		= '0;
			end else begin
				// Valid

				// Set new vtype
				vtype_d = decoded_data.vtype;
				if (~decoded_data.op_cgf.keep_vl) begin
					// Normal stripmining mode
					automatic int unsigned vlmax = 0;
					vlmax = VLENB >> decoded_data.vtype.vsew;

					unique case (decoded_data.vtype.vlmul)
						LMUL_F2: vlmax >>= 1;
						LMUL_F4: vlmax >>= 2;
						LMUL_F8: vlmax >>= 3;
						LMUL_1: vlmax <<= 0;
						LMUL_2: vlmax <<= 1;
						LMUL_4: vlmax <<= 2;
						LMUL_8: vlmax <<= 3;
					endcase

					vl_d = (decoded_data.rs1 == '1) ? MAXVL : (vlmax < decoded_data.rs1) ? vlmax : decoded_data.rs1;
				end
			end
  	end
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
