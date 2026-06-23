// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich



package spatz_pkg;

  import rvv_pkg::*;
  import vtl_pkg::*;
  import cf_math_pkg::idx_width;

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

  // Number of VLSU interfaces
  localparam int unsigned NumVLSUInterfaces = 1;

  // Width of scalar register file adresses
  // Depends on whether we have a FP regfile or not
  localparam int GPRWidth = FPU ? 6 : 5;

  // Number of parallel vector instructions. Ventaglio needs more in-flight
  // slots (vfx, vlx32, vventclr can stack with regular vector ops) so the
  // scoreboard is widened to 8 when VENTAGLIO is defined; otherwise stays
  // at the upstream default of 4.
`ifdef VENTAGLIO
  localparam int unsigned NrParallelInstructions = 8;
`else
  localparam int unsigned NrParallelInstructions = 4;
`endif

  // Largest element width that Spatz supports
  localparam vew_e MAXEW = RVD ? EW_64 : EW_32;

  // Encodes both the scalar RD and the VD address in the VRF
  localparam int VFURespAddrWidth  = GPRWidth > $clog2(NrVRFWords) ? GPRWidth : $clog2(NrVRFWords);

  //////////////////////
  // Type Definitions //
  //////////////////////

  // Vector length register
`ifdef VENTAGLIO
  typedef logic [$clog2(MAXVL+1)+1:0] vlen_t;
`else
  typedef logic [$clog2(MAXVL+1)-1:0] vlen_t;
`endif
  // Vector register
  typedef logic [$clog2(NRVREG)-1:0] vreg_t;

  // Element of length type
  typedef logic [ELEN-1:0] elen_t;
  typedef logic [ELENB-1:0] elenb_t;

  // VREG address, byte enable, and data type
  typedef logic [$clog2(NrVRFWords)-1:0] vrf_addr_t;
  typedef logic [VFURespAddrWidth-1:0] vfu_rsp_addr_t;
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
    // Ventaglio (VTL) custom load: vlx32.v — indexed load whose index
    // vreg lives in the VTL bank rather than the regular VRF.
    VLX,
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
    VFMADD, VFMSUB, VFNMSUB, VFNMADD, VSDOTP,
    // Ventaglio indexed fused-multiply ops (vfxmacc.vrf / vfxmul.vrf)
    VFXMADD
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
    // Ventaglio (VTL) CSR control flags. Decoded from the vcsr immediates
    // 0x7c3..0x7c6; consumed in the controller's proc_vcsr block.
    logic vtl_redirect;          // 0x7c3: write VTL vreg bitmap
    logic set_vtl_index_width;   // 0x7c4: encode IDXW_*
    logic set_vtl_blk_size;      // 0x7c5: encode BLK_*
    logic set_vtl_ratio;         // 0x7c6: encode SP_RATIO_*
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

  ////////////////////////////////////
  // Ventaglio (VTL) operand details //
  ////////////////////////////////////
  // The sparse-format config CSRs (set via vcsr 0x7c4..0x7c6). Live in
  // every spatz_req so the controller and ventaglio see a consistent
  // snapshot of the format in effect when the op was issued.
  typedef struct packed {
    sp_idxw_e  sp_cfg_index_width;
    // TODO: planned for future sparse-format BLK_SIZE configurability;
    // currently set from `csrwi 0x7c5` but not consumed by any datapath.
    sp_blk_e   sp_cfg_blk_size;
    sp_ratio_e sp_cfg_ratio;
  } sp_cfg_t;

  // Per-op VTL operand metadata. Set by the decoder for vfxmacc.vrf /
  // vfxmul.vrf / vlx32.v / vventclr; left at all-zero for any op that
  // doesn't touch the Ventaglio bank. Consumed by the controller's
  // scoreboard (vtl_table) and by ventaglio itself.
  typedef struct packed {
    logic vm;

    // general flag
    logic use_vtl;       // any operand go through VTL
    logic is_load_idx;   // vlx32.v: this load targets the VTL bank's index vreg

    logic gather_vd;     // vfx ops with vd_is_src: pre-gather old vd
    logic scatter_vd;    // vfx ops: scatter result back through index
    logic clear_buffer;  // vventclr: zero the entire ventaglio bank

    vreg_t old_vd;       // decoded vd (before VTL ratio remap)
    vreg_t idx_vreg;     // explicit index vreg for vfxmacc.vrf / vfxmul.vrf
    sp_cfg_t sp_cfg;
  } op_vtl_t;

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
    op_vtl_t op_vtl;

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

    // Interface that is committing
    logic intf_id;
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

  typedef enum logic [idx_width(4 + 2 * 1):0] {
    VFU_VS2_RD,
    VFU_VS1_RD,
    VFU_VD_RD,
    VLSU_VS2_RD,
    VLSU_VD_RD,
    VSLDU_VS2_RD
  } vreg_port_rd_e;

  typedef enum logic [idx_width(2 + 1):0] {
    VFU_VD_WD,
    VLSU_VD_WD,
    VSLDU_VD_WD
  } vreg_port_wd_e;

  typedef enum logic [idx_width(6 + 3 * 1):0] {
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

  //////////////////////////////
  // Ventaglio Configurations //
  //////////////////////////////

  // Ventaglio internal wide datapath. By default Ventaglio supports 4x
  // scatter/gather; the bank is sliced into VENTAGLIO_WFACTOR channels.
  localparam int unsigned VENTAGLIO_WFACTOR     = `ifdef VENTAGLIO_WFACTOR `VENTAGLIO_WFACTOR `else 4 `endif;
  // Buffer size in bit. By default 8192 (8K-bit).
  localparam int unsigned VENTAGLIO_BUFFER_SIZE = `ifdef VENTAGLIO_BUFFER_SIZE `VENTAGLIO_BUFFER_SIZE `else 8192 `endif;

  // Ventaglio wide datapath types (one element per channel slot)
  typedef logic [VENTAGLIO_WFACTOR*N_FU*ELENB-1:0] ventaglio_wide_be_t;
  typedef logic [VENTAGLIO_WFACTOR*N_FU*ELEN-1:0]  ventaglio_wide_data_t;
  // Ventaglio narrow datapath types (one channel slot)
  typedef logic                   [N_FU*ELENB-1:0] ventaglio_narrow_be_t;
  typedef logic                   [N_FU*ELEN-1:0]  ventaglio_narrow_data_t;

  // Buffer layout:
  // | ------------------------------ Buffer ------------------------------- |
  // | --- Channel --- | --- Channel --- | --- Channel --- | --- Channel --- |
  // | -Bank- | -Bank- | -Bank- | -Bank- | -Bank- | -Bank- | -Bank- | -Bank- |
  //
  // VTGNrChannels represents the max asymmetric bandwidth ratio (1:4 -> 4,
  // 1:2 -> 2). Default 4 (= 1:4 support).
  localparam int unsigned VTGNrChannels         = VENTAGLIO_WFACTOR;
  localparam int unsigned VTGNrBanksPerChannel  = N_FU;
  localparam int unsigned VTGChannelWidth       = VTGNrBanksPerChannel * ELEN;
  localparam int unsigned VTGChannelBWidth      = VTGNrBanksPerChannel * ELENB;
  localparam int unsigned VTGNrWordsPerChannel  = VENTAGLIO_BUFFER_SIZE / (VTGNrChannels * VTGChannelWidth);

endpackage : spatz_pkg
