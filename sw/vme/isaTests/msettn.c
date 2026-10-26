// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Pei-Yu Lin, ETH Zürich
//
// Test cases:
//   For every supported SEW/TWIDEN pair, check TN requests below, at, and above
//   LMUL * EVE and TE, including a zero source and preserved matrix/vector state.

#include <stdint.h>
#include "vector_macros.h"
#define VME_DIMENSION_TEST
#include "vector_matrix_macros.h"

// Each case checks one request independently of previous test state.
static void check_request(const vme_test_config_t *cfg, uintptr_t vlenb,
                          uintptr_t request, unsigned id, int zero_source) {
    uintptr_t rd, mtype, vtype, vl;
    uintptr_t expected = vme_test_min(request, vme_test_capacity(cfg, vlenb));
    uintptr_t initial_mtype = MTYPE(cfg->mtwiden, 1, 1);
    vme_test_setup(cfg);
    asm volatile("csrr %0, vtype" : "=r"(vtype));
    XCMP(id, vtype, vme_test_vtype(cfg, vlenb));
    asm volatile("csrr %0, 0xC23" : "=r"(mtype));
    XCMP(id + 1, mtype, initial_mtype);

    if (zero_source)
        asm volatile("msettn %0, zero" : "=r"(rd) :: "memory");
    else
        asm volatile("msettn %0, %1" : "=r"(rd) : "r"(request) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype));
    asm volatile("csrr %0, vl" : "=r"(vl));
    asm volatile("csrr %0, vtype" : "=r"(vtype));
    XCMP(id + 2, rd, expected);
    XCMP(id + 3, vl, expected);
    XCMP(id + 4, mtype, initial_mtype);
    XCMP(id + 5, vtype, vme_test_vtype(cfg, vlenb));

    // rd=x0 discards the result but must still update the destination CSR.
    vme_test_setup(cfg);
    asm volatile("msettn zero, %0" :: "r"(request) : "memory");
    asm volatile("csrr %0, 0xC23" : "=r"(mtype));
    asm volatile("csrr %0, vl" : "=r"(vl));
    XCMP(id + 7, vl, expected);
}

int main(void) {
    INIT_CHECK();
    uintptr_t vlenb;
    asm volatile("csrr %0, vlenb" : "=r"(vlenb));
    for (unsigned c = 0; c < sizeof(vme_test_configs) / sizeof(vme_test_configs[0]); ++c) {
        const vme_test_config_t *cfg = &vme_test_configs[c];
        uintptr_t limit = vme_test_capacity(cfg, vlenb);
        uintptr_t requests[] = {0, 1, limit - 1, limit, limit + 1, UINTPTR_MAX, 0};
        printf("msettn: SEW=%u TWIDEN=%u TE=%u capacity=%u\n",
               cfg->sew, cfg->twiden, VME_TEST_TE, (unsigned)limit);
        for (unsigned r = 0; r < sizeof(requests) / sizeof(requests[0]); ++r)
            check_request(cfg, vlenb, requests[r], 100 * (c + 1) + 10 * r, r == 6);
    }
    EXIT_CHECK();
}
