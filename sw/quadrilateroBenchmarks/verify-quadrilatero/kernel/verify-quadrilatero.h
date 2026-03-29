// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _VERIFY_
#define _VERIFY_

// Configuration instructions
void __attribute__ ((noinline)) test_cfg ();

// Load instructions
void __attribute__ ((noinline)) test_load_1x1 (float* addrA, int K);
void __attribute__ ((noinline)) test_load_1x2 (float* addrA, int K);
void __attribute__ ((noinline)) test_load_1x4 (float* addrA, int K);
void __attribute__ ((noinline)) test_load_2x1 (float* addrA, int K);
void __attribute__ ((noinline)) test_load_4x1 (float* addrA, int K);
void __attribute__ ((noinline)) test_load_2x2 (float* addrA, int K);

// Store instructions
void __attribute__ ((noinline)) test_store_1x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_store_1x2 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_store_1x4 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_store_2x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_store_4x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_store_2x2 (float* addrA, float* addrC, int K);

// Memory instructions
void __attribute__ ((noinline)) test_load_store_1x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_load_store_1x2 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_load_store_1x4 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_load_store_2x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_load_store_4x1 (float* addrA, float* addrC, int K);
void __attribute__ ((noinline)) test_load_store_2x2 (float* addrA, float* addrC, int K);

// Multiply-accumulate instructions
void __attribute__ ((noinline)) test_macc (float* addrA,float* addrB,float* addrC, int K);
void __attribute__ ((noinline)) test_macc_adv(float* addrA,float* addrB,float* addrC, int K);

// Main test function
void verify_quadrilatero (float* addrA,float* addrB,float* addrC, int K);
#endif