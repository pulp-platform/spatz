// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The deocder takes in a new instruction that is offloaded to Spatz
// and analyzes and decodes it.

module spatz_decoder import spatz_pkg::*; import rvv_pkg::*; (
  input  logic clk_i,
  input  logic rst_ni,
  // Request
  input decoder_req_t decoder_req_i,
  input logic         decoder_req_valid_i,
  // Response
  output decoder_rsp_t decoder_rsp_o,
  output logic         decoder_rsp_valid_o
);

  /////////////
  // Signals //
  /////////////

  // Is the instruction illegal
  logic illegal_instr;
  // Do we want to reset the current vstart CSR value
  logic reset_vstart;

  // New spatz request from decoded instruction
  spatz_req_t spatz_req;

  /////////////
  // Decoder //
  /////////////

  always_comb begin : decoder
    illegal_instr = 1'b0;
    spatz_req = '0;
    reset_vstart = 1'b1;

    // We have a new instruction that need to be decoded
    if (decoder_req_valid_i) begin
      // Retrieve the opcode
      automatic int unsigned opcode = decoder_req_i.instr[6:0];

      unique case (opcode)
        // Load and store instruction
        riscv_pkg::OpcodeLoadFP,
        riscv_pkg::OpcodeStoreFP: begin
          automatic int unsigned ls_vd    = decoder_req_i.instr[11:7];
          automatic int unsigned ls_rs1   = decoder_req_i.instr[19:15];
          automatic int unsigned ls_s2    = decoder_req_i.instr[24:20];
          automatic int unsigned ls_width = decoder_req_i.instr[14:12];
          automatic int unsigned ls_vm    = decoder_req_i.instr[25];
          automatic int unsigned ls_mop   = decoder_req_i.instr[27:26];
          automatic int unsigned ls_mew   = decoder_req_i.instr[28];
          automatic int unsigned ls_nf    = decoder_req_i.instr[31:29];

          // Retrieve VSEW
          unique case ({ls_mew, ls_width})
            4'b0000: spatz_req.vtype.vsew = EW_8;
            4'b0101: spatz_req.vtype.vsew = EW_16;
            4'b0110: spatz_req.vtype.vsew = EW_32;
            default: illegal_instr = 1'b1;
          endcase

          spatz_req.op_mem.vm = ls_vm;
          spatz_req.ex_unit = LSU;

          // Check which type of load or store operation is requested
          unique casez (decoder_req_i.instr)
            riscv_instruction::VLE8_V,
            riscv_instruction::VLE16_V,
            riscv_instruction::VLE32_V: begin
              spatz_req.op     = VLE;
              spatz_req.vd     = ls_vd;
              spatz_req.use_vd = 1'b1;
              spatz_req.rs1    = decoder_req_i.rs1;
              illegal_instr    = ~decoder_req_i.rs1_valid;
            end

            riscv_instruction::VLSE8_V,
            riscv_instruction::VLSE16_V,
            riscv_instruction::VLSE32_V: begin
              spatz_req.op     = VLSE;
              spatz_req.vd     = ls_vd;
              spatz_req.use_vd = 1'b1;
              spatz_req.rs1    = decoder_req_i.rs1;
              spatz_req.rs2    = decoder_req_i.rs2;
              illegal_instr    = ~decoder_req_i.rs1_valid | ~decoder_req_i.rs2_valid;
            end

            riscv_instruction::VSE8_V,
            riscv_instruction::VSE16_V,
            riscv_instruction::VSE32_V: begin
              spatz_req.op        = VSE;
              spatz_req.vd        = ls_vd;
              spatz_req.use_vd    = 1'b1;
              spatz_req.vd_is_src = 1'b1;
              spatz_req.rs1       = decoder_req_i.rs1;
              illegal_instr       = ~decoder_req_i.rs1_valid;
            end

            riscv_instruction::VSSE8_V,
            riscv_instruction::VSSE16_V,
            riscv_instruction::VSSE32_V: begin
              spatz_req.op        = VSSE;
              spatz_req.vd        = ls_vd;
              spatz_req.use_vd    = 1'b1;
              spatz_req.vd_is_src = 1'b1;
              spatz_req.rs1       = decoder_req_i.rs1;
              spatz_req.rs2       = decoder_req_i.rs2;
              illegal_instr       = ~decoder_req_i.rs1_valid | ~decoder_req_i.rs2_valid;
            end
            default: begin
              illegal_instr = 1'b1;
            end
          endcase // decoder_req_i.instr
        end // OpcodeLoadFP or OpcodeStoreFP

        // Vector instruction
        riscv_pkg::OpcodeVec: begin
          automatic int unsigned func3 = decoder_req_i.instr[14:12];

          // Config instruction
          if (func3 == OPCFG) begin
            automatic int unsigned setvl_rs1 = decoder_req_i.instr[19:15];
            automatic int unsigned setvl_rd  = decoder_req_i.instr[11:7];

            spatz_req.rd = setvl_rd;
            spatz_req.use_rd = 1'b1;
            spatz_req.op = VCFG;
            spatz_req.ex_unit = CON;

            // Extract vtype
            if (decoder_req_i.instr[31] == 1'b0) begin
              spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};
              spatz_req.rs1   = decoder_req_i.rs1;
              illegal_instr   = ~decoder_req_i.rs1_valid;
            end else if (decoder_req_i.instr[31:30] == 2'b11) begin
              spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};
              spatz_req.rs1   = elen_t'(setvl_rs1);
            end else if (decoder_req_i.instr[31:25] == 7'b1000000) begin
              spatz_req.vtype = {1'b0, decoder_req_i.rs2[7:0]};
              spatz_req.rs1   = decoder_req_i.rs1;
              illegal_instr   = ~decoder_req_i.rs1_valid || ~decoder_req_i.rs2_valid;
            end

            // Set to maxvl or new desired value
            spatz_req.rs1 = (setvl_rs1 == 0 && setvl_rd != 0) ? '1 : spatz_req.rs1;
            // Keep vl
            spatz_req.op_cgf.keep_vl = setvl_rs1 == '0 && setvl_rd == '0;
          // Arithmetic instruction (except for masked and widening)
          end else begin
            automatic logic [4:0] arith_s1 = decoder_req_i.instr[19:15];
            automatic logic [4:0] arith_s2 = decoder_req_i.instr[24:20];
            automatic logic [4:0] arith_d  = decoder_req_i.instr[11:7];
            automatic bit         arith_vm = decoder_req_i.instr[25];

            spatz_req.op_arith.vm = arith_vm;
            spatz_req.use_vs2 = 1'b1;
            spatz_req.vs2 = arith_s2;
            spatz_req.use_vd = 1'b1;
            spatz_req.vd = arith_d;
            spatz_req.ex_unit = VFU;

            // Decide which operands to use (vs1 or rs1 or imm)
            unique case (func3)
              OPIVV,
              OPMVV: begin
                spatz_req.use_vs1 = 1'b1;
                spatz_req.vs1 = arith_s1;
              end
              OPIVI: begin
                spatz_req.rs1 = elen_t'(signed'(arith_s1));
              end
              OPIVX,
              OPMVX: begin
                spatz_req.rs1 = decoder_req_i.rs1;
                illegal_instr = ~decoder_req_i.rs1_valid;
              end
              default: illegal_instr = 1'b1;
            endcase

            // Check what arithmetic operation is requested
            unique casez (decoder_req_i.instr)
              // Vector Arithmetic
              riscv_instruction::VADD_VV,
              riscv_instruction::VADD_VX,
              riscv_instruction::VADD_VI: begin
                spatz_req.op = VADD;
              end

              riscv_instruction::VSUB_VV,
              riscv_instruction::VSUB_VX: begin
                spatz_req.op = VSUB;
              end

              riscv_instruction::VRSUB_VX,
              riscv_instruction::VRSUB_VI: begin
                spatz_req.op = VRSUB;
              end

              // Vector Logic
              riscv_instruction::VAND_VV,
              riscv_instruction::VAND_VX,
              riscv_instruction::VAND_VI: begin
                spatz_req.op = VAND;
              end

              riscv_instruction::VOR_VV,
              riscv_instruction::VOR_VX,
              riscv_instruction::VOR_VI: begin
                spatz_req.op = VOR;
              end

              riscv_instruction::VXOR_VV,
              riscv_instruction::VXOR_VX,
              riscv_instruction::VXOR_VI: begin
                spatz_req.op = VXOR;
              end

              // Vector Arithmetic with Carry
              riscv_instruction::VADC_VVM,
              riscv_instruction::VADC_VXM,
              riscv_instruction::VADC_VIM: begin
                spatz_req.op = VADC;
              end

              riscv_instruction::VMADC_VV,
              riscv_instruction::VMADC_VX,
              riscv_instruction::VMADC_VI: begin
                spatz_req.op = VMADC;
              end

              riscv_instruction::VMADC_VVM,
              riscv_instruction::VMADC_VXM,
              riscv_instruction::VMADC_VIM: begin
                spatz_req.op = VMADC;
                spatz_req.op_arith.use_carry_borrow_in = 1'b1;
              end

              riscv_instruction::VSBC_VVM,
              riscv_instruction::VSBC_VXM: begin
                spatz_req.op = VSBC;
              end

              riscv_instruction::VMSBC_VV,
              riscv_instruction::VMSBC_VX: begin
                spatz_req.op = VMSBC;
              end

              riscv_instruction::VMSBC_VVM,
              riscv_instruction::VMSBC_VXM: begin
                spatz_req.op = VMSBC;
                spatz_req.op_arith.use_carry_borrow_in = 1'b1;
              end

              // Vector Shift
              riscv_instruction::VSLL_VV,
              riscv_instruction::VSLL_VX,
              riscv_instruction::VSLL_VI: begin
                spatz_req.op = VSLL;
              end

              riscv_instruction::VSRL_VV,
              riscv_instruction::VSRL_VX,
              riscv_instruction::VSRL_VI: begin
                spatz_req.op = VSRL;
              end

              riscv_instruction::VSRA_VV,
              riscv_instruction::VSRA_VX,
              riscv_instruction::VSRA_VI: begin
                spatz_req.op = VSRA;
              end

              // Vector Min/Max
              riscv_instruction::VMIN_VV,
              riscv_instruction::VMIN_VX: begin
                spatz_req.op = VMIN;
              end

              riscv_instruction::VMINU_VV,
              riscv_instruction::VMINU_VX: begin
                spatz_req.op = VMINU;
              end

              riscv_instruction::VMAX_VV,
              riscv_instruction::VMAX_VX: begin
                spatz_req.op = VMAX;
              end

              riscv_instruction::VMAXU_VV,
              riscv_instruction::VMAXU_VX: begin
                spatz_req.op = VMAXU;
              end

              // Vector Comparison
              riscv_instruction::VMSEQ_VV,
              riscv_instruction::VMSEQ_VX,
              riscv_instruction::VMSEQ_VI: begin
                spatz_req.op = VMSEQ;
              end

              riscv_instruction::VMSNE_VV,
              riscv_instruction::VMSNE_VX,
              riscv_instruction::VMSNE_VI: begin
                spatz_req.op = VMSNE;
              end

              riscv_instruction::VMSLTU_VV,
              riscv_instruction::VMSLTU_VX: begin
                spatz_req.op = VMSLTU;
              end

              riscv_instruction::VMSLT_VV,
              riscv_instruction::VMSLT_VX: begin
                spatz_req.op = VMSLT;
              end

              riscv_instruction::VMSLEU_VV,
              riscv_instruction::VMSLEU_VX,
              riscv_instruction::VMSLEU_VI: begin
                spatz_req.op = VMSLEU;
              end

              riscv_instruction::VMSLE_VV,
              riscv_instruction::VMSLE_VX,
              riscv_instruction::VMSLE_VI: begin
                spatz_req.op = VMSLE;
              end

              riscv_instruction::VMSGTU_VX,
              riscv_instruction::VMSGTU_VI: begin
                spatz_req.op = VMSGTU;
              end

              riscv_instruction::VMSGT_VX,
              riscv_instruction::VMSGT_VI: begin
                spatz_req.op = VMSGT;
              end

              // Vector Multiply
              riscv_instruction::VMUL_VV,
              riscv_instruction::VMUL_VX: begin
                spatz_req.op = VMUL;
              end

              riscv_instruction::VMULH_VV,
              riscv_instruction::VMULH_VX: begin
                spatz_req.op = VMULH;
              end

              riscv_instruction::VMULHU_VV,
              riscv_instruction::VMULHU_VX: begin
                spatz_req.op = VMULHU;
              end

              riscv_instruction::VMULHSU_VV,
              riscv_instruction::VMULHSU_VX: begin
                spatz_req.op = VMULHSU;
              end

              // Vector Division
              riscv_instruction::VDIVU_VV,
              riscv_instruction::VDIVU_VX: begin
                spatz_req.op = VDIVU;
              end

              riscv_instruction::VDIV_VV,
              riscv_instruction::VDIV_VX: begin
                spatz_req.op = VDIV;
              end

              riscv_instruction::VREMU_VV,
              riscv_instruction::VREMU_VX: begin
                spatz_req.op = VREMU;
              end

              riscv_instruction::VREM_VV,
              riscv_instruction::VREM_VX: begin
                spatz_req.op = VREM;
              end

              // Vector Multiply-Add
              riscv_instruction::VMACC_VV,
              riscv_instruction::VMACC_VX: begin
                spatz_req.op = VMACC;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instruction::VNMSAC_VV,
              riscv_instruction::VNMSAC_VX: begin
                spatz_req.op = VNMSAC;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instruction::VMADD_VV,
              riscv_instruction::VMADD_VX: begin
                spatz_req.op = VMADD;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instruction::VNMSUB_VV,
              riscv_instruction::VNMSUB_VX: begin
                spatz_req.op = VNMSUB;
                spatz_req.vd_is_src = 1'b1;
              end

              // Vector Merge
              riscv_instruction::VMERGE_VVM,
              riscv_instruction::VMERGE_VXM,
              riscv_instruction::VMERGE_VIM: begin
                spatz_req.op = VMERGE;
              end

              riscv_instruction::VMV_V_V,
              riscv_instruction::VMV_V_X,
              riscv_instruction::VMV_V_I: begin
                spatz_req.op = VMV;
              end

              default: begin
                illegal_instr = 1'b1;
              end
            endcase // Arithmetic Instruction Type
          end
        end // OpcodeVec

        // CSR instruction
        riscv_pkg::OpcodeSystem: begin
          automatic int unsigned csr_addr   = decoder_req_i.instr[31:20];
          automatic int unsigned csr_rd     = decoder_req_i.instr[11:7];
          automatic int unsigned csr_rs1    = decoder_req_i.instr[19:15];
          automatic int unsigned csr_is_imm = decoder_req_i.instr[14];

          spatz_req.op = VCSR;
          spatz_req.ex_unit = CON;
          spatz_req.rd = csr_rd;
          spatz_req.use_rd = 1'b1;
          spatz_req.rs1 = csr_is_imm ? 32'(csr_rs1) : decoder_req_i.rs1;
          illegal_instr = csr_is_imm ? 1'b0 : ~decoder_req_i.rs1_valid;
          reset_vstart = 1'b0;

          // Check if CSR access is really destined for Spatz
          case (csr_addr)
            riscv_instruction::CSR_VSTART,
            riscv_instruction::CSR_VL,
            riscv_instruction::CSR_VTYPE,
            riscv_instruction::CSR_VLENB,
            riscv_instruction::CSR_VXSAT,
            riscv_instruction::CSR_VXRM,
            riscv_instruction::CSR_VCSR: begin
              spatz_req.op_csr.addr = csr_addr;
            end
            default: illegal_instr = 1'b1;
          endcase

          // Check type of CSR access (read/write)
          unique casez (decoder_req_i.instr)
            riscv_instruction::CSRRW,
            riscv_instruction::CSRRWI: begin
              if (csr_addr == riscv_instruction::CSR_VSTART) begin
                spatz_req.use_rd = csr_rd != '0;
                spatz_req.op_cgf.write_vstart = 1'b1;
              end
            end

            riscv_instruction::CSRRS,
            riscv_instruction::CSRRSI: begin
              if (csr_addr == riscv_instruction::CSR_VSTART) begin
                spatz_req.op_cgf.set_vstart = csr_rs1 != '0;
              end
            end

            riscv_instruction::CSRRC,
            riscv_instruction::CSRRCI: begin
              if (csr_addr == riscv_instruction::CSR_VSTART) begin
                spatz_req.op_cgf.clear_vstart = csr_rs1 != '0;
              end
            end
            default: begin
              illegal_instr = 1'b1;
            end
          endcase // CSR
        end // OpcodeSystem

        default: begin
          illegal_instr = 1'b1;
        end
      endcase // Opcodes

      // Add correct reset_vstart value
      spatz_req.op_cgf.reset_vstart = illegal_instr ? 1'b0 : reset_vstart;
    end // Instruction valid
  end : decoder

  // Check if rsp valid and assign spatz_req
  assign decoder_rsp_o.spatz_req = spatz_req;
  assign decoder_rsp_o.instr_illegal = decoder_req_valid_i & illegal_instr;
  assign decoder_rsp_valid_o = decoder_req_valid_i;

endmodule : spatz_decoder
