// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifdef SNRT_INIT_CLS
static inline uint32_t snrt_cls_base_addr() {
    extern volatile uint32_t __cdata_start, __cdata_end;
    extern volatile uint32_t __cbss_start, __cbss_end;
    uint32_t cdata_size = ((uint32_t)&__cdata_end) - ((uint32_t)&__cdata_start);
    uint32_t cbss_size = ((uint32_t)&__cbss_end) - ((uint32_t)&__cbss_start);
    uint32_t l1_end_addr = SNRT_TCDM_START_ADDR +
                           snrt_cluster_idx() * SNRT_CLUSTER_OFFSET +
                           SNRT_TCDM_SIZE;
    return l1_end_addr - cdata_size - cbss_size;
}
#endif

#ifdef SNRT_INIT_TLS
static inline void snrt_init_tls() {
    extern volatile uint32_t __tdata_start, __tdata_end;
    extern volatile uint32_t __tbss_start, __tbss_end;

    size_t size;
    volatile uint32_t tls_ptr;

    // To avoid contentions in main memory, and take advantage of the
    // bandwidth of the DMA, the DM core initializes the TLS section
    // for every core in a cluster.
    if (snrt_is_dm_core()) {
        size = (size_t)(&__tdata_end) - (size_t)(&__tdata_start);

        // First initialize the DM core's .tdata section from main memory
        asm volatile("mv %0, tp" : "=r"(tls_ptr) : :);
        snrt_dma_start_1d((void*)tls_ptr, (void*)(&__tdata_start), size);

        // Then initialize all other cores' .tdata sections from the DM
        // core's. The offset between the TLS section of successive cores
        // is defined in start.S
        size_t tls_offset = (1 << SNRT_LOG2_STACK_SIZE) + 8;
        for (int i = 1; i < snrt_cluster_core_num(); i++) {
            snrt_dma_start_1d((void*)(tls_ptr + i * tls_offset), (void*)tls_ptr,
                              size);
        }

        // Initialize all cores' .tbss sections
        tls_ptr += size;
        size = (size_t)(&__tbss_end) - (size_t)(&__tbss_start);
        for (int i = 0; i < snrt_cluster_core_num(); i++) {
            snrt_dma_start_1d((void*)(tls_ptr + i * tls_offset),
                              (void*)(snrt_zero_memory_ptr()), size);
        }
    }

    snrt_cluster_hw_barrier();
}
#endif

#ifdef SNRT_INIT_BSS
static inline void snrt_init_bss() {
    extern volatile uint32_t __bss_start, __bss_end;

    // Only one core needs to perform the initialization
    if (snrt_cluster_idx() == 0 && snrt_is_dm_core()) {
        size_t size = (size_t)(&__bss_end) - (size_t)(&__bss_start);
        snrt_dma_start_1d_wideptr((uint64_t)(&__bss_start),
                                  (uint64_t)(snrt_zero_memory_ptr()), size);
    }
}
#endif

#ifdef SNRT_INIT_CLS
static inline void snrt_init_cls() {
    extern volatile uint32_t __cdata_start, __cdata_end;
    extern volatile uint32_t __cbss_start, __cbss_end;

    _cls_ptr = (cls_t*)snrt_cls_base_addr();

    // Only one core per cluster has to do this
    if (snrt_is_dm_core()) {
        void* ptr = (void*)snrt_cls_base_addr();
        size_t size;

        // Copy cdata section to base of the TCDM
        size = (size_t)(&__cdata_end) - (size_t)(&__cdata_start);
        snrt_dma_start_1d(ptr, (void*)(&__cdata_start), size);

        // Clear cbss section
        ptr = (void*)((uint32_t)ptr + size);
        size = (size_t)(&__cbss_end) - (size_t)(&__cbss_start);
        snrt_dma_start_1d(ptr, (void*)(snrt_zero_memory_ptr()), size);
    }
}
#endif

#ifdef SNRT_INIT_LIBS
static inline void snrt_init_libs() { snrt_alloc_init(); }
#endif

#ifdef SNRT_CRT0_EXIT
static inline void snrt_exit_default(int exit_code) {
    exit_code = snrt_global_all_to_all_reduction(exit_code);
    if (snrt_global_core_idx() == 0)
        *(snrt_exit_code_destination()) = (exit_code << 1) | 1;
}
#ifndef SNRT_CRT0_ALTERNATE_EXIT
static inline void snrt_exit(int exit_code) { snrt_exit_default(exit_code); }
#endif
#endif

void snrt_main() {
    int exit_code = 0;

#ifdef SNRT_CRT0_CALLBACK0
    snrt_crt0_callback0();
#endif

#ifdef SNRT_INIT_TLS
    snrt_init_tls();
#endif

#ifdef SNRT_CRT0_CALLBACK1
    snrt_crt0_callback1();
#endif

#ifdef SNRT_INIT_BSS
    snrt_init_bss();
#endif

#ifdef SNRT_CRT0_CALLBACK2
    snrt_crt0_callback2();
#endif

#ifdef SNRT_INIT_CLS
    snrt_init_cls();
#endif

#if defined(SNRT_INIT_BSS) || defined(SNRT_INIT_CLS)
    // Single DMA wait call for both snrt_init_bss() and snrt_init_cls()
    if (snrt_is_dm_core()) snrt_dma_wait_all();
#endif

#ifdef SNRT_CRT0_CALLBACK3
    snrt_crt0_callback3();
#endif

#ifdef SNRT_INIT_LIBS
    snrt_init_libs();
#endif

#ifdef SNRT_CRT0_CALLBACK4
    snrt_crt0_callback4();
#endif

#ifdef SNRT_CRT0_PRE_BARRIER
    snrt_cluster_hw_barrier();
#endif

#ifdef SNRT_CRT0_CALLBACK5
    snrt_crt0_callback5();
#endif

#ifdef SNRT_INVOKE_MAIN
    extern int main();
    exit_code = main();
#endif

#ifdef SNRT_CRT0_CALLBACK6
    snrt_crt0_callback6();
#endif

#ifdef SNRT_CRT0_POST_BARRIER
    snrt_cluster_hw_barrier();
#endif

#ifdef SNRT_CRT0_CALLBACK7
    snrt_crt0_callback7();
#endif

#ifdef SNRT_CRT0_EXIT
    snrt_exit(exit_code);
#endif

#ifdef SNRT_CRT0_CALLBACK8
    snrt_crt0_callback8();
#endif
}
