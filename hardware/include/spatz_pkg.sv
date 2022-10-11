// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Domenic Wüthrich, ETH Zurich

package spatz_pkg;

  import rvv_pkg::*;

  //////////////////
  //  Parameters  //
  //////////////////

  // Number of IPUs in VFU (between 2 and 8)
  localparam int unsigned N_IPU  = `ifdef N_IPU `N_IPU `else 2 `endif;
  // Maximum size of a single vector element in bits
  localparam int unsigned ELEN   = 32;
  // Maximum size of a single vector element in bytes
  localparam int unsigned ELENB  = ELEN / 8;
  // Number of bits in a vector register
  localparam int unsigned VLEN   = `ifdef VLEN `VLEN `else 256 `endif;
  // Number of bytes in a vector register
  localparam int unsigned VLENB  = VLEN / 8;
  // Maximum vector length in elements
  localparam int unsigned MAXVL  = VLEN;
  // Number of vector registers
  localparam int unsigned NRVREG = 32;

  // Width of a VRF word
  localparam int unsigned VRFWordWidth     = N_IPU * ELEN;
  // Width of a VRF word (bytes)
  localparam int unsigned VRFWordBWidth    = N_IPU * ELENB;
  // Number of addressable words in a vector register
  localparam int unsigned NrWordsPerVector = VLEN/VRFWordWidth;
  // Number of VRF banks
  localparam int unsigned NrVRFBanks       = NrWordsPerVector; // In order to fix 1 word per bank
  // Number of elements per VRF Bank
  localparam int unsigned NrWordsPerBank   = NrWordsPerVector / NrVRFBanks;

  // Width of scalar register file adresses
  // Depends on whether we have a FP regfile or not
  localparam int GPRWidth = `ifndef FPU 5 `else (5 + `FPU) `endif;

  // Number of parallel vector instructions
  localparam int unsigned NrParallelInstructions = 4;

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
  typedef struct packed {
    logic [$clog2(NRVREG)-1:0] vreg;
    logic [$clog2(NrVRFBanks)-1:0] bank;
  } vreg_addr_t;
  typedef logic [N_IPU*ELENB-1:0] vreg_be_t;
  typedef logic [N_IPU*ELEN-1:0] vreg_data_t;

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
    VFADD, VFSUB, VFMIN, VFMAX, VFSGNJ, VFSGNJN, VFSGNJX,
    VFMUL, VFDIV, VFSQRT, VFCLASS, VFMADD, VFMSUB, VFNMSUB, VFNMADD
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
  } op_arith_t;

  typedef struct packed {
    logic vm;
    logic is_load;
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

    // Instruction operation
    op_e op;
    ex_unit_e ex_unit;

    // Operation specific details
    op_cfg_t op_cgf;
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
    VLSU_VD_RD,
    VSLDU_VS2_RD
  } vreg_port_rd_e;

  typedef enum logic [1:0] {
    VFU_VD_WD,
    VLSU_VD_WD,
    VSLDU_VD_WD
  } vreg_port_wd_e;

  typedef enum logic [2:0] {
    SB_VFU_VS2_RD,
    SB_VFU_VS1_RD,
    SB_VFU_VD_RD,
    SB_VLSU_VD_RD,
    SB_VSLDU_VS2_RD,
    SB_VFU_VD_WD,
    SB_VLSU_VD_WD,
    SB_VSLDU_VD_WD
  } sb_port_e;

  /////////////////////////
  //  FPU Configuration  //
  /////////////////////////

  localparam bit FPU_EN = `ifdef FPU `FPU `else 0 `endif;

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation = '{
    PipeRegs: '{
      // FMA Block
      '{
        3, // FP32
        4, // FP64
        2, // FP16
        1, // FP8
        1  // FP16alt
      },
      // DIVSQRT
      '{1, 1, 1, 1, 1},
      // NONCOMP
      '{1, 1, 1, 1, 1},
      // CONV
      '{2, 2, 2, 2, 2}
    },
    UnitTypes: '{
      '{
        fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED,
        fpnew_pkg::MERGED, fpnew_pkg::MERGED
      }, // FMA
      '{
        fpnew_pkg::DISABLED, fpnew_pkg::DISABLED, fpnew_pkg::DISABLED,
        fpnew_pkg::DISABLED, fpnew_pkg::DISABLED
      }, // DIVSQRT
      '{
        fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL,
        fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL
      }, // NONCOMP
      '{
        fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED,
        fpnew_pkg::MERGED, fpnew_pkg::MERGED
      } // CONV
    },
    PipeConfig: fpnew_pkg::BEFORE
  };

  localparam fpnew_pkg::fpu_features_t FPUFeatures = '{
    Width        : ELEN,
    EnableVectors: 1'b1,
    EnableNanBox : 1'b1,
    FpFmtMask    : {(ELEN >= 32), (ELEN >= 64), 1'b0, 1'b0, 1'b0},
    IntFmtMask   : {1'b0, 1'b0, 1'b1, 1'b0}
  };

endpackage : spatz_pkg
