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
	  			automatic int unsigned func3 = instr_i[14:12];

	  			// Config instruction
	  			if (func3 == OPCFG) begin
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
  				// Arithmetic instruction
  				end else begin
  					automatic int unsigned arith_s1 = instr_i[19:15];
  					automatic int unsigned arith_s2 = instr_i[24:20];
  					automatic int unsigned arith_d  = instr_i[11:7];
  					automatic int unsigned arith_vm = instr_i[25];

  					decoded_data_o.op_arith.vm = arith_vm;
  					decoded_data_o.use_vs2 = 1'b1;
  					decoded_data_o.vs2 = arith_s2;
  					decoded_data_o.use_vd = 1'b1;
  					decoded_data_o.vd = arith_d;
  					// Decide which operands to use (vs1 or rs1 or imm)
  					unique case (func3)
  						OPIVV,
  						OPMVV: begin
  							decoded_data_o.use_vs1 = 1'b1;
  							decoded_data_o.vs1 = arith_s1;
  						end
  						OPIVI: begin
  							decoded_data_o.rs1 = elen_t'($signed(arith_s1));
  						end
  						OPIVX,
  						OPMVX: begin
  							decoded_data_o.rs1 = rs1_i;
  						end
  						OPFVV,
  						OPFVF: illegal_instr = 1'b1;
  					endcase

  					// Check what arithmetic operation is requested
  					unique casez (instr_i)
  						// Vector Arithmetic
  						riscv_instr::VADD_VV,
  						riscv_instr::VADD_VX,
  						riscv_instr::VADD_VI: begin
  							decoded_data_o.op = VADD;
  						end

  						riscv_instr::VSUB_VV,
  						riscv_instr::VSUB_VX: begin
  							decoded_data_o.op = VSUB;
  						end

  						riscv_instr::VRSUB_VX,
  						riscv_instr::VRSUB_VI: begin
  							decoded_data_o.op = VRSUB;
  						end

  						// Vector Logic
  						riscv_instr::VAND_VV,
  						riscv_instr::VAND_VX,
  						riscv_instr::VAND_VI: begin
  							decoded_data_o.op = VAND;
  						end

  						riscv_instr::VOR_VV,
  						riscv_instr::VOR_VX,
  						riscv_instr::VOR_VI: begin
  							decoded_data_o.op = VOR;
  						end

  						riscv_instr::VXOR_VV,
  						riscv_instr::VXOR_VX,
  						riscv_instr::VXOR_VI: begin
  							decoded_data_o.op = VXOR;
  						end

  						// Vector Arithmetic with Carry
  						riscv_instr::VADC_VVM,
  						riscv_instr::VADC_VXM,
  						riscv_instr::VADC_VIM: begin
  							decoded_data_o.op = VADC;
  						end

  						riscv_instr::VMADC_VV,
  						riscv_instr::VMADC_VX,
  						riscv_instr::VMADC_VI: begin
  							decoded_data_o.op = VMADC;
  						end

  						riscv_instr::VMADC_VVM,
  						riscv_instr::VMADC_VXM,
  						riscv_instr::VMADC_VIM: begin
  							decoded_data_o.op = VMADC;
  							decoded_data_o.op_arith.use_carry_borrow_in = 1'b1;
  						end

  						riscv_instr::VSBC_VVM,
  						riscv_instr::VSBC_VXM: begin
  							decoded_data_o.op = VSBC;
  						end

  						riscv_instr::VMSBC_VV,
  						riscv_instr::VMSBC_VX: begin
  							decoded_data_o.op = VMSBC;
  						end

  						riscv_instr::VMSBC_VVM,
  						riscv_instr::VMSBC_VXM: begin
  							decoded_data_o.op = VMSBC;
  							decoded_data_o.op_arith.use_carry_borrow_in = 1'b1;
  						end

  						// Vector Shift
  						riscv_instr::VSLL_VV,
  						riscv_instr::VSLL_VX,
  						riscv_instr::VSLL_VI: begin
  							decoded_data_o.op = VSLL;
  						end

  						riscv_instr::VSRL_VV,
  						riscv_instr::VSRL_VX,
  						riscv_instr::VSRL_VI: begin
  							decoded_data_o.op = VSRL;
  						end

  						riscv_instr::VSRA_VV,
  						riscv_instr::VSRA_VX,
  						riscv_instr::VSRA_VI: begin
  							decoded_data_o.op = VSRA;
  						end

  						// Vector Min/Max
  						riscv_instr::VMIN_VV,
  						riscv_instr::VMIN_VX: begin
  							decoded_data_o.op = VMIN;
  						end

  						riscv_instr::VMINU_VV,
  						riscv_instr::VMINU_VX: begin
  							decoded_data_o.op = VMINU;
  						end

  						riscv_instr::VMAX_VV,
  						riscv_instr::VMAX_VX: begin
  							decoded_data_o.op = VMAX;
  						end

  						riscv_instr::VMAXU_VV,
  						riscv_instr::VMAXU_VX: begin
  							decoded_data_o.op = VMAXU;
  						end

  						// Vector Comparison
  						riscv_instr::VMSEQ_VV,
  						riscv_instr::VMSEQ_VX,
  						riscv_instr::VMSEQ_VI: begin
  							decoded_data_o.op = VMSEQ;
  						end

  						riscv_instr::VMSNE_VV,
  						riscv_instr::VMSNE_VX,
  						riscv_instr::VMSNE_VI: begin
  							decoded_data_o.op = VMSNE;
  						end

  						riscv_instr::VMSLTU_VV,
  						riscv_instr::VMSLTU_VX: begin
  							decoded_data_o.op = VMSLTU;
  						end

  						riscv_instr::VMSLT_VV,
  						riscv_instr::VMSLT_VX: begin
  							decoded_data_o.op = VMSLT;
  						end

  						riscv_instr::VMSLEU_VV,
  						riscv_instr::VMSLEU_VX,
  						riscv_instr::VMSLEU_VI: begin
  							decoded_data_o.op = VMSLEU;
  						end

  						riscv_instr::VMSLE_VV,
  						riscv_instr::VMSLE_VX,
  						riscv_instr::VMSLE_VI: begin
  							decoded_data_o.op = VMSLE;
  						end

  						riscv_instr::VMSGTU_VX,
  						riscv_instr::VMSGTU_VI: begin
  							decoded_data_o.op = VMSGTU;
  						end

  						riscv_instr::VMSGT_VX,
  						riscv_instr::VMSGT_VI: begin
  							decoded_data_o.op = VMSGT;
  						end

  						// Vector Multiply
  						riscv_instr::VMUL_VV,
  						riscv_instr::VMUL_VX: begin
  							decoded_data_o.op = VMUL;
  						end

  						riscv_instr::VMULH_VV,
  						riscv_instr::VMULH_VX: begin
  							decoded_data_o.op = VMULH;
  						end

  						riscv_instr::VMULHU_VV,
  						riscv_instr::VMULHU_VX: begin
  							decoded_data_o.op = VMULHU;
  						end

  						riscv_instr::VMULHSU_VV,
  						riscv_instr::VMULHSU_VX: begin
  							decoded_data_o.op = VMULHSU;
  						end

  						// Vector Division
  						riscv_instr::VDIVU_VV,
  						riscv_instr::VDIVU_VX: begin
  							decoded_data_o.op = VDIVU;
  						end

  						riscv_instr::VDIV_VV,
  						riscv_instr::VDIV_VX: begin
  							decoded_data_o.op = VDIV;
  						end

  						riscv_instr::VREMU_VV,
  						riscv_instr::VREMU_VX: begin
  							decoded_data_o.op = VREMU;
  						end

  						riscv_instr::VREM_VV,
  						riscv_instr::VREM_VX: begin
  							decoded_data_o.op = VREM;
  						end

  						// Vector Multiply-Add
  						riscv_instr::VMACC_VV,
  						riscv_instr::VMACC_VX: begin
  							decoded_data_o.op = VMACC;
  						end

  						riscv_instr::VNMSAC_VV,
  						riscv_instr::VNMSAC_VX: begin
  							decoded_data_o.op = VNMSAC;
  						end

  						riscv_instr::VMADD_VV,
  						riscv_instr::VMADD_VX: begin
  							decoded_data_o.op = VMADD;
  						end

  						riscv_instr::VNMSUB_VV,
  						riscv_instr::VNMSUB_VX: begin
  							decoded_data_o.op = VNMSUB;
  						end

  						// Vector Merge
  						riscv_instr::VMERGE_VVM,
  						riscv_instr::VMERGE_VXM,
  						riscv_instr::VMERGE_VIM: begin
  							decoded_data_o.op = VMERGE;
  						end

  						riscv_instr::VMV_V_V,
  						riscv_instr::VMV_V_X,
  						riscv_instr::VMV_V_I: begin
  							decoded_data_o.op = VMV;
  						end

  						default: begin
  							illegal_instr = 1'b1;
  							$error("Unsupported Vector Arithmetic Instruction.");
  						end
  					endcase // Arithmetic Instruction Type
  				end
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

	  			case (csr_addr)
						riscv_instr::CSR_VSTART,
						riscv_instr::CSR_VL,
						riscv_instr::CSR_VTYPE,
						riscv_instr::CSR_VLENB,
						riscv_instr::CSR_VXSAT,
						riscv_instr::CSR_VXRM,
						riscv_instr::CSR_VCSR: begin
							decoded_data_o.op_csr.addr = csr_addr;
						end
						default: illegal_instr = 1'b1;
					endcase

	  			unique casez (instr_i)
	  				riscv_instr::CSRRW,
	  				riscv_instr::CSRRWI: begin
	  					if (csr_addr ==	riscv_instr::CSR_VSTART) begin
  							decoded_data_o.use_rd = csr_rd != '0;
  							decoded_data_o.op_cgf.write_vstart = 1'b1;
  						end
	  				end

	  				riscv_instr::CSRRS,
	  				riscv_instr::CSRRSI: begin
	  					if (csr_addr ==	riscv_instr::CSR_VSTART) begin
	  						decoded_data_o.op_cgf.set_vstart = csr_rs1 != '0;
  						end
	  				end

	  				riscv_instr::CSRRC,
	  				riscv_instr::CSRRCI: begin
	  					if (csr_addr ==	riscv_instr::CSR_VSTART) begin
	  						decoded_data_o.op_cgf.clear_vstart = csr_rs1 != '0;
  						end
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

  // Check if instruction is legal and if calculated output is valid
  assign instr_illegal_o = instr_valid_i & illegal_instr;
  assign valid_o = instr_valid_i & ~illegal_instr;

endmodule : spatz_decoder