// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

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

  logic illegal_instr;
  logic reset_vstart = 1'b1;

  spatz_req_t spatz_req;

  /////////////
  // Decoder //
  /////////////

  always_comb begin : proc_decoder
    illegal_instr = 1'b0;
    spatz_req = '0;

    if (decoder_req_valid_i) begin
      automatic int unsigned opcode = decoder_req_i.instr[6:0];

      unique case (opcode)
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

          automatic logic is_load = (opcode == riscv_pkg::OpcodeLoadFP);

          spatz_req.op_mem.vm = ls_vm;
          spatz_req.ex_unit = LSU;

          $error("Unsupported Load/Store Instruction.");
        end // OpcodeLoadFP or OpcodeStoreFP

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
            spatz_req.rs1 = (setvl_rs1 == '0 && setvl_rd != '0) ? '1 : spatz_req.rs1;
            // Keep vl
            spatz_req.op_cgf.keep_vl = setvl_rs1 == '0 && setvl_rd == '0;
          // Arithmetic instruction (except for masked and widening)
          end else begin
            automatic int unsigned arith_s1 = decoder_req_i.instr[19:15];
            automatic int unsigned arith_s2 = decoder_req_i.instr[24:20];
            automatic int unsigned arith_d  = decoder_req_i.instr[11:7];
            automatic int unsigned arith_vm = decoder_req_i.instr[25];

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
                spatz_req.rs1 = elen_t'($signed(arith_s1));
              end
              OPIVX,
              OPMVX: begin
                spatz_req.rs1 = decoder_req_i.rs1;
                illegal_instr = ~decoder_req_i.rs1_valid;
              end
              OPFVV,
              OPFVF: illegal_instr = 1'b1;
            endcase

            // Check what arithmetic operation is requested
            unique casez (decoder_req_i.instr)
              // Vector Arithmetic
              riscv_instr::VADD_VV,
              riscv_instr::VADD_VX,
              riscv_instr::VADD_VI: begin
                spatz_req.op = VADD;
              end

              riscv_instr::VSUB_VV,
              riscv_instr::VSUB_VX: begin
                spatz_req.op = VSUB;
              end

              riscv_instr::VRSUB_VX,
              riscv_instr::VRSUB_VI: begin
                spatz_req.op = VRSUB;
              end

              // Vector Logic
              riscv_instr::VAND_VV,
              riscv_instr::VAND_VX,
              riscv_instr::VAND_VI: begin
                spatz_req.op = VAND;
              end

              riscv_instr::VOR_VV,
              riscv_instr::VOR_VX,
              riscv_instr::VOR_VI: begin
                spatz_req.op = VOR;
              end

              riscv_instr::VXOR_VV,
              riscv_instr::VXOR_VX,
              riscv_instr::VXOR_VI: begin
                spatz_req.op = VXOR;
              end

              // Vector Arithmetic with Carry
              riscv_instr::VADC_VVM,
              riscv_instr::VADC_VXM,
              riscv_instr::VADC_VIM: begin
                spatz_req.op = VADC;
              end

              riscv_instr::VMADC_VV,
              riscv_instr::VMADC_VX,
              riscv_instr::VMADC_VI: begin
                spatz_req.op = VMADC;
              end

              riscv_instr::VMADC_VVM,
              riscv_instr::VMADC_VXM,
              riscv_instr::VMADC_VIM: begin
                spatz_req.op = VMADC;
                spatz_req.op_arith.use_carry_borrow_in = 1'b1;
              end

              riscv_instr::VSBC_VVM,
              riscv_instr::VSBC_VXM: begin
                spatz_req.op = VSBC;
              end

              riscv_instr::VMSBC_VV,
              riscv_instr::VMSBC_VX: begin
                spatz_req.op = VMSBC;
              end

              riscv_instr::VMSBC_VVM,
              riscv_instr::VMSBC_VXM: begin
                spatz_req.op = VMSBC;
                spatz_req.op_arith.use_carry_borrow_in = 1'b1;
              end

              // Vector Shift
              riscv_instr::VSLL_VV,
              riscv_instr::VSLL_VX,
              riscv_instr::VSLL_VI: begin
                spatz_req.op = VSLL;
              end

              riscv_instr::VSRL_VV,
              riscv_instr::VSRL_VX,
              riscv_instr::VSRL_VI: begin
                spatz_req.op = VSRL;
              end

              riscv_instr::VSRA_VV,
              riscv_instr::VSRA_VX,
              riscv_instr::VSRA_VI: begin
                spatz_req.op = VSRA;
              end

              // Vector Min/Max
              riscv_instr::VMIN_VV,
              riscv_instr::VMIN_VX: begin
                spatz_req.op = VMIN;
              end

              riscv_instr::VMINU_VV,
              riscv_instr::VMINU_VX: begin
                spatz_req.op = VMINU;
              end

              riscv_instr::VMAX_VV,
              riscv_instr::VMAX_VX: begin
                spatz_req.op = VMAX;
              end

              riscv_instr::VMAXU_VV,
              riscv_instr::VMAXU_VX: begin
                spatz_req.op = VMAXU;
              end

              // Vector Comparison
              riscv_instr::VMSEQ_VV,
              riscv_instr::VMSEQ_VX,
              riscv_instr::VMSEQ_VI: begin
                spatz_req.op = VMSEQ;
              end

              riscv_instr::VMSNE_VV,
              riscv_instr::VMSNE_VX,
              riscv_instr::VMSNE_VI: begin
                spatz_req.op = VMSNE;
              end

              riscv_instr::VMSLTU_VV,
              riscv_instr::VMSLTU_VX: begin
                spatz_req.op = VMSLTU;
              end

              riscv_instr::VMSLT_VV,
              riscv_instr::VMSLT_VX: begin
                spatz_req.op = VMSLT;
              end

              riscv_instr::VMSLEU_VV,
              riscv_instr::VMSLEU_VX,
              riscv_instr::VMSLEU_VI: begin
                spatz_req.op = VMSLEU;
              end

              riscv_instr::VMSLE_VV,
              riscv_instr::VMSLE_VX,
              riscv_instr::VMSLE_VI: begin
                spatz_req.op = VMSLE;
              end

              riscv_instr::VMSGTU_VX,
              riscv_instr::VMSGTU_VI: begin
                spatz_req.op = VMSGTU;
              end

              riscv_instr::VMSGT_VX,
              riscv_instr::VMSGT_VI: begin
                spatz_req.op = VMSGT;
              end

              // Vector Multiply
              riscv_instr::VMUL_VV,
              riscv_instr::VMUL_VX: begin
                spatz_req.op = VMUL;
              end

              riscv_instr::VMULH_VV,
              riscv_instr::VMULH_VX: begin
                spatz_req.op = VMULH;
              end

              riscv_instr::VMULHU_VV,
              riscv_instr::VMULHU_VX: begin
                spatz_req.op = VMULHU;
              end

              riscv_instr::VMULHSU_VV,
              riscv_instr::VMULHSU_VX: begin
                spatz_req.op = VMULHSU;
              end

              // Vector Division
              riscv_instr::VDIVU_VV,
              riscv_instr::VDIVU_VX: begin
                spatz_req.op = VDIVU;
              end

              riscv_instr::VDIV_VV,
              riscv_instr::VDIV_VX: begin
                spatz_req.op = VDIV;
              end

              riscv_instr::VREMU_VV,
              riscv_instr::VREMU_VX: begin
                spatz_req.op = VREMU;
              end

              riscv_instr::VREM_VV,
              riscv_instr::VREM_VX: begin
                spatz_req.op = VREM;
              end

              // Vector Multiply-Add
              riscv_instr::VMACC_VV,
              riscv_instr::VMACC_VX: begin
                spatz_req.op = VMACC;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instr::VNMSAC_VV,
              riscv_instr::VNMSAC_VX: begin
                spatz_req.op = VNMSAC;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instr::VMADD_VV,
              riscv_instr::VMADD_VX: begin
                spatz_req.op = VMADD;
                spatz_req.vd_is_src = 1'b1;
              end

              riscv_instr::VNMSUB_VV,
              riscv_instr::VNMSUB_VX: begin
                spatz_req.op = VNMSUB;
                spatz_req.vd_is_src = 1'b1;
              end

              // Vector Merge
              riscv_instr::VMERGE_VVM,
              riscv_instr::VMERGE_VXM,
              riscv_instr::VMERGE_VIM: begin
                spatz_req.op = VMERGE;
              end

              riscv_instr::VMV_V_V,
              riscv_instr::VMV_V_X,
              riscv_instr::VMV_V_I: begin
                spatz_req.op = VMV;
              end

              default: begin
                illegal_instr = 1'b1;
                $error("Unsupported Vector Arithmetic Instruction.");
              end
            endcase // Arithmetic Instruction Type
          end
        end // OpcodeVec

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

          case (csr_addr)
            riscv_instr::CSR_VSTART,
            riscv_instr::CSR_VL,
            riscv_instr::CSR_VTYPE,
            riscv_instr::CSR_VLENB,
            riscv_instr::CSR_VXSAT,
            riscv_instr::CSR_VXRM,
            riscv_instr::CSR_VCSR: begin
              spatz_req.op_csr.addr = csr_addr;
            end
            default: illegal_instr = 1'b1;
          endcase

          unique casez (decoder_req_i.instr)
            riscv_instr::CSRRW,
            riscv_instr::CSRRWI: begin
              if (csr_addr == riscv_instr::CSR_VSTART) begin
                spatz_req.use_rd = csr_rd != '0;
                spatz_req.op_cgf.write_vstart = 1'b1;
              end
            end

            riscv_instr::CSRRS,
            riscv_instr::CSRRSI: begin
              if (csr_addr == riscv_instr::CSR_VSTART) begin
                spatz_req.op_cgf.set_vstart = csr_rs1 != '0;
              end
            end

            riscv_instr::CSRRC,
            riscv_instr::CSRRCI: begin
              if (csr_addr == riscv_instr::CSR_VSTART) begin
                spatz_req.op_cgf.clear_vstart = csr_rs1 != '0;
              end
            end
            default: begin
              illegal_instr = 1'b1;
              $error("Unsupported CSR Instruction.");
            end
          endcase // CSR
        end // OpcodeSystem

        default: begin
          illegal_instr = 1'b1;
          $error("Unsupported Instruction.");
        end
      endcase // Opcodes

      spatz_req.op_cgf.reset_vstart = illegal_instr ? 1'b0 : reset_vstart;
    end // Instruction valid
  end // proc_decoder

  // Check if rsp valid and assign spatz_req
  assign decoder_rsp_o.spatz_req = spatz_req;
  assign decoder_rsp_o.instr_illegal = decoder_req_valid_i & illegal_instr;
  assign decoder_rsp_valid_o = decoder_req_valid_i;

endmodule : spatz_decoder
