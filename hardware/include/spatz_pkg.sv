// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

package spatz_pkg;

  import rvv_pkg::*;

	//////////////////
  //  Parameters  //
  //////////////////

  // Maximum size of a single vector element in bits
  localparam int unsigned ELEN = 32;
  // Maximum size of a single vector element in bytes
  localparam int unsigned ELENB = ELEN / 8;
  // Number of bits in a vector register
  localparam int unsigned VLEN = `ifdef VLEN `VLEN `else 0 `endif;
  // Number of bytes in a vector register
  localparam int unsigned VLENB = VLEN / 8;
  // Maximum vector length in elements
  localparam int unsigned MAXVL = VLEN;

  //////////////////////
  // Type Definitions //
  //////////////////////

  // Vector length register
  typedef logic [$clog2(MAXVL+1)-1:0] vlen_t;
  // Operad register
  typedef logic [4:0] opreg_t;

  // Element of length type
  typedef logic [ELEN-1:0] elen_t;

  /////////////////////
  // Operation Types //
  /////////////////////

  typedef enum logic [6:0] {
    // Arithmetic and logic instructions
    VADD, VSUB, VADC, VSBC, VRSUB, VMINU, VMIN, VMAXU, VMAX, VAND, VOR, VXOR,
    // Shifts,
    VSLL, VSRL, VSRA, VNSRL, VNSRA,
    // Merge
    VMERGE,
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
    VCFG
  } op_e;

  ///////////////////
  // Spatz request //
  ///////////////////

  typedef struct packed {
    logic keep_vl;
  } op_cfg_t;

  // Result from decoder
  typedef struct packed {
    // Used vector registers
    opreg_t   vs1;
    logic     use_vs1;
    opreg_t   vs2;
    logic     use_vs2;
    opreg_t   vd;
    logic     use_vd;

    // Scalar input values
    elen_t    rs1;
    elen_t    rs2;

    // Destination register
    elen_t    rd;
    logic     use_rd;

    // Instruction operation
    op_e      op;

    op_cfg_t  op_cgf;

    // Spatz config details
    vtype_t   vtype;
    vlen_t    vl;
    vlen_t    vstart;
  } spatz_req_t;

 endpackage : spatz_pkg