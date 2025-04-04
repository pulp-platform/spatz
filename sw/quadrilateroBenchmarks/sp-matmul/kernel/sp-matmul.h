// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _SPMATMUL_H_
#define _SPMATMUL_H_

void __attribute__ ((noinline)) matrixMul_8x8(int* addrA,int* addrB,int* addrC, int K, int N, int M, int shift);

#endif