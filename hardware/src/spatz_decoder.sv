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
	logic reset_vstart = 1'b1;

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
	  		end // OpcodeLoadFP

	  		riscv_pkg::OpcodeStoreFP: begin
	  			$error("Unsupported Instruction (OpcodeStoreFP).");
	  		end // OpcodeStoreFP

	  		riscv_pkg::OpcodeVec: begin
	  			unique case (instr_i[14:12])
	  				OPCFG: begin
	  					automatic int unsigned setvl_rs1 = instr_i[19:15];
	  					automatic int unsigned setvl_rd	 = instr_i[11:7];

	  					decoded_data_o.rd = setvl_rd;
	  					decoded_data_o.use_rd = 1'b1;
	  					decoded_data_o.op = VCFG;

	  					// Extract vtype
	  					if (instr_i[31] == 1'b0) begin
	  						decoded_data_o.vtype = {1'b0, instr_i[27:20]};
	  						decoded_data_o.rs1 	 = rs1_i;
	  					end else if (instr_i[31:30] == 2'b11) begin
								decoded_data_o.vtype = {1'b0, instr_i[27:20]};
								decoded_data_o.rs1 	 = elen_t'(setvl_rs1);
	  					end else if (instr_i[31:25] == 7'b1000000) begin
	  						decoded_data_o.vtype = {1'b0, rs2_i[7:0]};
	  						decoded_data_o.rs1 	 = rs1_i;
	  					end

	  					// Set to maxvl or new desired value
	  					decoded_data_o.rs1 = (setvl_rs1 == '0 && setvl_rd != '0) ? '1 : decoded_data_o.rs1;
	  					// Keep vl
	  					decoded_data_o.op_cgf.keep_vl = setvl_rs1 == '0 && setvl_rd == '0;
	  				end // OPCFG
	  				default: $error("Unsupported OpcodeVec function.");
	  			endcase
	  		end // OpcodeVec

	  		riscv_pkg::OpcodeSystem: begin
	  			automatic int unsigned csr_addr 	= instr_i[31:20];
	  			automatic int unsigned csr_rd 		= instr_i[11:7];
	  			automatic int unsigned csr_rs1 		= instr_i[19:15];
	  			automatic int unsigned csr_is_imm = instr_i[14];

	  			decoded_data_o.op = VCSR;
	  			decoded_data_o.rd = csr_rd;
	  			decoded_data_o.use_rd = 1'b1;
	  			decoded_data_o.rs1 = csr_is_imm ? 32'(csr_rs1) : rs1_i;
	  			reset_vstart = 1'b0;

	  			unique casez (instr_i)
	  				riscv_instr::CSRRW,
	  				riscv_instr::CSRRWI: begin
	  					case (csr_addr)
	  						CSR_VSTART: begin
	  							decoded_data_o.use_rd = csr_rd != '0;
	  							decoded_data_o.op_csr.addr = CSR_VSTART;
	  							decoded_data_o.op_cgf.write_vstart = 1'b1;
	  						end
	  						CSR_VL: decoded_data_o.op_csr.addr = CSR_VL;
	  						CSR_VTYPE: decoded_data_o.op_csr.addr = CSR_VTYPE;
	  						CSR_VLENB: decoded_data_o.op_csr.addr = CSR_VLENB;
	  						CSR_VXSAT: decoded_data_o.op_csr.addr = CSR_VXSAT;
	  						CSR_VXRM: decoded_data_o.op_csr.addr = CSR_VXRM;
	  						CSR_VCSR: decoded_data_o.op_csr.addr = CSR_VCSR;
	  						default: illegal_instr = 1'b1;
	  					endcase
	  				end

	  				riscv_instr::CSRRS,
	  				riscv_instr::CSRRSI: begin
	  					case (csr_addr)
	  						CSR_VSTART: begin
	  							decoded_data_o.op_csr.addr = CSR_VSTART;
	  							decoded_data_o.op_cgf.set_vstart = csr_rs1 != '0;
	  						end
	  						CSR_VL: decoded_data_o.op_csr.addr = CSR_VL;
	  						CSR_VTYPE: decoded_data_o.op_csr.addr = CSR_VTYPE;
	  						CSR_VLENB: decoded_data_o.op_csr.addr = CSR_VLENB;
	  						CSR_VXSAT: decoded_data_o.op_csr.addr = CSR_VXSAT;
	  						CSR_VXRM: decoded_data_o.op_csr.addr = CSR_VXRM;
	  						CSR_VCSR: decoded_data_o.op_csr.addr = CSR_VCSR;
	  						default: illegal_instr = 1'b1;
	  					endcase
	  				end

	  				riscv_instr::CSRRC,
	  				riscv_instr::CSRRCI: begin
	  					case (csr_addr)
	  						CSR_VSTART: begin
	  							decoded_data_o.op_csr.addr = CSR_VSTART;
	  							decoded_data_o.op_cgf.clear_vstart = csr_rs1 != '0;
	  						end
	  						CSR_VL: decoded_data_o.op_csr.addr = CSR_VL;
	  						CSR_VTYPE: decoded_data_o.op_csr.addr = CSR_VTYPE;
	  						CSR_VLENB: decoded_data_o.op_csr.addr = CSR_VLENB;
	  						CSR_VXSAT: decoded_data_o.op_csr.addr = CSR_VXSAT;
	  						CSR_VXRM: decoded_data_o.op_csr.addr = CSR_VXRM;
	  						CSR_VCSR: decoded_data_o.op_csr.addr = CSR_VCSR;
	  						default: illegal_instr = 1'b1;
	  					endcase
	  				end
	  				default: begin
	  					illegal_instr = 1'b1;
	  					$error("Unsupported Instruction (OpcodeSystem).");
	  				end
	  			endcase // CSR
	  		end // OpcodeSystem

	  		default: begin
	  			illegal_instr = 1'b1;
	  			$error("Unsupported Instruction.");
	  		end
	  	endcase // Opcodes

	  	decoded_data_o.op_cgf.reset_vstart = illegal_instr ? 1'b0 : reset_vstart;
	  end // Instruction valid
  end // proc_decoder

  assign instr_illegal_o = illegal_instr;

endmodule : spatz_decoder