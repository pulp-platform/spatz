// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Matheus Cavalcante, ETH Zurich

package rvv_pkg;

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
    LMUL_F8  = 3'b101,
    LMUL_F4  = 3'b110,
    LMUL_F2  = 3'b111,
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

 endpackage : rvv_pkg
