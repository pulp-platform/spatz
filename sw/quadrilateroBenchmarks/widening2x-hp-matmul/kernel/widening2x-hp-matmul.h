// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _WIDENING2XHPMATMUL_H_
#define _WIDENING2XHPMATMUL_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
void __attribute__ ((noinline)) matrixMul_8x8(int16_t* addrA,int16_t* addrB, int32_t* addrC, int K, int N, int M, int shift);

#endif