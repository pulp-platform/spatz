// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Exercises snrt_l3alloc:
//   1. small allocations return non-null pointers inside [_edram, __l3_end)
//   2. sequential allocations advance by ALIGN_UP(size, MIN_CHUNK_SIZE)
//   3. writes/reads to allocated memory round-trip
//   4. an obviously-too-large allocation returns 0 (overflow path)
//
// Run only on the DM core — the test is single-core, and the DM core is the
// one main.c / snRuntime conventions expect to orchestrate init work.

#include <snrt.h>
#include "printf.h"

// Must match alloc.c; not exposed via a public header.
#define MIN_CHUNK_SIZE 256
#define ALIGN_UP(addr, size) (((addr) + (size)-1) & ~((size)-1))

// Linker-provided symbols (see common.ld.in). Take their addresses; the
// value stored "at" them is meaningless.
extern uint32_t _edram;
extern uint32_t __l3_end;

int main() {
    if (!snrt_is_dm_core()) return 0;

    uint32_t errors = 0;
    const uint32_t heap_base = (uint32_t)&_edram;
    const uint32_t heap_end  = (uint32_t)&__l3_end;  // last valid byte of DRAM

    // ----------------------------------------------------------- test 1
    // Small allocation — must return non-null and be inside [heap_base, heap_end].
    void *p1 = snrt_l3alloc(1024);
    uint32_t a1 = (uint32_t)p1;
    if (p1 == 0) {
        printf("[FAIL t1] snrt_l3alloc(1024) returned NULL\r\n");
        errors++;
    } else if (a1 < heap_base) {
        printf("[FAIL t1] p1=%p below heap_base=%p\r\n", p1, (void*)heap_base);
        errors++;
    } else if (a1 > heap_end) {
        printf("[FAIL t1] p1=%p above heap_end=%p\r\n", p1, (void*)heap_end);
        errors++;
    }

    // ----------------------------------------------------------- test 2
    // Second allocation follows p1 with size rounded up to MIN_CHUNK_SIZE.
    // 1024 is already 256-aligned, so p2 == p1 + 1024.
    void *p2 = snrt_l3alloc(2048);
    uint32_t a2 = (uint32_t)p2;
    if (p2 == 0) {
        printf("[FAIL t2] snrt_l3alloc(2048) returned NULL\r\n");
        errors++;
    } else if (a2 != a1 + 1024) {
        printf("[FAIL t2] expected p2=%p, got %p\r\n", (void*)(a1 + 1024), p2);
        errors++;
    }

    // ----------------------------------------------------------- test 3
    // Non-aligned request 500 rounds up to ALIGN_UP(500, 256) = 512.
    void *p3 = snrt_l3alloc(500);
    uint32_t a3 = (uint32_t)p3;
    uint32_t expected_offset_p3 = ALIGN_UP(500, MIN_CHUNK_SIZE);  // 512
    if (p3 == 0) {
        printf("[FAIL t3] snrt_l3alloc(500) returned NULL\r\n");
        errors++;
    } else if (a3 != a2 + 2048) {
        printf("[FAIL t3] expected p3=%p, got %p\r\n", (void*)(a2 + 2048), p3);
        errors++;
    }

    // ----------------------------------------------------------- test 4
    // Tiny request rounds up to MIN_CHUNK_SIZE.
    void *p4 = snrt_l3alloc(1);
    if (p4 == 0) {
        printf("[FAIL t4] snrt_l3alloc(1) returned NULL\r\n");
        errors++;
    } else if ((uint32_t)p4 != a3 + expected_offset_p3) {
        printf("[FAIL t4] expected p4=%p, got %p\r\n", (void*)(a3 + expected_offset_p3), p4);
        errors++;
    }

    // ----------------------------------------------------------- test 5
    // Writes and reads round-trip across the allocated region.
    if (p1 != 0) {
        volatile uint32_t *w = (volatile uint32_t *)p1;
        const uint32_t num_words = 1024 / sizeof(uint32_t);
        w[0]              = 0xDEADBEEFu;
        w[num_words - 1]  = 0xCAFEBABEu;
        if (w[0] != 0xDEADBEEFu) {
            printf("[FAIL t5a] w[0]=%08x expected DEADBEEF\r\n", w[0]);
            errors++;
        }
        if (w[num_words - 1] != 0xCAFEBABEu) {
            printf("[FAIL t5b] w[%u]=%08x expected CAFEBABE\r\n",
                   num_words - 1, w[num_words - 1]);
            errors++;
        }
    }

    // ----------------------------------------------------------- test 6
    // Overflow path: request way more than any reasonable heap.
    // Must return 0 (and must not crash or return a wild pointer).
    void *oops = snrt_l3alloc((size_t)0xF0000000u);
    if (oops != 0) {
        printf("[FAIL t6] overflow alloc returned %p (expected NULL)\r\n", oops);
        errors++;
    }

    // Summary
    printf("\r\n=== snrt_l3alloc test ===\r\n");
    printf("  heap_base = %p\r\n", (void*)heap_base);
    printf("  heap_end  = %p\r\n", (void*)heap_end);
    printf("  p1=%p p2=%p p3=%p p4=%p\r\n", p1, p2, p3, p4);
    printf("  errors: %u\r\n", errors);
    return errors;
}
