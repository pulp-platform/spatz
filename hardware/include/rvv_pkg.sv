// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

package rvv_pkg;

  import riscv_pkg::*;

  ///////////////////
  //  Definitions  //
  ///////////////////

  typedef enum logic [2:0] {
    EW_8  = 3'b000,
    EW_16 = 3'b001,
    EW_32 = 3'b010,
    EW_64 = 3'b011
  } vew_e;

  typedef enum logic [2:0] {
    LMUL_RES = 3'b100,
    LMUL_1_8 = 3'b101,
    LMUL_1_4 = 3'b110,
    LMUL_1_2 = 3'b111,
    LMUL_1   = 3'b000,
    LMUL_2   = 3'b001,
    LMUL_4   = 3'b010,
    LMUL_8   = 3'b011
  } vlmul_e;

  typedef struct packed {
    logic   vill;
    logic   vma;
    logic   vta;
    vew_e   vsew;
    vlmul_e vlmul;
  } vtype_t;

	///////////////
  //  Opcodes  //
  ///////////////

  typedef enum logic [2:0] {
    OPIVV = 3'b000,
    OPFVV = 3'b001,
    OPMVV = 3'b010,
    OPIVI = 3'b011,
    OPIVX = 3'b100,
    OPFVF = 3'b101,
    OPMVX = 3'b110,
    OPCFG = 3'b111
  } opcodev_func3_e;

  ////////////
  //  CSRs  //
  ////////////

  typedef enum logic [11:0] {
    CSR_VSTART = 12'h008,
    CSR_VXSAT  = 12'h009,
    CSR_VXRM   = 12'h00A,
    CSR_VCSR   = 12'h00F,
    CSR_VL     = 12'hC20,
    CSR_VTYPE  = 12'hC21,
    CSR_VLENB  = 12'hC22
  } vcsr_reg_e;

  ////////////////////
  //  Instructions  //
  ////////////////////

  typedef struct packed {
    logic [31:29] nf;
    logic         mew;
    logic [27:26] mop;
    logic         vm;
    logic [24:20] rs2;
    logic [19:15] rs1;
    logic [14:12] width;
    logic [11:7]  vd;
    logic [6:0]   opcode;
  } vmem_t;

  typedef struct packed {
    logic [31:26]   func6;
    logic           vm;
    logic [24:20]   vs2;
    logic [19:15]   vs1;
    opcodev_func3_e func3;
    logic [11:7]    vd;
    logic [6:0]     opcode;
  } varith_t;

  typedef struct packed {
    logic           func1;
    logic [30:20]   zimm11;
    logic [19:15]   rs1;
    opcodev_func3_e func3;
    logic [11:7]    rd;
    logic [6:0]     opcode;
  } vsetvli_t;

  typedef struct packed {
    logic [31:30]   func2;
    logic [29:20]   zimm11;
    logic [19:15]   uimm5;
    opcodev_func3_e func3;
    logic [11:7]    rd;
    logic [6:0]     opcode;
  } vsetivli_t;

  typedef struct packed {
    logic [31:25]   func7;
    logic [24:20]   rs2;
    logic [19:15]   rs1;
    opcodev_func3_e func3;
    logic [11:7]    rd;
    logic [6:0]     opcode;
  } vsetvl_t;

 endpackage : rvv_pkg