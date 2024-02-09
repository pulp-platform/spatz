// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich
//
// The decoder takes in a new instruction that is offloaded to Spatz
// and decodes it.

module spatz_decoder
  import spatz_pkg::*;
  import rvv_pkg::*;
  import fpnew_pkg::roundmode_e;
  import fpnew_pkg::fmt_mode_t;
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
    input  roundmode_e   fpu_rnd_mode_i,
    input  fmt_mode_t    fpu_fmt_mode_i
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
        riscv_instr::VLE64_V,
        riscv_instr::VLSE8_V,
        riscv_instr::VLSE16_V,
        riscv_instr::VLSE32_V,
        riscv_instr::VLSE64_V,
        riscv_instr::VLUXEI8_V,
        riscv_instr::VLUXEI16_V,
        riscv_instr::VLUXEI32_V,
        riscv_instr::VLUXEI64_V,
        riscv_instr::VLOXEI8_V,
        riscv_instr::VLOXEI16_V,
        riscv_instr::VLOXEI32_V,
        riscv_instr::VLOXEI64_V,
        riscv_instr::VSE8_V,
        riscv_instr::VSE16_V,
        riscv_instr::VSE32_V,
        riscv_instr::VSE64_V,
        riscv_instr::VSSE8_V,
        riscv_instr::VSSE16_V,
        riscv_instr::VSSE32_V,
        riscv_instr::VSSE64_V,
        riscv_instr::VSUXEI8_V,
        riscv_instr::VSUXEI16_V,
        riscv_instr::VSUXEI32_V,
        riscv_instr::VSUXEI64_V,
        riscv_instr::VSOXEI8_V,
        riscv_instr::VSOXEI16_V,
        riscv_instr::VSOXEI32_V,
        riscv_instr::VSOXEI64_V: begin
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
            4'b0111: spatz_req.vtype.vsew = EW_64;
            default: illegal_instr        = 1'b1;
          endcase

          spatz_req.op_mem.vm = ls_vm;
          spatz_req.ex_unit   = LSU;

          // Illegal width?
          if (spatz_req.vtype.vsew == EW_64 && MAXEW != EW_64)
            illegal_instr = 1'b1;

          // Check which type of load or store operation is requested
          unique casez (decoder_req_i.instr)
            riscv_instr::VLE8_V,
            riscv_instr::VLE16_V,
            riscv_instr::VLE32_V,
            riscv_instr::VLE64_V: begin
              spatz_req.op             = VLE;
              spatz_req.op_mem.is_load = 1'b1;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
            end

            riscv_instr::VLSE8_V,
            riscv_instr::VLSE16_V,
            riscv_instr::VLSE32_V,
            riscv_instr::VLSE64_V: begin
              spatz_req.op             = VLSE;
              spatz_req.op_mem.is_load = 1'b1;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.rs2            = decoder_req_i.rs2;
            end

            riscv_instr::VLUXEI8_V,
            riscv_instr::VLUXEI16_V,
            riscv_instr::VLUXEI32_V,
            riscv_instr::VLUXEI64_V,
            riscv_instr::VLOXEI8_V,
            riscv_instr::VLOXEI16_V,
            riscv_instr::VLOXEI32_V,
            riscv_instr::VLOXEI64_V: begin
              spatz_req.op             = VLXE;
              spatz_req.op_mem.is_load = 1'b1;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.vs2            = ls_s2;
              spatz_req.use_vs2        = 1'b1;

              // This is an indexed operation
              spatz_req.op_mem.ew  = spatz_req.vtype.vsew;
              spatz_req.vtype.vsew = decoder_req_i.vtype.vsew;
            end

            riscv_instr::VSE8_V,
            riscv_instr::VSE16_V,
            riscv_instr::VSE32_V,
            riscv_instr::VSE64_V: begin
              spatz_req.op             = VSE;
              spatz_req.op_mem.is_load = 1'b0;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.vd_is_src      = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
            end

            riscv_instr::VSSE8_V,
            riscv_instr::VSSE16_V,
            riscv_instr::VSSE32_V,
            riscv_instr::VSSE64_V: begin
              spatz_req.op             = VSSE;
              spatz_req.op_mem.is_load = 1'b0;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.vd_is_src      = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.rs2            = decoder_req_i.rs2;
            end

            riscv_instr::VSUXEI8_V,
            riscv_instr::VSUXEI16_V,
            riscv_instr::VSUXEI32_V,
            riscv_instr::VSUXEI64_V,
            riscv_instr::VSOXEI8_V,
            riscv_instr::VSOXEI16_V,
            riscv_instr::VSOXEI32_V,
            riscv_instr::VSOXEI64_V: begin
              spatz_req.op             = VSXE;
              spatz_req.op_mem.is_load = 1'b0;
              spatz_req.vd             = ls_vd;
              spatz_req.use_vd         = 1'b1;
              spatz_req.vd_is_src      = 1'b1;
              spatz_req.rs1            = decoder_req_i.rs1;
              spatz_req.vs2            = ls_s2;
              spatz_req.use_vs2        = 1'b1;

              // This is an indexed operation
              spatz_req.op_mem.ew  = spatz_req.vtype.vsew;
              spatz_req.vtype.vsew = decoder_req_i.vtype.vsew;
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
        riscv_instr::VREDSUM_VS,
        riscv_instr::VREDAND_VS,
        riscv_instr::VREDOR_VS,
        riscv_instr::VREDXOR_VS,
        riscv_instr::VREDMIN_VS,
        riscv_instr::VREDMINU_VS,
        riscv_instr::VREDMAX_VS,
        riscv_instr::VREDMAXU_VS,
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
        riscv_instr::VWMUL_VV,
        riscv_instr::VWMUL_VX,
        riscv_instr::VWMULU_VV,
        riscv_instr::VWMULU_VX,
        riscv_instr::VWMULSU_VV,
        riscv_instr::VWMULSU_VX,
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
        riscv_instr::VWMACC_VV,
        riscv_instr::VWMACC_VX,
        riscv_instr::VWMACCU_VV,
        riscv_instr::VWMACCU_VX,
        riscv_instr::VWMACCSU_VV,
        riscv_instr::VWMACCSU_VX,
        riscv_instr::VWMACCUS_VX,
        riscv_instr::VMERGE_VVM,
        riscv_instr::VMERGE_VXM,
        riscv_instr::VMERGE_VIM,
        riscv_instr::VMV_V_V,
        riscv_instr::VMV_V_X,
        riscv_instr::VMV_V_I,
        riscv_instr::VMV_S_X,
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

            // Reductions
            riscv_instr::VREDSUM_VS: begin
              spatz_req.op                    = VADD;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDAND_VS: begin
              spatz_req.op                    = VAND;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDOR_VS: begin
              spatz_req.op                    = VOR;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDXOR_VS: begin
              spatz_req.op                    = VXOR;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDMIN_VS: begin
              spatz_req.op                    = VMIN;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDMINU_VS: begin
              spatz_req.op                    = VMINU;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDMAX_VS: begin
              spatz_req.op                    = VMAX;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
            end

            riscv_instr::VREDMAXU_VS: begin
              spatz_req.op                    = VMAXU;
              spatz_req.op_arith.is_reduction = 1'b1;
              // Switch vs1 and vs2
              spatz_req.vs1                   = arith_s2;
              spatz_req.vs2                   = arith_s1;
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

            // Vector Widening Multiply
            riscv_instr::VWMUL_VV,
            riscv_instr::VWMUL_VX: begin
              spatz_req.op                  = VMUL;
              spatz_req.op_arith.widen_vs1  = 1'b1;
              spatz_req.op_arith.signed_vs1 = 1'b1;
              spatz_req.op_arith.widen_vs2  = 1'b1;
              spatz_req.op_arith.signed_vs2 = 1'b1;
            end

            riscv_instr::VWMULU_VV,
            riscv_instr::VWMULU_VX: begin
              spatz_req.op                 = VMUL;
              spatz_req.op_arith.widen_vs1 = 1'b1;
              spatz_req.op_arith.widen_vs2 = 1'b1;
            end

            riscv_instr::VWMULSU_VV,
            riscv_instr::VWMULSU_VX: begin
              spatz_req.op                  = VMUL;
              spatz_req.op_arith.widen_vs1  = 1'b1;
              spatz_req.op_arith.widen_vs2  = 1'b1;
              spatz_req.op_arith.signed_vs2 = 1'b1;
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

            // Vector Widening Multiply-Add
            riscv_instr::VWMACC_VV,
            riscv_instr::VWMACC_VX: begin
              spatz_req.op                  = VMACC;
              spatz_req.vd_is_src           = 1'b1;
              spatz_req.op_arith.widen_vs1  = 1'b1;
              spatz_req.op_arith.signed_vs1 = 1'b1;
              spatz_req.op_arith.widen_vs2  = 1'b1;
              spatz_req.op_arith.signed_vs2 = 1'b1;
            end

            riscv_instr::VWMACCU_VV,
            riscv_instr::VWMACCU_VX: begin
              spatz_req.op                 = VMACC;
              spatz_req.vd_is_src          = 1'b1;
              spatz_req.op_arith.widen_vs1 = 1'b1;
              spatz_req.op_arith.widen_vs2 = 1'b1;
            end

            riscv_instr::VWMACCSU_VV,
            riscv_instr::VWMACCSU_VX: begin
              spatz_req.op                  = VMACC;
              spatz_req.vd_is_src           = 1'b1;
              spatz_req.op_arith.widen_vs1  = 1'b1;
              spatz_req.op_arith.signed_vs1 = 1'b1;
              spatz_req.op_arith.widen_vs2  = 1'b1;
            end

            riscv_instr::VWMACCUS_VX: begin
              spatz_req.op                  = VMACC;
              spatz_req.vd_is_src           = 1'b1;
              spatz_req.op_arith.widen_vs1  = 1'b1;
              spatz_req.op_arith.widen_vs2  = 1'b1;
              spatz_req.op_arith.signed_vs2 = 1'b1;
            end

            // Vector Merge
            riscv_instr::VMERGE_VVM,
            riscv_instr::VMERGE_VXM,
            riscv_instr::VMERGE_VIM: begin
              spatz_req.op = VMERGE;
            end

            riscv_instr::VMV_V_V,
            riscv_instr::VMV_V_X,
            riscv_instr::VMV_S_X,
            riscv_instr::VMV_V_I: begin
              // vmv is the same as a zero slide
              spatz_req.op                 = VSLIDEUP;
              spatz_req.ex_unit            = SLD;
              spatz_req.op_sld.insert      = (func3 == OPIVI || func3 == OPIVX);
              spatz_req.op_sld.vmv         = 1'b1;
              spatz_req.vs2                = spatz_req.vs1;
              spatz_req.use_vs2            = func3 != OPIVI || decoder_req_i.instr inside {riscv_instr::VMV_S_X};
              spatz_req.op_arith.is_scalar = decoder_req_i.instr inside {riscv_instr::VMV_S_X};
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

        // Move to the scalar RF
        riscv_instr::VMV_X_S: begin
          automatic vreg_t arith_s2 = decoder_req_i.instr[24:20];
          automatic vreg_t arith_d  = decoder_req_i.instr[11:7];

          spatz_req.op                 = VADD;
          spatz_req.ex_unit            = VFU;
          spatz_req.rd                 = arith_d;
          spatz_req.use_rd             = 1'b1;
          spatz_req.vs2                = arith_s2;
          spatz_req.use_vs2            = 1'b1;
          spatz_req.op_arith.is_scalar = 1'b1;
        end

        // Vector floating-point instructions
        riscv_instr::VFADD_VV,
        riscv_instr::VFADD_VF,
        riscv_instr::VFSUB_VV,
        riscv_instr::VFSUB_VF,
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
        riscv_instr::VFREDOSUM_VS,
        riscv_instr::VFREDUSUM_VS,
        riscv_instr::VFREDMAX_VS,
        riscv_instr::VFREDMIN_VS,
        riscv_instr::VFCVT_F_X_V,
        riscv_instr::VFCVT_F_XU_V,
        riscv_instr::VFCVT_X_F_V,
        riscv_instr::VFCVT_XU_F_V,
        riscv_instr::VFCVT_RTZ_X_F_V,
        riscv_instr::VFCVT_RTZ_XU_F_V,
        riscv_instr::VFNCVT_XU_F_W,
        riscv_instr::VFNCVT_X_F_W,
        riscv_instr::VFNCVT_RTZ_XU_F_W,
        riscv_instr::VFNCVT_RTZ_X_F_W,
        riscv_instr::VFNCVT_F_XU_W,
        riscv_instr::VFNCVT_F_X_W,
        riscv_instr::VFNCVT_F_F_W,
        riscv_instr::VFMV_V_F,
        riscv_instr::VFMV_S_F,
        riscv_instr::VFWADD_VV,
        riscv_instr::VFWADD_WV,
        riscv_instr::VFWADD_VF,
        riscv_instr::VFWADD_WF,
        riscv_instr::VFWSUB_VV,
        riscv_instr::VFWSUB_WV,
        riscv_instr::VFWSUB_VF,
        riscv_instr::VFWSUB_WF,
        riscv_instr::VFWMUL_VV,
        riscv_instr::VFWMUL_VF,
        riscv_instr::VFWDOTP_VV,
        riscv_instr::VFWDOTP_VF,
        riscv_instr::VFWMACC_VV,
        riscv_instr::VFWMACC_VF,
        riscv_instr::VFWNMACC_VV,
        riscv_instr::VFWNMACC_VF,
        riscv_instr::VFWMSAC_VV,
        riscv_instr::VFWMSAC_VF,
        riscv_instr::VFWNMSAC_VV,
        riscv_instr::VFWNMSAC_VF,
        riscv_instr::VFSLIDE1UP_VF,
        riscv_instr::VFSLIDE1DOWN_VF: begin
          if (spatz_pkg::FPU) begin
            automatic opcodev_func3_e func3 = opcodev_func3_e'(decoder_req_i.instr[14:12]);
            automatic vreg_t arith_s1       = decoder_req_i.instr[19:15];
            automatic vreg_t arith_s2       = decoder_req_i.instr[24:20];
            automatic vreg_t arith_d        = decoder_req_i.instr[11:7];
            automatic logic arith_vm        = decoder_req_i.instr[25];

            spatz_req.op_arith.vm = arith_vm;
            spatz_req.op_sld.vm   = arith_vm;
            spatz_req.use_vs1     = 1'b1;
            spatz_req.vs1         = arith_s2;
            spatz_req.use_vd      = 1'b1;
            spatz_req.vd          = arith_d;
            spatz_req.ex_unit     = VFU;
            spatz_req.rm          = fpu_rnd_mode_i;
            spatz_req.fm          = fpu_fmt_mode_i;

            // Decide which operands to use (vs2 or rs1 or imm)
            unique case (func3)
              OPFVV: begin
                spatz_req.use_vs2 = 1'b1;
                spatz_req.vs2     = arith_s1;
              end
              OPFVF: begin
                spatz_req.rs2 = decoder_req_i.rs1;
              end
              default: illegal_instr = 1'b1;
            endcase

            unique casez (decoder_req_i.instr)
              riscv_instr::VFADD_VV,
              riscv_instr::VFADD_VF: spatz_req.op = VFADD;
              riscv_instr::VFSUB_VV: begin
                spatz_req.op  = VFSUB;
                spatz_req.vs1 = arith_s1;
                spatz_req.vs2 = arith_s2;
              end
              // Switch the operands
              riscv_instr::VFSUB_VF : begin
                spatz_req.op      = VFSUB;
                spatz_req.vs2     = arith_s2;
                spatz_req.use_vs2 = 1'b1;
                spatz_req.rs1     = decoder_req_i.rs1;
                spatz_req.use_vs1 = 1'b0;
              end
              riscv_instr::VFRSUB_VF: spatz_req.op = VFSUB;

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

              riscv_instr::VFMUL_VV,
              riscv_instr::VFMUL_VF: spatz_req.op = VFMUL;
              riscv_instr::VFMACC_VV,
              riscv_instr::VFMACC_VF,
              riscv_instr::VFMADD_VV,
              riscv_instr::VFMADD_VF: begin
                spatz_req.op                     = VFMADD;
                spatz_req.vd_is_src              = 1'b1;
                spatz_req.op_arith.switch_rs1_rd = decoder_req_i.instr inside {riscv_instr::VFMADD_VV, riscv_instr::VFMADD_VF};
              end
              riscv_instr::VFNMACC_VV,
              riscv_instr::VFNMACC_VF,
              riscv_instr::VFNMADD_VV,
              riscv_instr::VFNMADD_VF: begin
                spatz_req.op                     = VFNMADD;
                spatz_req.vd_is_src              = 1'b1;
                spatz_req.op_arith.switch_rs1_rd = decoder_req_i.instr inside {riscv_instr::VFNMADD_VV, riscv_instr::VFNMADD_VF};
              end
              riscv_instr::VFMSAC_VV,
              riscv_instr::VFMSAC_VF,
              riscv_instr::VFMSUB_VV,
              riscv_instr::VFMSUB_VF: begin
                spatz_req.op                     = VFMSUB;
                spatz_req.vd_is_src              = 1'b1;
                spatz_req.op_arith.switch_rs1_rd = decoder_req_i.instr inside {riscv_instr::VFMSUB_VV, riscv_instr::VFMSUB_VF};
              end
              riscv_instr::VFNMSAC_VV,
              riscv_instr::VFNMSAC_VF,
              riscv_instr::VFNMSUB_VV,
              riscv_instr::VFNMSUB_VF: begin
                spatz_req.op                     = VFNMSUB;
                spatz_req.vd_is_src              = 1'b1;
                spatz_req.op_arith.switch_rs1_rd = decoder_req_i.instr inside {riscv_instr::VFNMSUB_VV, riscv_instr::VFNMSUB_VF};
              end

              // Reductions
              riscv_instr::VFREDUSUM_VS,
              riscv_instr::VFREDOSUM_VS: begin
                spatz_req.op                    = VFADD;
                spatz_req.op_arith.is_reduction = 1'b1;
                // Switch vs1 and vs2
                spatz_req.vs1                   = arith_s2;
                spatz_req.vs2                   = arith_s1;
              end

              riscv_instr::VFREDMIN_VS: begin
                spatz_req.op                    = VFMINMAX;
                spatz_req.rm                    = fpnew_pkg::RNE;
                spatz_req.op_arith.is_reduction = 1'b1;
                // Switch vs1 and vs2
                spatz_req.vs1                   = arith_s2;
                spatz_req.vs2                   = arith_s1;
              end

              riscv_instr::VFREDMAX_VS: begin
                spatz_req.op                    = VFMINMAX;
                spatz_req.rm                    = fpnew_pkg::RTZ;
                spatz_req.op_arith.is_reduction = 1'b1;
                // Switch vs1 and vs2
                spatz_req.vs1                   = arith_s2;
                spatz_req.vs2                   = arith_s1;
              end

              riscv_instr::VFSGNJ_VV,
              riscv_instr::VFSGNJ_VF: begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::VFSGNJN_VV,
              riscv_instr::VFSGNJN_VF: begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::VFSGNJX_VV,
              riscv_instr::VFSGNJX_VF: begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::VFCVT_F_X_V    : spatz_req.op = VI2F;
              riscv_instr::VFCVT_F_XU_V   : spatz_req.op = VU2F;
              riscv_instr::VFCVT_X_F_V    : spatz_req.op = VF2I;
              riscv_instr::VFCVT_RTZ_X_F_V: begin
                spatz_req.op = VF2I;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::VFCVT_XU_F_V     : spatz_req.op = VF2U;
              riscv_instr::VFCVT_RTZ_XU_F_V : begin
                spatz_req.op = VF2U;
                spatz_req.rm = fpnew_pkg::RTZ;
              end

              riscv_instr::VFNCVT_F_X_W: begin
                spatz_req.op                    = VI2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end
              riscv_instr::VFNCVT_F_XU_W: begin
                spatz_req.op                    = VU2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end
              riscv_instr::VFNCVT_X_F_W: begin
                spatz_req.op                    = VF2I;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end
              riscv_instr::VFNCVT_RTZ_X_F_W: begin
                spatz_req.op                    = VF2I;
                spatz_req.op_arith.is_narrowing = 1'b1;
                spatz_req.rm                    = fpnew_pkg::RTZ;
              end
              riscv_instr::VFNCVT_XU_F_W: begin
                spatz_req.op                    = VF2U;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end
              riscv_instr::VFNCVT_RTZ_XU_F_W: begin
                spatz_req.op                    = VF2U;
                spatz_req.op_arith.is_narrowing = 1'b1;
                spatz_req.rm                    = fpnew_pkg::RTZ;
              end
              riscv_instr::VFNCVT_F_F_W: begin
                spatz_req.op                    = VF2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end

              riscv_instr::VFMV_V_F,
              riscv_instr::VFMV_S_F: begin
                // vmv is the same as a zero slide
                spatz_req.op                 = VSLIDEUP;
                spatz_req.ex_unit            = SLD;
                spatz_req.op_sld.insert      = 1'b1;
                spatz_req.op_sld.vmv         = 1'b1;
                spatz_req.rs1                = decoder_req_i.rs1;
                spatz_req.use_vs1            = 1'b0;
                spatz_req.vs2                = spatz_req.vs1;
                spatz_req.use_vs2            = decoder_req_i.instr inside {riscv_instr::VFMV_S_F};
                spatz_req.op_arith.is_scalar = decoder_req_i.instr inside {riscv_instr::VFMV_S_F};
              end

              riscv_instr::VFWADD_VV,
              riscv_instr::VFWADD_WV,
              riscv_instr::VFWADD_VF,
              riscv_instr::VFWADD_WF: begin
                spatz_req.op                 = VFADD;
                spatz_req.op_arith.widen_vs1 = !(decoder_req_i.instr inside {riscv_instr::VFWADD_WV, riscv_instr::VFWADD_WF});
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end
              riscv_instr::VFWSUB_VV,
              riscv_instr::VFWSUB_WV: begin
                spatz_req.op                 = VFSUB;
                spatz_req.vs1                = arith_s1;
                spatz_req.vs2                = arith_s2;
                spatz_req.op_arith.widen_vs2 = !(decoder_req_i.instr inside {riscv_instr::VFWSUB_WV, riscv_instr::VFWSUB_WF});
                spatz_req.op_arith.widen_vs1 = 1'b1;
              end
              riscv_instr::VFWSUB_VF,
              riscv_instr::VFWSUB_WF: begin
                spatz_req.op                 = VFSUB;
                spatz_req.vs2                = arith_s2;
                spatz_req.use_vs2            = 1'b1;
                spatz_req.rs1                = decoder_req_i.rs1;
                spatz_req.use_vs1            = 1'b0;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = !(decoder_req_i.instr inside {riscv_instr::VFWSUB_WV, riscv_instr::VFWSUB_WF});
              end
              riscv_instr::VFWMUL_VV,
              riscv_instr::VFWMUL_VF: begin
                spatz_req.op                 = VFMUL;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end

              riscv_instr::VFWDOTP_VV,
              riscv_instr::VFWDOTP_VF: begin
                spatz_req.op        = VSDOTP;
                spatz_req.vd_is_src = 1'b1;
              end
              riscv_instr::VFWMACC_VV,
              riscv_instr::VFWMACC_VF: begin
                spatz_req.op                 = VFMADD;
                spatz_req.vd_is_src          = 1'b1;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end
              riscv_instr::VFWNMACC_VV,
              riscv_instr::VFWNMACC_VF: begin
                spatz_req.op                 = VFNMADD;
                spatz_req.vd_is_src          = 1'b1;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end
              riscv_instr::VFWMSAC_VV,
              riscv_instr::VFWMSAC_VF: begin
                spatz_req.op                 = VFMSUB;
                spatz_req.vd_is_src          = 1'b1;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end
              riscv_instr::VFWNMSAC_VV,
              riscv_instr::VFWNMSAC_VF: begin
                spatz_req.op                 = VFNMSUB;
                spatz_req.vd_is_src          = 1'b1;
                spatz_req.op_arith.widen_vs1 = 1'b1;
                spatz_req.op_arith.widen_vs2 = 1'b1;
              end

              // Slides
              riscv_instr::VFSLIDE1UP_VF: begin
                spatz_req.op            = VSLIDEUP;
                spatz_req.op_sld.insert = 1'b1;
                spatz_req.ex_unit       = SLD;

                spatz_req.rs1     = decoder_req_i.rs1;
                spatz_req.use_vs1 = 1'b0;
                spatz_req.vs2     = arith_s2;
                spatz_req.use_vs2 = 1'b1;
              end

              riscv_instr::VFSLIDE1DOWN_VF: begin
                spatz_req.op            = VSLIDEDOWN;
                spatz_req.op_sld.insert = 1'b1;
                spatz_req.ex_unit       = SLD;

                spatz_req.rs1     = decoder_req_i.rs1;
                spatz_req.use_vs1 = 1'b0;
                spatz_req.vs2     = arith_s2;
                spatz_req.use_vs2 = 1'b1;
              end

              default;
            endcase
          end
        end

        // Move to the scalar FP RF
        riscv_instr::VFMV_F_S: begin
          if (spatz_pkg::FPU) begin
            automatic vreg_t arith_s2 = decoder_req_i.instr[24:20];
            automatic vreg_t arith_d  = decoder_req_i.instr[11:7];

            spatz_req.op                 = VADD;
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = arith_d;
            spatz_req.use_rd             = 1'b1;
            spatz_req.vs2                = arith_s2;
            spatz_req.use_vs2            = 1'b1;
            spatz_req.op_arith.is_scalar = 1'b1;
            // Keep default value (EW_8) if max element length is not 32 bit
            spatz_req.vtype.vsew         = (ELEN == 32) ? EW_32 : EW_8;
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
          spatz_req.vtype.vsew         = EW_32;
          spatz_req.op_arith.is_scalar = 1'b1;

          unique casez (decoder_req_i.instr)
            riscv_instr::MUL   : spatz_req.op = VMUL;
            riscv_instr::MULH  : spatz_req.op = VMULH;
            riscv_instr::MULHU : spatz_req.op = VMULHU;
            riscv_instr::MULHSU: spatz_req.op = VMULHSU;
            default;
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
          spatz_req.vtype.vsew         = EW_32;
          spatz_req.op_arith.is_scalar = 1'b1;

          unique casez (decoder_req_i.instr)
            riscv_instr::DIV : spatz_req.op = VDIV;
            riscv_instr::DIVU: spatz_req.op = VDIVU;
            riscv_instr::REM : spatz_req.op = VREM;
            riscv_instr::REMU: spatz_req.op = VREMU;
            default;
          endcase
        end

        // Scalar byte-precision floating-point instructions
        riscv_instr::FADD_B,
        riscv_instr::FSUB_B,
        riscv_instr::FMUL_B,
        riscv_instr::FSGNJ_B,
        riscv_instr::FSGNJN_B,
        riscv_instr::FSGNJX_B,
        riscv_instr::FMIN_B,
        riscv_instr::FMAX_B,
        riscv_instr::FCLASS_B,
        riscv_instr::FLE_B,
        riscv_instr::FLT_B,
        riscv_instr::FEQ_B,
        riscv_instr::FCVT_B_W,
        riscv_instr::FCVT_B_WU,
        riscv_instr::FCVT_W_B,
        riscv_instr::FCVT_WU_B,
        riscv_instr::FMADD_B,
        riscv_instr::FMSUB_B,
        riscv_instr::FNMSUB_B,
        riscv_instr::FNMADD_B,
        riscv_instr::FCVT_B_H,
        riscv_instr::FCVT_H_B: begin
          if (spatz_pkg::FPU && spatz_pkg::RVF) begin
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = decoder_req_i.instr[11:7];
            spatz_req.use_rd             = 1'b1;
            spatz_req.rs1                = decoder_req_i.rs1;
            spatz_req.rs2                = decoder_req_i.rs2;
            spatz_req.rsd                = decoder_req_i.rsd;
            spatz_req.op_arith.is_scalar = 1'b1;
            spatz_req.rm                 = fpu_rnd_mode_i;
            spatz_req.fm                 = fpu_fmt_mode_i;
            spatz_req.vtype.vsew         = EW_8;

            unique casez (decoder_req_i.instr)
              riscv_instr::FADD_B : spatz_req.op = VFADD;
              riscv_instr::FSUB_B : begin
                spatz_req.op  = VFSUB;
                spatz_req.rs1 = decoder_req_i.rs2;
                spatz_req.rs2 = decoder_req_i.rs1;
              end
              riscv_instr::FMUL_B  : spatz_req.op = VFMUL;
              riscv_instr::FSGNJ_B : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FSGNJN_B : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FSGNJX_B : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FMIN_B : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FMAX_B : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FCLASS_B : spatz_req.op = VFCLASS;
              riscv_instr::FLE_B    : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FLT_B : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FEQ_B : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FCVT_B_W : spatz_req.op = VI2F;
              riscv_instr::FCVT_B_WU: spatz_req.op = VU2F;
              riscv_instr::FCVT_W_B : begin
                spatz_req.op = VF2I;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_WU_B: begin
                spatz_req.op = VF2U;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_H_B : begin
                spatz_req.op                 = VF2F;
                spatz_req.op_arith.widen_vs1 = 1'b1;
              end
              riscv_instr::FCVT_B_H : begin
                spatz_req.op                    = VF2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end
              riscv_instr::FMADD_B  : spatz_req.op = VFMADD;
              riscv_instr::FMSUB_B  : spatz_req.op = VFMSUB;
              riscv_instr::FNMADD_B : spatz_req.op = VFNMADD;
              riscv_instr::FNMSUB_B : spatz_req.op = VFNMSUB;
              default;
            endcase
          end else
            illegal_instr = 1'b1;
        end

        // Scalar half-precision floating-point instructions
        riscv_instr::FADD_H,
        riscv_instr::FSUB_H,
        riscv_instr::FMUL_H,
        riscv_instr::FSGNJ_H,
        riscv_instr::FSGNJN_H,
        riscv_instr::FSGNJX_H,
        riscv_instr::FMIN_H,
        riscv_instr::FMAX_H,
        riscv_instr::FCLASS_H,
        riscv_instr::FLE_H,
        riscv_instr::FLT_H,
        riscv_instr::FEQ_H,
        riscv_instr::FCVT_H_W,
        riscv_instr::FCVT_H_WU,
        riscv_instr::FCVT_W_H,
        riscv_instr::FCVT_WU_H,
        riscv_instr::FMADD_H,
        riscv_instr::FMSUB_H,
        riscv_instr::FNMSUB_H,
        riscv_instr::FNMADD_H,
        riscv_instr::FCVT_H_S,
        riscv_instr::FCVT_S_H: begin
          if (spatz_pkg::FPU && spatz_pkg::RVF) begin
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = decoder_req_i.instr[11:7];
            spatz_req.use_rd             = 1'b1;
            spatz_req.rs1                = decoder_req_i.rs1;
            spatz_req.rs2                = decoder_req_i.rs2;
            spatz_req.rsd                = decoder_req_i.rsd;
            spatz_req.op_arith.is_scalar = 1'b1;
            spatz_req.rm                 = fpu_rnd_mode_i;
            spatz_req.fm                 = fpu_fmt_mode_i;
            spatz_req.vtype.vsew         = EW_16;

            unique casez (decoder_req_i.instr)
              riscv_instr::FADD_H : spatz_req.op = VFADD;
              riscv_instr::FSUB_H : begin
                spatz_req.op  = VFSUB;
                spatz_req.rs1 = decoder_req_i.rs2;
                spatz_req.rs2 = decoder_req_i.rs1;
              end
              riscv_instr::FMUL_H  : spatz_req.op = VFMUL;
              riscv_instr::FSGNJ_H : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FSGNJN_H : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FSGNJX_H : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FMIN_H : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FMAX_H : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FCLASS_H : spatz_req.op = VFCLASS;
              riscv_instr::FLE_H    : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FLT_H : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FEQ_H : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FCVT_H_W : spatz_req.op = VI2F;
              riscv_instr::FCVT_H_WU: spatz_req.op = VU2F;
              riscv_instr::FCVT_W_H : begin
                spatz_req.op = VF2I;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_WU_H: begin
                spatz_req.op = VF2U;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_S_H : begin
                spatz_req.op                 = VF2F;
                spatz_req.op_arith.widen_vs1 = 1'b1;
              end
              riscv_instr::FCVT_H_S : begin
                spatz_req.op                    = VF2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end

              riscv_instr::FMADD_H  : spatz_req.op = VFMADD;
              riscv_instr::FMSUB_H  : spatz_req.op = VFMSUB;
              riscv_instr::FNMADD_H : spatz_req.op = VFNMADD;
              riscv_instr::FNMSUB_H : spatz_req.op = VFNMSUB;
              default;
            endcase
          end else
            illegal_instr = 1'b1;
        end

        // Scalar single-precision floating-point instructions
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
        riscv_instr::FNMADD_S,
        riscv_instr::FCVT_S_D,
        riscv_instr::FCVT_D_S: begin
          if (spatz_pkg::FPU && spatz_pkg::RVF) begin
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = decoder_req_i.instr[11:7];
            spatz_req.use_rd             = 1'b1;
            spatz_req.rs1                = decoder_req_i.rs1;
            spatz_req.rs2                = decoder_req_i.rs2;
            spatz_req.rsd                = decoder_req_i.rsd;
            spatz_req.op_arith.is_scalar = 1'b1;
            spatz_req.rm                 = fpu_rnd_mode_i;
            spatz_req.fm                 = fpu_fmt_mode_i;
            spatz_req.vtype.vsew         = EW_32;

            unique casez (decoder_req_i.instr)
              riscv_instr::FADD_S : spatz_req.op = VFADD;
              riscv_instr::FSUB_S : begin
                spatz_req.op  = VFSUB;
                spatz_req.rs1 = decoder_req_i.rs2;
                spatz_req.rs2 = decoder_req_i.rs1;
              end
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
              riscv_instr::FCVT_D_S: begin
                spatz_req.op                 = VF2F;
                spatz_req.op_arith.widen_vs1 = 1'b1;
              end
              riscv_instr::FCVT_S_D: begin
                spatz_req.op                    = VF2F;
                spatz_req.op_arith.is_narrowing = 1'b1;
              end

              riscv_instr::FMADD_S  : spatz_req.op = VFMADD;
              riscv_instr::FMSUB_S  : spatz_req.op = VFMSUB;
              riscv_instr::FNMADD_S : spatz_req.op = VFNMADD;
              riscv_instr::FNMSUB_S : spatz_req.op = VFNMSUB;
              default;
            endcase
          end else
            illegal_instr = 1'b1;
        end

        // Scalar double-precision floating point instructions
        riscv_instr::FADD_D,
        riscv_instr::FSUB_D,
        riscv_instr::FMUL_D,
        riscv_instr::FSGNJ_D,
        riscv_instr::FSGNJN_D,
        riscv_instr::FSGNJX_D,
        riscv_instr::FMIN_D,
        riscv_instr::FMAX_D,
        riscv_instr::FCLASS_D,
        riscv_instr::FLE_D,
        riscv_instr::FLT_D,
        riscv_instr::FEQ_D,
        riscv_instr::FCVT_D_W,
        riscv_instr::FCVT_D_WU,
        riscv_instr::FCVT_W_D,
        riscv_instr::FCVT_WU_D,
        riscv_instr::FMADD_D,
        riscv_instr::FMSUB_D,
        riscv_instr::FNMSUB_D,
        riscv_instr::FNMADD_D: begin
          if (spatz_pkg::FPU && spatz_pkg::RVD) begin
            spatz_req.ex_unit            = VFU;
            spatz_req.rd                 = decoder_req_i.instr[11:7];
            spatz_req.use_rd             = 1'b1;
            spatz_req.rs1                = decoder_req_i.rs1;
            spatz_req.rs2                = decoder_req_i.rs2;
            spatz_req.rsd                = decoder_req_i.rsd;
            spatz_req.op_arith.is_scalar = 1'b1;
            spatz_req.rm                 = fpu_rnd_mode_i;
            spatz_req.fm                 = fpu_fmt_mode_i;
            spatz_req.vtype.vsew         = EW_64;

            unique casez (decoder_req_i.instr)
              riscv_instr::FADD_D : spatz_req.op = VFADD;
              riscv_instr::FSUB_D : begin
                spatz_req.op  = VFSUB;
                spatz_req.rs1 = decoder_req_i.rs2;
                spatz_req.rs2 = decoder_req_i.rs1;
              end
              riscv_instr::FMUL_D  : spatz_req.op = VFMUL;
              riscv_instr::FSGNJ_D : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FSGNJN_D : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FSGNJX_D : begin
                spatz_req.op = VFSGNJ;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FMIN_D : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FMAX_D : begin
                spatz_req.op = VFMINMAX;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FCLASS_D : spatz_req.op = VFCLASS;
              riscv_instr::FLE_D    : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RNE;
              end
              riscv_instr::FLT_D : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RTZ;
              end
              riscv_instr::FEQ_D : begin
                spatz_req.op = VFCMP;
                spatz_req.rm = fpnew_pkg::RDN;
              end
              riscv_instr::FCVT_D_W : spatz_req.op = VI2F;
              riscv_instr::FCVT_D_WU: spatz_req.op = VU2F;
              riscv_instr::FCVT_W_D : begin
                spatz_req.op = VF2I;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FCVT_WU_D: begin
                spatz_req.op = VF2U;
                spatz_req.rm = fpnew_pkg::roundmode_e'(decoder_req_i.instr[14:12]);
              end
              riscv_instr::FMADD_D  : spatz_req.op = VFMADD;
              riscv_instr::FMSUB_D  : spatz_req.op = VFMSUB;
              riscv_instr::FNMADD_D : spatz_req.op = VFNMADD;
              riscv_instr::FNMSUB_D : spatz_req.op = VFNMSUB;
              default;
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
                spatz_req.op_cfg.write_vstart = 1'b1;
              end

            riscv_instr::CSRRS,
            riscv_instr::CSRRSI:
              if (csr_addr == riscv_instr::CSR_VSTART)
                spatz_req.op_cfg.set_vstart = csr_rs1 != '0;

            riscv_instr::CSRRC,
            riscv_instr::CSRRCI:
              if (csr_addr == riscv_instr::CSR_VSTART)
                spatz_req.op_cfg.clear_vstart = csr_rs1 != '0;

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
          end else if (decoder_req_i.instr[31:30] == 2'b11) begin
            spatz_req.vtype = {1'b0, decoder_req_i.instr[27:20]};
            spatz_req.rs1   = elen_t'(setvl_rs1);
          end else if (decoder_req_i.instr[31:25] == 7'b1000000) begin
            spatz_req.vtype = {1'b0, decoder_req_i.rs2[7:0]};
            spatz_req.rs1   = decoder_req_i.rs1;
          end else begin
            illegal_instr = 1'b1;
          end

          // Set to maxvl or new desired value
          spatz_req.rs1            = (setvl_rs1 == 0 && setvl_rd != 0) ? '1 : spatz_req.rs1;
          // Keep vl
          spatz_req.op_cfg.keep_vl = setvl_rs1 == '0 && setvl_rd == '0;
        end

        default: illegal_instr = 1'b1;
      endcase // Opcodes

      // Add correct reset_vstart value
      spatz_req.op_cfg.reset_vstart = illegal_instr ? 1'b0 : reset_vstart;
      spatz_req.rd                  = decoder_req_i.rd;
    end // Instruction valid
  end : decoder

  // Check if rsp valid and assign spatz_req
  assign decoder_rsp_o.spatz_req     = spatz_req;
  assign decoder_rsp_o.instr_illegal = decoder_req_valid_i & illegal_instr;
  assign decoder_rsp_valid_o         = decoder_req_valid_i;

endmodule : spatz_decoder
