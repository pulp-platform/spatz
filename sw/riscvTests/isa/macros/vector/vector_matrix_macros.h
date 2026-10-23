// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vector_matrix_macros.h — VME ISA test helpers

#ifndef __VECTOR_MATRIX_MACROS_H__
#define __VECTOR_MATRIX_MACROS_H__

#include <stdint.h>

// Tile subset specifier: tile[30:27], pattern[26:24], index[23:0].
#define TSS(tile, pattern, index) \
    (((uintptr_t)((tile) & 0xF) << 27) | \
     ((uintptr_t)((pattern) & 0x7) << 24) | \
     (uintptr_t)((index) & 0xFFFFFF))
#define TSS_ROW(tile, row) TSS(tile, 0, row)
#define TSS_COL(tile, col) TSS(tile, 1, col)

// mtype and vtype constructors and field extractors.
#define MTYPE(mtwiden, tk, tm) \
    ((uintptr_t)(((mtwiden) & 0x3) | (((tk) & 0x7) << 5) | \
                 (((tm) & 0x3FFF) << 10)))
#define VTYPE(vsew, vlmul, vta, vma, altfmt) \
    ((uintptr_t)(((vlmul) & 0x7) | (((vsew) & 0x7) << 3) | \
                 (((vta) & 0x1) << 6) | (((vma) & 0x1) << 7) | \
                 (((altfmt) & 0x1) << 8)))
#define MTYPE_MTWIDEN(mtype) (((mtype) >>  0) & 0x3)
#define MTYPE_TK(mtype)      (((mtype) >>  5) & 0x7)
#define MTYPE_TM(mtype)      (((mtype) >> 10) & 0x3FFF)

// Zvt v0.4 KMAX dictionary, keyed by SEW and TWIDEN (not their encodings).
#define KMAX_8_1 4
#define KMAX_8_2 4
#define KMAX_8_4 4
#define KMAX_16_1 2
#define KMAX_16_2 2
#define KMAX_16_4 2
#define KMAX_32_1 1
#define KMAX_32_2 1
#define KMAX_64_1 1
#define KMAX_LOOKUP(sew, twiden) KMAX_##sew##_##twiden
#define KMAX(sew, twiden) KMAX_LOOKUP(sew, twiden)

#ifdef VME_DIMENSION_TEST
// Independent configuration model for the dimension ISA tests.
// The current Spatz package sets TileEdge to 8 * 2 elements.
#define VME_TEST_TE 16
typedef struct {
    unsigned sew, twiden, vsew, mtwiden, kmax;
} vme_test_config_t;

#define VME_CONFIG(sew, twiden, vsew, mtwiden) \
    {sew, twiden, vsew, mtwiden, KMAX(sew, twiden)}
static const vme_test_config_t vme_test_configs[] = {
    VME_CONFIG(8, 1, 0, 1), VME_CONFIG(8, 2, 0, 2),
    VME_CONFIG(8, 4, 0, 3), VME_CONFIG(16, 1, 1, 1),
    VME_CONFIG(16, 2, 1, 2), VME_CONFIG(32, 1, 2, 1),
};

static inline uintptr_t vme_test_min(uintptr_t a, uintptr_t b) {
    return a < b ? a : b;
}

static inline uintptr_t vme_test_lmul(const vme_test_config_t *cfg,
                                      uintptr_t vlenb) {
    uintptr_t ete = cfg->sew * cfg->twiden < 64 ? VME_TEST_TE : VME_TEST_TE / 2;
    uintptr_t eve = vlenb * 8 / cfg->sew;
    return vme_test_min((ete + eve - 1) / eve,
                       vme_test_min(8 / cfg->kmax, 8 / cfg->twiden));
}

static inline uintptr_t vme_test_capacity(const vme_test_config_t *cfg,
                                          uintptr_t vlenb) {
    uintptr_t ete = cfg->sew * cfg->twiden < 64 ? VME_TEST_TE : VME_TEST_TE / 2;
    return vme_test_min(ete, vme_test_lmul(cfg, vlenb) * (vlenb * 8 / cfg->sew));
}

static inline uintptr_t vme_test_vtype(const vme_test_config_t *cfg,
                                       uintptr_t vlenb) {
    uintptr_t lmul = vme_test_lmul(cfg, vlenb), enc = 0;
    while (lmul > 1) { lmul >>= 1; ++enc; }
    return VTYPE(cfg->vsew, enc, 1, 1, 0);
}

// Seed all dimensions without relying on another dimension-setting instruction.
static inline void vme_test_setup(const vme_test_config_t *cfg) {
    uintptr_t mtype = MTYPE(cfg->mtwiden, 1, 1);
    uintptr_t vtype = VTYPE(cfg->vsew, 0, 0, 0, 0);
    asm volatile("msetmtype %0, %1" :: "r"(mtype), "r"(vtype) : "memory");
}
#endif

#endif // __VECTOR_MATRIX_MACROS_H__
