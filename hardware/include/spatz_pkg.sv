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
  localparam int unsigned N_IPU = `ifdef N_IPU `N_IPU `else 2 `endif;
  // Maximum size of a single vector element in bits
  localparam int unsigned ELEN = 32;
  // Maximum size of a single vector element in bytes
  localparam int unsigned ELENB = ELEN / 8;
  // Number of bits in a vector register
  localparam int unsigned VLEN = `ifdef VLEN `VLEN `else 256 `endif;
  // Number of bytes in a vector register
  localparam int unsigned VLENB = VLEN / 8;
  // Maximum vector length in elements
  localparam int unsigned MAXVL = VLEN;
  // Number of vector registers
  localparam int unsigned NRVREG = 32;
  // Number of addressable elements in a vector register
  localparam int unsigned VELE = VLEN/(N_IPU*ELEN);
  // Number of bytes in a vector register element
  localparam int unsigned VELEB = N_IPU*ELENB;

  //////////////////////
  // Type Definitions //
  //////////////////////

  // Vector length register
  typedef logic [$clog2(MAXVL+1)-1:0] vlen_t;
  // Operand register
  typedef logic [$clog2(NRVREG)-1:0] opreg_t;

  // Element of length type
  typedef logic [ELEN-1:0] elen_t;

  // VREG address, byte enable, and data type
  typedef logic [$clog2(NRVREG)+$clog2(VELE)-1:0] vreg_addr_t;
  typedef logic [N_IPU*ELENB-1:0]                 vreg_be_t;
  typedef logic [N_IPU*ELEN-1:0]                  vreg_data_t;

  // Instruction ID
  typedef logic [4:0] instr_id_t;

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
    VCSR
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
    instr_id_t id;

    // Used vector registers
    opreg_t     vs1;
    logic       use_vs1;
    opreg_t     vs2;
    logic       use_vs2;
    opreg_t     vd;
    logic       use_vd;
    logic       vd_is_src;

    // Scalar input values
    elen_t      rs1;
    elen_t      rs2;

    // Destination register
    logic [4:0] rd;
    logic       use_rd;

    // Instruction operation
    op_e        op;
    ex_unit_e   ex_unit;

    // Operation specific details
    op_cfg_t    op_cgf;
    op_csr_t    op_csr;
    op_arith_t  op_arith;
    op_mem_t    op_mem;
    op_sld_t    op_sld;

    // Spatz config details
    vtype_t     vtype;
    vlen_t      vl;
    vlen_t      vstart;
  } spatz_req_t;

  //////////////////////////////////
  // Decoder Request and Response //
  //////////////////////////////////

  typedef struct packed {
    // Request id
    instr_id_t id;

    // Instruction
    riscv_pkg::instr_t instr;

    // Rs values
    elen_t rs1;
    logic  rs1_valid;
    elen_t rs2;
    logic  rs2_valid;
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
    instr_id_t  id;

    // Retiring registers
    opreg_t vs2;
    opreg_t vs1;
    opreg_t vd;

    // WB
    logic       wb;
    logic [4:0] rd;
    elen_t      result;
  } vfu_rsp_t;

  ///////////////////
  // VLSU Response //
  ///////////////////

  typedef struct packed {
    // Instruction ID
    instr_id_t  id;

    // Retiring registers
    opreg_t vd;

    // Did the memory request trigger an exception
    logic exc;
  } vlsu_rsp_t;

  ////////////////////
  // VSLDU Response //
  ////////////////////

  typedef struct packed {
    // Instruction ID
    instr_id_t  id;

    // Retiring registers
    opreg_t vd;
    opreg_t vs2;
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

 endpackage : spatz_pkg
