// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

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
  localparam int unsigned VLEN = N_IPU * (`ifdef VLEN `VLEN `else 0 `endif);
  // Number of bytes in a vector register
  localparam int unsigned VLENB = VLEN / 8;
  // Maximum vector length in elements
  localparam int unsigned MAXVL = VLEN;
  // Number of vector registers
  localparam int unsigned NRVREG = 32;
  // Number of elements in a vector register
  localparam int unsigned VELE = VLEN/ELEN;

  //////////////////////
  // Type Definitions //
  //////////////////////

  // Vector length register
  typedef logic [$clog2(MAXVL+1)-1:0] vlen_t;
  // Operad register
  typedef logic [$clog2(NRVREG)-1:0] opreg_t;

  // Element of length type
  typedef logic [ELEN-1:0] elen_t;

  // VREG address type
  typedef logic [4:0][VELE-1:0]   vreg_addr_t;
  typedef logic [N_IPU*ELENB-1:0] vreg_be_t;
  typedef logic [N_IPU*ELEN-1:0]  vreg_data_t;

  /////////////////////
  // Operation Types //
  /////////////////////

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

  typedef enum logic [1:0] {
    CON, LSU, SLD, VFU
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
  } op_mem_t;

  // Result from decoder
  typedef struct packed {
    // Used vector registers
    opreg_t    vs1;
    logic      use_vs1;
    opreg_t    vs2;
    logic      use_vs2;
    opreg_t    vd;
    logic      use_vd;
    logic      vd_is_src;

    // Scalar input values
    elen_t     rs1;
    elen_t     rs2;

    // Destination register
    elen_t     rd;
    logic      use_rd;

    // Instruction operation
    op_e       op;
    ex_unit_e  ex_unit;

    // Operation specific details
    op_cfg_t   op_cgf;
    op_csr_t   op_csr;
    op_arith_t op_arith;
    op_mem_t   op_mem;

    // Spatz config details
    vtype_t    vtype;
    vlen_t     vl;
    vlen_t     vstart;
  } spatz_req_t;

  /////////////////////
  // Decoder Request //
  /////////////////////

  typedef struct packed {
    // Instruction
    riscv_pkg::instr_t instr;
    // Rs values
    elen_t rs1;
    elen_t rs2;
  } decoder_req_t;

  typedef struct packed {
    // Illegal instruction
    logic instr_illegal;
    // Spatz request
    spatz_req_t spatz_req;
  } decoder_rsp_t;

 endpackage : spatz_pkg
