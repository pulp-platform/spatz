// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich
//
// The deocder takes in a new instruction that is offloaded to Spatz
// and analyzes and decodes it.

module spatz_decoder
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::roundmode_e;
  (
    input  logic         clk_i,
    input  logic         rst_ni,
    // Request
    input  decoder_req_t decoder_req_i,
    input  logic         decoder_req_valid_i,
    // Response
    output decoder_rsp_t decoder_rsp_o,
    output logic         decoder_rsp_valid_o,
    // FPU untimed sidechannel
    input  roundmode_e   fpu_rnd_mode_i
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
    spatz_req     = '0;
    reset_vstart  = 1'b1;

    // We have a new instruction that need to be decoded
    if (decoder_req_valid_i) begin
      // Retrieve the opcode
      automatic logic [6:0] opcode = decoder_req_i.instr[6:0];

      unique casez (decoder_req_i.instr)
        // Load and store instructions
        riscv_instr::VLE8_V,
        riscv_instr::VLE16_V,
        riscv_instr::VLE32_V,
        riscv_instr::VLSE8_V,
        riscv_instr::VLSE16_V,
        riscv_instr::VLSE32_V,
        riscv_instr::VSE8_V,
        riscv_instr::VSE16_V,
        riscv_instr::VSE32_V,
        riscv_instr::VSSE8_V,
        riscv_instr::VSSE16_V,
        riscv_instr::VSSE32_V: begin
          automatic vreg_t ls_vd         = decoder_req_i.instr[11:7];
          automatic vreg_t ls_rs1        = decoder_req_i.instr[19:15];
          automatic vreg_t ls_s2         = decoder_req_i.instr[24:20];
          automatic logic [2:0] ls_width = decoder_req_i.instr[14:12];
          automatic logic ls_vm          = decoder_req_i.instr[25];
          automatic logic [1:0] ls_mop   = decoder_req_i.instr[27:26];
          automatic logic ls_mew         = decoder_req_i.instr[28];
          automatic logic [2:0] ls_nf    = decoder_req_i.instr[31:29];

          // Retrieve VSEW
          unique case ({ls_mew, ls_width})
            4'b0000: spatz_req.vtype.vsew = EW_8;
            4'b0101: spatz_req.vtype.vsew = EW_16;
            4'b0110: spatz_req.vtype.vsew = EW_32;
            default: illegal_instr        = 1'b1;
          endcase

          spatz_req.op_mem.vm = ls_vm;
          spatz_req.ex_unit   = LSU;

          // Check which type of load or store operation is requested
          unique casez (decoder_req_i.instr)
            riscv_instr::VLE8_V,
            riscv_instr::VLE16_V,
            riscv_instr::VLE32_V: begin
              spatz_req.op             = VLE;
              spatz_req.op_mem.is_load = 1'b1;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              illegal_instr            = ~decoder_req_i.rs1_valid;
            end

            riscv_instr::VLSE8_V,
            riscv_instr::VLSE16_V,
            riscv_instr::VLSE32_V: begin
              spatz_req.op             = VLSE;
              spatz_req.op_mem.is_load = 1'b1;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.rs2            = decoder_req_i.rs2;
              illegal_instr            = ~decoder_req_i.rs1_valid | ~decoder_req_i.rs2_valid;
            end

            riscv_instr::VSE8_V,
            riscv_instr::VSE16_V,
            riscv_instr::VSE32_V: begin
              spatz_req.op             = VSE;
              spatz_req.op_mem.is_load = 1'b0;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.vd_is_src      = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              illegal_instr            = ~decoder_req_i.rs1_valid;
            end

            riscv_instr::VSSE8_V,
            riscv_instr::VSSE16_V,
            riscv_instr::VSSE32_V: begin
              spatz_req.op             = VSSE;
              spatz_req.op_mem.is_load = 1'b0;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.vd_is_src      = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.rs2            = decoder_req_i.rs2;
              illegal_instr            = ~decoder_req_i.rs1_valid | ~decoder_req_i.rs2_valid;
            end

            default:
              illegal_instr = 1'b1;
          endcase // decoder_req_i.instr
        end

        // Vector instruction
        riscv_instr::VADD_VV,
        riscv_instr::VADD_VX,
        riscv_instr::VADD_VI,
        riscv_instr::VSUB_VV,
        riscv_instr::VSUB_VX,
        riscv_instr::VRSUB_VX,
        riscv_instr::VRSUB_VI,
        riscv_instr::VAND_VV,
        riscv_instr::VAND_VX,
        riscv_instr::VAND_VI,
        riscv_instr::VOR_VV,
        riscv_instr::VOR_VX,
        riscv_instr::VOR_VI,
        riscv_instr::VXOR_VV,
        riscv_instr::VXOR_VX,
        riscv_instr::VXOR_VI,
        riscv_instr::VADC_VVM,
        riscv_instr::VADC_VXM,
        riscv_instr::VADC_VIM,
        riscv_instr::VMADC_VV,
        riscv_instr::VMADC_VX,
        riscv_instr::VMADC_VI,
        riscv_instr::VMADC_VVM,
        riscv_instr::VMADC_VXM,
        riscv_instr::VMADC_VIM,
        riscv_instr::VSBC_VVM,
        riscv_instr::VSBC_VXM,
        riscv_instr::VMSBC_VV,
        riscv_instr::VMSBC_VX,
        riscv_instr::VMSBC_VVM,
        riscv_instr::VMSBC_VXM,
        riscv_instr::VSLL_VV,
        riscv_instr::VSLL_VX,
        riscv_instr::VSLL_VI,
        riscv_instr::VSRL_VV,
        riscv_instr::VSRL_VX,
        riscv_instr::VSRL_VI,
        riscv_instr::VSRA_VV,
        riscv_instr::VSRA_VX,
        riscv_instr::VSRA_VI,
        riscv_instr::VMIN_VV,
        riscv_instr::VMIN_VX,
        riscv_instr::VMINU_VV,
        riscv_instr::VMINU_VX,
        riscv_instr::VMAX_VV,
        riscv_instr::VMAX_VX,
        riscv_instr::VMAXU_VV,
        riscv_instr::VMAXU_VX,
        riscv_instr::VMSEQ_VV,
        riscv_instr::VMSEQ_VX,
        riscv_instr::VMSEQ_VI,
        riscv_instr::VMSNE_VV,
        riscv_instr::VMSNE_VX,
        riscv_instr::VMSNE_VI,
        riscv_instr::VMSLTU_VV,
        riscv_instr::VMSLTU_VX,
        riscv_instr::VMSLT_VV,
        riscv_instr::VMSLT_VX,
        riscv_instr::VMSLEU_VV,
        riscv_instr::VMSLEU_VX,
        riscv_instr::VMSLEU_VI,
        riscv_instr::VMSLE_VV,
        riscv_instr::VMSLE_VX,
        riscv_instr::VMSLE_VI,
        riscv_instr::VMSGTU_VX,
        riscv_instr::VMSGTU_VI,
        riscv_instr::VMSGT_VX,
        riscv_instr::VMSGT_VI,
        riscv_instr::VMUL_VV,
        riscv_instr::VMUL_VX,
        riscv_instr::VMULH_VV,
        riscv_instr::VMULH_VX,
        riscv_instr::VMULHU_VV,
        riscv_instr::VMULHU_VX,
        riscv_instr::VMULHSU_VV,
        riscv_instr::VMULHSU_VX,
        riscv_instr::VDIVU_VV,
        riscv_instr::VDIVU_VX,
        riscv_instr::VDIV_VV,
        riscv_instr::VDIV_VX,
        riscv_instr::VREMU_VV,
        riscv_instr::VREMU_VX,
        riscv_instr::VREM_VV,
        riscv_instr::VREM_VX,
        riscv_instr::VMACC_VV,
        riscv_instr::VMACC_VX,
        riscv_instr::VNMSAC_VV,
        riscv_instr::VNMSAC_VX,
        riscv_instr::VMADD_VV,
        riscv_instr::VMADD_VX,
        riscv_instr::VNMSUB_VV,
        riscv_instr::VNMSUB_VX,
        riscv_instr::VMERGE_VVM,
        riscv_instr::VMERGE_VXM,
        riscv_instr::VMERGE_VIM,
        riscv_instr::VMV_V_V,
        riscv_instr::VMV_V_X,
        riscv_instr::VMV_V_I,
        riscv_instr::VSLIDEUP_VX,
        riscv_instr::VSLIDEUP_VI,
        riscv_instr::VSLIDE1UP_VX,
        riscv_instr::VSLIDEDOWN_VX,
        riscv_instr::VSLIDEDOWN_VI,
        riscv_instr::VSLIDE1DOWN_VX: begin
          automatic opcodev_func3_e func3 = opcodev_func3_e'(decoder_req_i.instr[14:12]);
          automatic vreg_t arith_s1       = decoder_req_i.instr[19:15];
          automatic vreg_t arith_s2       = decoder_req_i.instr[24:20];
          automatic vreg_t arith_d        = decoder_req_i.instr[11:7];
          automatic logic arith_vm        = decoder_req_i.instr[25];

          spatz_req.op_arith.vm = arith_vm;
          spatz_req.op_sld.vm   = arith_vm;
          spatz_req.use_vs2     = 1'b1;
          spatz_req.vs2         = arith_s2;
          spatz_req.use_vd      = 1'b1;
          spatz_req.vd          = arith_d;
          spatz_req.ex_unit     = VFU;

          // Decide which operands to use (vs1 or rs1 or imm)
          unique case (func3)
            OPIVV,
            OPMVV: begin
              spatz_req.use_vs1 = 1'b1;
              spatz_req.vs1     = arith_s1;
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
              spatz_req.op                           = VMADC;
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
              spatz_req.op                           = VMSBC;
              spatz_req.op_arith.use_carry_borrow_in = 1'b1;
            end

            // Vector Shift
            riscv_instr::VSLL_VV,
            riscv_instr::VSLL_VX,
            riscv_instr::VSLL_VI: begin
              spatz_req.op = VSLL;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            riscv_instr::VSRL_VV,
            riscv_instr::VSRL_VX,
            riscv_instr::VSRL_VI: begin
              spatz_req.op = VSRL;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            riscv_instr::VSRA_VV,
            riscv_instr::VSRA_VX,
            riscv_instr::VSRA_VI: begin
              spatz_req.op = VSRA;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
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
              spatz_req.op        = VMACC;
              spatz_req.vd_is_src = 1'b1;
            end

            riscv_instr::VNMSAC_VV,
            riscv_instr::VNMSAC_VX: begin
              spatz_req.op        = VNMSAC;
              spatz_req.vd_is_src = 1'b1;
            end

            riscv_instr::VMADD_VV,
            riscv_instr::VMADD_VX: begin
              spatz_req.op        = VMADD;
              spatz_req.vd_is_src = 1'b1;
            end

            riscv_instr::VNMSUB_VV,
            riscv_instr::VNMSUB_VX: begin
              spatz_req.op        = VNMSUB;
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
              // vmv is the same as a zero slide
              spatz_req.op            = VSLIDEUP;
              spatz_req.ex_unit       = SLD;
              spatz_req.op_sld.insert = (func3 == OPIVI || func3 == OPIVX);
              spatz_req.op_sld.vmv    = 1'b1;
              spatz_req.vs2           = spatz_req.vs1;
              spatz_req.use_vs2       = func3 != OPIVI;
            end

            // Vector Slide
            riscv_instr::VSLIDEUP_VX,
            riscv_instr::VSLIDEUP_VI: begin
              spatz_req.op      = VSLIDEUP;
              spatz_req.ex_unit = SLD;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            riscv_instr::VSLIDE1UP_VX: begin
              spatz_req.op            = VSLIDEUP;
              spatz_req.op_sld.insert = 1'b1;
              spatz_req.ex_unit       = SLD;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            riscv_instr::VSLIDEDOWN_VX,
            riscv_instr::VSLIDEDOWN_VI: begin
              spatz_req.op      = VSLIDEDOWN;
              spatz_req.ex_unit = SLD;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            riscv_instr::VSLIDE1DOWN_VX: begin
              spatz_req.op            = VSLIDEDOWN;
              spatz_req.op_sld.insert = 1'b1;
              spatz_req.ex_unit       = SLD;
              if (func3 == OPIVI) begin
                spatz_req.rs1 = elen_t'(arith_s1);
              end
            end

            default: illegal_instr = 1'b1;
          endcase // Arithmetic Instruction Type
        end

        // Vector floating-point instructions
        riscv_instr::VFADD_VV,
        riscv_instr::VFADD_VF,
        riscv_instr::VFRSUB_VF,
        riscv_instr::VFMIN_VV,
        riscv_instr::VFMIN_VF,
        riscv_instr::VFMAX_VV,
        riscv_instr::VFMAX_VF,
        riscv_instr::VFSGNJ_VV,
        riscv_instr::VFSGNJ_VF,
        riscv_instr::VFSGNJN_VV,
        riscv_instr::VFSGNJN_VF,
        riscv_instr::VFSGNJX_VV,
        riscv_instr::VFSGNJX_VF,
        riscv_instr::VMFEQ_VV,
        riscv_instr::VMFEQ_VF,
        riscv_instr::VMFLE_VV,
        riscv_instr::VMFLE_VF,
        riscv_instr::VMFLT_VV,
        riscv_instr::VMFLT_VF,
        riscv_instr::VMFNE_VV,
        riscv_instr::VMFNE_VF,
        riscv_instr::VMFGT_VF,
        riscv_instr::VMFGE_VF,
        riscv_instr::VFMUL_VV,
        riscv_instr::VFMUL_VF,
        riscv_instr::VFMADD_VV,
        riscv_instr::VFMADD_VF,
        riscv_instr::VFNMADD_VV,
        riscv_instr::VFNMADD_VF,
        riscv_instr::VFMSUB_VV,
        riscv_instr::VFMSUB_VF,
        riscv_instr::VFNMSUB_VV,
        riscv_instr::VFNMSUB_VF,
        riscv_instr::VFMACC_VV,
        riscv_instr::VFMACC_VF,
        riscv_instr::VFNMACC_VV,
        riscv_instr::VFNMACC_VF,
        riscv_instr::VFMSAC_VV,
        riscv_instr::VFMSAC_VF,
        riscv_instr::VFNMSAC_VV,
        riscv_instr::VFNMSAC_VF,
        riscv_instr::VFCVT_F_X_V,
        riscv_instr::VFCVT_F_XU_V,
        riscv_instr::VFCVT_X_F_V,
        riscv_instr::VFCVT_XU_F_V,
        riscv_instr::VFCVT_RTZ_X_F_V,
        riscv_instr::VFCVT_RTZ_XU_F_V,
        riscv_instr::VFCLASS_V,
        riscv_instr::VFMV_V_F: begin
          if (spatz_pkg::FPU_EN) begin
            automatic opcodev_func3_e func3 = opcodev_func3_e'(decoder_req_i.instr[14:12]);
            automatic vreg_t arith_s1       = decoder_req_i.instr[19:15];
            automatic vreg_t arith_s2       = decoder_req_i.instr[24:20];
            automatic vreg_t arith_d        = decoder_req_i.instr[11:7];
            automatic logic arith_vm        = decoder_req_i.instr[25];

            spatz_req.op_arith.vm = arith_vm;
            spatz_req.op_sld.vm   = arith_vm;
            spatz_req.use_vs2     = 1'b1;
            spatz_req.vs2         = arith_s2;
            spatz_req.use_vd      = 1'b1;
            spatz_req.vd          = arith_d;
            spatz_req.ex_unit     = VFU;
            spatz_req.rm          = fpu_rnd_mode_i;

            // Decide which operands to use (vs1 or rs1 or imm)
            unique case (func3)
              OPFVV: begin
                spatz_req.use_vs1 = 1'b1;
                spatz_req.vs1     = arith_s1;
              end
              OPFVF: begin
                spatz_req.rs1 = decoder_req_i.rs1;
                illegal_instr = ~decoder_req_i.rs1_valid;
              end
              default: illegal_instr = 1'b1;
            endcase

            unique casez (decoder_req_i.instr)
              riscv_instr::VFADD_VV,
              riscv_instr::VFADD_VF: spatz_req.op = VFADD;
              riscv_instr::VFSUB_VV,
              riscv_instr::VFSUB_VF: spatz_req.op = VFSUB;
              riscv_instr::VFMIN_VV,
              riscv_instr::VFMIN_VF: begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::VFMAX_VV,
              riscv_instr::VFMAX_VF: begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
            endcase
          end
        end

        // Scalar multiplication
        riscv_instr::MUL,
        riscv_instr::MULH,
        riscv_instr::MULHU,
        riscv_instr::MULHSU: begin
          spatz_req.ex_unit            = VFU;
          spatz_req.rd                 = decoder_req_i.instr[11:7];
          spatz_req.use_rd             = 1'b1;
          // Switch rs2 and rs1
          spatz_req.rs1                = decoder_req_i.rs2;
          spatz_req.rs2                = decoder_req_i.rs1;
          spatz_req.op_arith.is_scalar = 1'b1;

          unique casez (decoder_req_i.instr)
            riscv_instr::MUL   : spatz_req.op = VMUL;
            riscv_instr::MULH  : spatz_req.op = VMULH;
            riscv_instr::MULHU : spatz_req.op = VMULHU;
            riscv_instr::MULHSU: begin
              spatz_req.op = VMULHSU;
            end
          endcase
        end

        // Scalar division
        riscv_instr::DIV,
        riscv_instr::DIVU,
        riscv_instr::REM,
        riscv_instr::REMU: begin
          spatz_req.ex_unit            = VFU;
          spatz_req.rd                 = decoder_req_i.instr[11:7];
          spatz_req.use_rd             = 1'b1;
          // Switch rs2 and rs1
          spatz_req.rs1                = decoder_req_i.rs2;
          spatz_req.rs2                = decoder_req_i.rs1;
          spatz_req.op_arith.is_scalar = 1'b1;

          unique casez (decoder_req_i.instr)
            riscv_instr::DIV : spatz_req.op = VDIV;
            riscv_instr::DIVU: spatz_req.op = VDIVU;
            riscv_instr::REM : spatz_req.op = VREM;
            riscv_instr::REMU: spatz_req.op = VREMU;
          endcase
        end

        // Scalar floating point instructions
        riscv_instr::FADD_S,
        riscv_instr::FSUB_S,
        riscv_instr::FMUL_S,
        riscv_instr::FSGNJ_S,
        riscv_instr::FSGNJN_S,
        riscv_instr::FSGNJX_S,
        riscv_instr::FMIN_S,
        riscv_instr::FMAX_S,
        riscv_instr::FCLASS_S,
        riscv_instr::FLE_S,
        riscv_instr::FLT_S,
        riscv_instr::FEQ_S,
        riscv_instr::FCVT_S_W,
        riscv_instr::FCVT_S_WU,
        riscv_instr::FCVT_W_S,
        riscv_instr::FCVT_WU_S,
        riscv_instr::FMADD_S,
        riscv_instr::FMSUB_S,
        riscv_instr::FNMSUB_S,
        riscv_instr::FNMADD_S: begin
          if (spatz_pkg::FPU_EN) begin
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = decoder_req_i.instr[11:7];
            spatz_req.use_rd             = 1'b1;
            spatz_req.rs1                = decoder_req_i.rs1;
            spatz_req.rs2                = decoder_req_i.rs2;
            spatz_req.rsd                = decoder_req_i.rsd;
            spatz_req.op_arith.is_scalar = 1'b1;
            spatz_req.rm                 = fpu_rnd_mode_i;

            unique casez (decoder_req_i.instr)
              riscv_instr::FADD_S  : spatz_req.op = VFADD;
              riscv_instr::FSUB_S  : spatz_req.op = VFSUB;
              riscv_instr::FMUL_S  : spatz_req.op = VFMUL;
              riscv_instr::FSGNJ_S : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FSGNJN_S : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FSGNJX_S : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FMIN_S : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FMAX_S : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FCLASS_S : spatz_req.op = VFCLASS;
              riscv_instr::FLE_S    : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FLT_S : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FEQ_S : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FCVT_S_W : spatz_req.op = VI2F;
              riscv_instr::FCVT_S_WU: spatz_req.op = VU2F;
              riscv_instr::FCVT_W_S : begin
                spatz_req.op = VF2I;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_WU_S: begin
                spatz_req.op = VF2U;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FMADD_S  : spatz_req.op = VFMADD;
              riscv_instr::FMSUB_S  : spatz_req.op = VFMSUB;
              riscv_instr::FNMADD_S : spatz_req.op = VFNMADD;
              riscv_instr::FNMSUB_S : spatz_req.op = VFNMSUB;
            endcase
          end else
            illegal_instr = 1'b1;
        end

        // CSR instruction
        riscv_instr::CSRRW,
        riscv_instr::CSRRS,
        riscv_instr::CSRRC,
        riscv_instr::CSRRWI,
        riscv_instr::CSRRSI,
        riscv_instr::CSRRCI: begin
          automatic logic [11:0] csr_addr = decoder_req_i.instr[31:20];
          automatic vreg_t csr_rd         = decoder_req_i.instr[11:7];
          automatic vreg_t csr_rs1        = decoder_req_i.instr[19:15];
          automatic logic csr_is_imm      = decoder_req_i.instr[14];

          spatz_req.op      = VCSR;
          spatz_req.ex_unit = CON;
          spatz_req.rd      = csr_rd;
          spatz_req.use_rd  = 1'b1;
          spatz_req.rs1     = csr_is_imm ? 32'(csr_rs1) : decoder_req_i.rs1;
          illegal_instr     = csr_is_imm ? 1'b0         : ~decoder_req_i.rs1_valid;
          reset_vstart      = 1'b0;

          // Check if CSR access is really destined for Spatz
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

          // Check type of CSR access (read/write)
          unique casez (decoder_req_i.instr)
            riscv_instr::CSRRW,
            riscv_instr::CSRRWI:
              if (csr_addr == riscv_instr::CSR_VSTART) begin
                spatz_req.use_rd              = csr_rd != '0;
                spatz_req.op_cgf.write_vstart = 1'b1;
              end

            riscv_instr::CSRRS,
            riscv_instr::CSRRSI:
              if (csr_addr == riscv_instr::CSR_VSTART)
                spatz_req.op_cgf.set_vstart = csr_rs1 != '0;

            riscv_instr::CSRRC,
            riscv_instr::CSRRCI:
              if (csr_addr == riscv_instr::CSR_VSTART)
                spatz_req.op_cgf.clear_vstart = csr_rs1 != '0;

            default:
              illegal_instr = 1'b1;
          endcase // CSR
        end

        // VSETVL instruction
        riscv_instr::VSETVL,
        riscv_instr::VSETVLI,
        riscv_instr::VSETIVLI: begin
          automatic vreg_t setvl_rs1 = decoder_req_i.instr[19:15];
          automatic vreg_t setvl_rd  = decoder_req_i.instr[11:7];

          spatz_req.rd      = setvl_rd;
          spatz_req.use_rd  = 1'b1;
          spatz_req.op      = VCFG;
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
          end else begin
            illegal_instr = 1'b1;
          end

          // Set to maxvl or new desired value
          spatz_req.rs1            = (setvl_rs1 == 0 && setvl_rd != 0) ? '1 : spatz_req.rs1;
          // Keep vl
          spatz_req.op_cgf.keep_vl = setvl_rs1 == '0 && setvl_rd == '0;
        end

        default: illegal_instr = 1'b1;
      endcase // Opcodes

      // Add correct reset_vstart value
      spatz_req.op_cgf.reset_vstart = illegal_instr ? 1'b0 : reset_vstart;
      spatz_req.rd                  = decoder_req_i.rd;
    end // Instruction valid
  end : decoder

  // Check if rsp valid and assign spatz_req
  assign decoder_rsp_o.spatz_req     = spatz_req;
  assign decoder_rsp_o.instr_illegal = decoder_req_valid_i & illegal_instr;
  assign decoder_rsp_valid_o         = decoder_req_valid_i;

endmodule : spatz_decoder
