// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   TC1: apply TM, TK, MTWIDEN, and altfmt from register operands.
//   TC2: clamp a register-supplied TK to KMAX while preserving TM.
//   TC3: preserve the vector configuration when matrix mode is disabled.

#include <stdint.h>
#include "vector_macros.h"
#include "vector_matrix_macros.h"

// Restore nonzero TM/TK from rs1 and altfmt from rs2.
void TEST_CASE1(void) {
    uintptr_t mtype_val, vtype_val;
    uintptr_t new_mtype = MTYPE(2, KMAX(16, 2), 4);
    uintptr_t new_vtype = VTYPE(1, 0, 0, 0, 1);

    asm volatile("msetmtype %0, %1" :: "r"(new_mtype), "r"(new_vtype) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype_val));
    asm volatile("csrr %0, vtype" : "=r"(vtype_val));
    XCMP(1, mtype_val, new_mtype);
    XCMP(2, (vtype_val & ~(uintptr_t)7), VTYPE(1, 0, 1, 1, 1));
}

// Clamp register-supplied TK to KMAX while retaining a valid TM.
void TEST_CASE2(void) {
    uintptr_t mtype_val;
    uintptr_t new_mtype = MTYPE(1, 7, 4);
    uintptr_t new_vtype = VTYPE(2, 0, 0, 0, 0);

    asm volatile("msetmtype %0, %1" :: "r"(new_mtype), "r"(new_vtype) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype_val));
    XCMP(3, MTYPE_TK(mtype_val), KMAX(32, 1));
    XCMP(4, MTYPE_TM(mtype_val), 4);
    XCMP(5, MTYPE_MTWIDEN(mtype_val), 1);
}

// With matrix mode disabled, retain rs2's vector LMUL and tail/mask policy.
void TEST_CASE3(void) {
    uintptr_t mtype_val, vtype_val;
    uintptr_t new_mtype = MTYPE(0, 0, 0);
    uintptr_t new_vtype = VTYPE(2, 1, 0, 0, 0); // e32, m2, tu, mu

    asm volatile("msetmtype %0, %1" :: "r"(new_mtype), "r"(new_vtype) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype_val));
    asm volatile("csrr %0, vtype" : "=r"(vtype_val));
    XCMP(6, mtype_val, 0);
    XCMP(7, vtype_val, new_vtype);
}

int main(void) {
    INIT_CHECK();
    TEST_CASE1();
    TEST_CASE2();
    TEST_CASE3();
    EXIT_CHECK();
}
