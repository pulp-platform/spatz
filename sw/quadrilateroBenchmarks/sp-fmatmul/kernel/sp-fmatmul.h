// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _SPFMATMUL_H_
#define _SPFMATMUL_H_

void __attribute__ ((noinline)) matrixMul_8x8(float* addrA,float* addrB,float* addrC, int K, int N, int M, int shift);

#endif