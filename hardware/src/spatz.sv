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

  	// Reset vstart to zero
  	if (decoded_data.op_cgf.reset_vstart) begin
  		vstart_d = '0;
  	end

  	// Set new vstart when written over vcsrs
  	if (decoded_data.op == VCSR) begin
  		if (decoded_data.op_cgf.write_vstart) begin
  			vstart_d = vlen_t'(decoded_data.rs1);
  		end else if (decoded_data.op_cgf.set_vstart) begin
  			vstart_d = vstart_q | vlen_t'(decoded_data.rs1);
  		end else if (decoded_data.op_cgf.clear_vstart) begin
  			vstart_d = vstart_q & ~vlen_t'(decoded_data.rs1);
  		end
  	end

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
					// Normal stripmining mode or set to MAXVL
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
				end else begin
					// Keep vl mode

					// If new vtype changes ration, mark as illegal
					if (($signed(decoded_data.vtype.vsew) - $signed(vtype_q.vsew)) != ($signed(decoded_data.vtype.vlmul) - $signed(vtype_q.vlmul))) begin
						vtype_d = '{vill: 1'b1, default: '0};
						vl_d		= '0;
					end
				end
			end
  	end
  end

  ////////////////
  // Controller //
  ////////////////

  always_comb begin : proc_controller
  	rd = '0;
  	case (decoded_data.op)
  		VCSR: begin
  			if (decoded_data.use_rd) begin
	  			unique case (decoded_data.op_csr.addr)
	  				CSR_VSTART: rd = 32'(vstart_q);
	  				CSR_VL: 		rd = 32'(vl_q);
	  				CSR_VTYPE: 	rd = 32'(vtype_q);
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
