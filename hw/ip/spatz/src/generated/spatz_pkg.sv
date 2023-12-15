// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich

package spatz_pkg;

  import rvv_pkg::*;

  //////////////////
  //  Parameters  //
  //////////////////

  // Number of IPUs in each VFU (between 1 and 8)
  localparam int unsigned N_IPU = 1;
  // Number of FPUs in each VFU (between 1 and 8)
  localparam int unsigned N_FPU = 4;
  // Number of FUs in each VFU
  localparam int unsigned N_FU  = N_IPU > N_FPU ? N_IPU : N_FPU;
  // FPU support
  localparam bit FPU            = N_FPU != 0;
  // Single-precision floating point support
  localparam bit RVF            = 1;
  // Double-precision floating-point support
  localparam bit RVD            = 1;
  // Vector support
  localparam bit RVV            = 1;

  // Maximum size of a single vector element in bits
  localparam int unsigned ELEN   = RVD ? 64 : 32;
  // Maximum size of a single vector element in bytes
  localparam int unsigned ELENB  = ELEN / 8;
  // Number of bits in a vector register
  localparam int unsigned VLEN   = 512;
  // Number of bytes in a vector register
  localparam int unsigned VLENB  = VLEN / 8;
  // Maximum vector length in elements
  localparam int unsigned MAXVL  = VLEN;
  // Number of vector registers
  localparam int unsigned NRVREG = 32;

  // Spatz' data width
  localparam int unsigned DataWidth = ELEN;
  // Spatz' strobe width
  localparam int unsigned StrbWidth = ELENB;

  // Width of a VRF word
  localparam int unsigned VRFWordWidth     = N_FU * ELEN;
  // Width of a VRF word (bytes)
  localparam int unsigned VRFWordBWidth    = N_FU * ELENB;
  // Number of addressable words in a vector register
  localparam int unsigned NrWordsPerVector = VLEN/VRFWordWidth;
  // Number of VRF words
  localparam int unsigned NrVRFWords       = NRVREG * NrWordsPerVector;
  // Number of VRF banks
  localparam int unsigned NrVRFBanks       = 4;
  // Number of elements per VRF Bank
  localparam int unsigned NrWordsPerBank   = NrVRFWords / NrVRFBanks;

  // Width of scalar register file adresses
  // Depends on whether we have a FP regfile or not
  localparam int GPRWidth = FPU ? 6 : 5;

  // Number of parallel vector instructions
  localparam int unsigned NrParallelInstructions = 4;

  // Largest element width that Spatz supports
  localparam vew_e MAXEW = RVD ? EW_64 : EW_32;

  //////////////////////
  // Type Definitions //
  //////////////////////

  // Vector length register
  typedef logic [$clog2(MAXVL+1)-1:0] vlen_t;
  // Vector register
  typedef logic [$clog2(NRVREG)-1:0] vreg_t;

  // Element of length type
  typedef logic [ELEN-1:0] elen_t;
  typedef logic [ELENB-1:0] elenb_t;

  // VREG address, byte enable, and data type
  typedef logic [$clog2(NrVRFWords)-1:0] vrf_addr_t;
  typedef logic [N_FU*ELENB-1:0] vrf_be_t;
  typedef logic [N_FU*ELEN-1:0] vrf_data_t;

  // Instruction ID
  typedef logic [$clog2(NrParallelInstructions)-1:0] spatz_id_t;

  /////////////////////
  // Operation Types //
  /////////////////////

  // Vector operations
  typedef enum logic [6:0] {
    // Arithmetic and logic instructions
    VADD, VSUB, VADC, VSBC, VRSUB, VMINU, VMIN, VMAXU, VMAX, VAND, VOR, VXOR,
    // Shifts,
    VSLL, VSRL, VSRA, VNSRL, VNSRA,
    // Merge and Move
    VMERGE, VMV,
    // Mul/Mul-Add
    VMUL, VMULH, VMULHU, VMULHSU, VMACC, VNMSAC, VMADD, VNMSUB,
    // Div
    VDIVU, VDIV, VREMU, VREM,
    // Integer comparison instructions
    VMSEQ, VMSNE, VMSLTU, VMSLT, VMSLEU, VMSLE, VMSGTU, VMSGT,
    // Integer add-with-carry and subtract-with-borrow carry-out instructions
    VMADC, VMSBC,
    // Mask operations
    VMANDNOT, VMAND, VMOR, VMXOR, VMORNOT, VMNAND, VMNOR, VMXNOR,
    // Slide instructions
    VSLIDEUP, VSLIDEDOWN,
    // Load instructions
    VLE, VLSE, VLXE,
    // Store instructions
    VSE, VSSE, VSXE,
    // Config instruction
    VCFG,
    // VCSR
    VCSR,
    // Floating point instructions
    VFADD, VFSUB, VFMUL,
    VFMINMAX, VFSGNJ, VFCMP, VFCLASS,
    VF2I, VF2U, VI2F, VU2F, VF2F,
    VFMADD, VFMSUB, VFNMSUB, VFNMADD, VSDOTP
  } op_e;

  // Execution units
  typedef enum logic [1:0] {
    // Controller
    CON,
    // Load/Store unit
    LSU,
    // Slide unit
    SLD,
    // Functional unit
    VFU
  } ex_unit_e;

  ///////////////////
  // Spatz request //
  ///////////////////

  typedef struct packed {
    logic keep_vl;
    logic write_vstart;
    logic set_vstart;
    logic clear_vstart;
    logic reset_vstart;
  } op_cfg_t;

  typedef struct packed {
    logic [11:0] addr;
  } op_csr_t;

  typedef struct packed {
    logic vm;
    logic use_carry_borrow_in;
    logic is_scalar;
    logic is_narrowing;
    logic is_reduction;
    logic switch_rs1_rd;

    logic widen_vs1;
    logic widen_vs2;

    logic signed_vs1;
    logic signed_vs2;
  } op_arith_t;

  typedef struct packed {
    logic vm;
    logic is_load;
    vew_e ew;
  } op_mem_t;

  typedef struct packed {
    logic vm;
    logic insert;
    logic vmv;
  } op_sld_t;

  // Result from decoder
  typedef struct packed {
    // Instruction ID
    spatz_id_t id;

    // Used vector registers
    vreg_t vs1;
    logic use_vs1;
    vreg_t vs2;
    logic use_vs2;
    vreg_t vd;
    logic use_vd;
    logic vd_is_src;

    // Scalar input values
    elen_t rs1;
    elen_t rs2;
    elen_t rsd;

    // Destination register
    logic [GPRWidth-1:0] rd;
    logic use_rd;

    // Rounding mode
    fpnew_pkg::roundmode_e rm;
    // Format mode
    fpnew_pkg::fmt_mode_t fm;

    // Instruction operation
    op_e op;
    ex_unit_e ex_unit;

    // Operation specific details
    op_cfg_t op_cfg;
    op_csr_t op_csr;
    op_arith_t op_arith;
    op_mem_t op_mem;
    op_sld_t op_sld;

    // Spatz config details
    vtype_t vtype;
    vlen_t vl;
    vlen_t vstart;
  } spatz_req_t;

  //////////////////////////////////
  // Decoder Request and Response //
  //////////////////////////////////

  typedef struct packed {
    // Request id
    logic [GPRWidth-1:0] rd;

    // Instruction
    logic [31:0] instr;

    // Scalar values
    elen_t rs1;
    logic rs1_valid;
    elen_t rs2;
    logic rs2_valid;
    elen_t rsd;
    logic rsd_valid;

    // Spatz config
    vtype_t vtype;
  } decoder_req_t;

  typedef struct packed {
    // Illegal instruction
    logic instr_illegal;
    // Spatz request
    spatz_req_t spatz_req;
  } decoder_rsp_t;

  //////////////////
  // VFU Response //
  //////////////////

  typedef struct packed {
    // Instruction ID
    spatz_id_t id;

    // WB
    elen_t result;
    logic [GPRWidth-1:0] rd;
    logic wb;
  } vfu_rsp_t;

  ///////////////////
  // VLSU Response //
  ///////////////////

  typedef struct packed {
    // Instruction ID
    spatz_id_t id;

    // Did the memory request trigger an exception
    logic exc;
  } vlsu_rsp_t;

  ////////////////////
  // VSLDU Response //
  ////////////////////

  typedef struct packed {
    // Instruction ID
    spatz_id_t id;
  } vsldu_rsp_t;

  //////////////////
  // VRF/SB Ports //
  //////////////////

  typedef enum logic [2:0] {
    VFU_VS2_RD,
    VFU_VS1_RD,
    VFU_VD_RD,
    VLSU_VS2_RD,
    VLSU_VD_RD,
    VSLDU_VS2_RD
  } vreg_port_rd_e;

  typedef enum logic [1:0] {
    VFU_VD_WD,
    VLSU_VD_WD,
    VSLDU_VD_WD
  } vreg_port_wd_e;

  typedef enum logic [3:0] {
    SB_VFU_VS2_RD,
    SB_VFU_VS1_RD,
    SB_VFU_VD_RD,
    SB_VLSU_VS2_RD,
    SB_VLSU_VD_RD,
    SB_VSLDU_VS2_RD,
    SB_VFU_VD_WD,
    SB_VLSU_VD_WD,
    SB_VSLDU_VD_WD
  } sb_port_e;

  /////////////////////////
  //  FPU Configuration  //
  /////////////////////////

  // No support for floating-point division and square-root for now
  localparam bit FDivSqrt = 1'b0;

  localparam int unsigned FLEN = RVD ? 64 : 32;

  localparam fpnew_pkg::fpu_features_t FPUFeatures = RVD ?
  // Double Precision FPU
  '{
    Width        : ELEN,
    EnableVectors: 1'b1,
    EnableNanBox : 1'b1,
    //              FP32  FP64  FP16  FP8   FP16a FP8a
    FpFmtMask    : {1'b1, 1'b1, 1'b1, 1'b1, 1'b1, 1'b1},
    //              INT8  INT16 INT32 INT64
    IntFmtMask   : {1'b1, 1'b1, 1'b1, 1'b1}
  } :
  // Single Precision FPU
  '{
    Width        : ELEN,
    EnableVectors: 1'b1,
    EnableNanBox : 1'b1,
    //              FP32  FP64  FP16  FP8   FP16a FP8a
    FpFmtMask    : {RVF,  1'b0, 1'b1, 1'b1, 1'b0, 1'b0},
    //              INT8  INT16 INT32 INT64
    IntFmtMask   : {1'b1, 1'b1, 1'b1, 1'b0}
  };

  // FP format conversion
  typedef struct packed {
    logic [63:63] sign;
    logic [62:52] exponent;
    logic [51:0] mantissa;
  } spatz_fp64_t;

  typedef struct packed {
    logic [31:31] sign;
    logic [30:23] exponent;
    logic [22:0] mantissa;
  } spatz_fp32_t;

  typedef struct packed {
    logic [15:15] sign;
    logic [14:10] exponent;
    logic [9:0] mantissa;
  } spatz_fp16_t;

  typedef struct packed {
    logic [7:7] sign;
    logic [6:2] exponent;
    logic [1:0] mantissa;
  } spatz_fp8_t;

  function automatic spatz_fp64_t widen_fp32_to_fp64(spatz_fp32_t operand);
    widen_fp32_to_fp64.sign     = operand.sign;
    widen_fp32_to_fp64.exponent = int'(operand.exponent - 127) + 1023;
    widen_fp32_to_fp64.mantissa = {operand.mantissa, 29'b0};
  endfunction

  function automatic spatz_fp32_t widen_fp16_to_fp32(spatz_fp16_t operand);
    widen_fp16_to_fp32.sign     = operand.sign;
    widen_fp16_to_fp32.exponent = int'(operand.exponent - 15) + 127;
    widen_fp16_to_fp32.mantissa = {operand.mantissa, 13'b0};
  endfunction

  function automatic spatz_fp16_t widen_fp8_to_fp16(spatz_fp8_t operand);
    widen_fp8_to_fp16.sign     = operand.sign;
    widen_fp8_to_fp16.exponent = operand.exponent;
    widen_fp8_to_fp16.mantissa = {operand.mantissa, 8'b0};
  endfunction

endpackage : spatz_pkg
