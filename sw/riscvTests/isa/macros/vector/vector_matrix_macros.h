// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Authors: Pei-Yu Lin (peilin@ethz.ch)
//
// vector_matrix_macros.h — VME ISA test helpers

#ifndef __VECTOR_MATRIX_MACROS_H__
#define __VECTOR_MATRIX_MACROS_H__

#include "dataset.h"
#include "encoding.h"
#include <stdint.h>
#include <string.h>

// -------------------------------------------------------
// TSS (Tile Subset Specifier) — VME Spec §1.5
//   bits[30:27] = tile index  (0–15)
//   bits[26:24] = pattern     (0 = row, 1 = col)
//   bits[23:0]  = row / col index
// -------------------------------------------------------
#define TSS_ROW(tile, row) \
    (((uintptr_t)((tile) & 0xF) << 27) | \
     (uintptr_t)((row)  & 0xFFFFFF))

#define TSS_COL(tile, col) \
    (((uintptr_t)((tile) & 0xF) << 27) | \
     ((uintptr_t)1 << 24)               | \
     (uintptr_t)((col)  & 0xFFFFFF))

// -------------------------------------------------------
// mtype CSR (0xC23) field extractors
// -------------------------------------------------------
#define MTYPE_MTWIDEN(mtype) (((mtype) >>  0) & 0x3)
#define MTYPE_TK(mtype)      (((mtype) >>  5) & 0x7)
#define MTYPE_TM(mtype)      (((mtype) >> 10) & 0x3FFF)

#define KMAX 4

// -------------------------------------------------------
// Expected mtype / vtype values (for msetmtype* tests)
//
//   mtype = uimm5 & 0x1F
//   vtype = (vsew << 3) | (mtwiden != 0 ? 0xC0 : 0x00)
//           where vsew = uimm3, mtwiden = uimm5 & 0x3
// -------------------------------------------------------
#define MTYPE_EXP(uimm5) \
    ((uintptr_t)((uimm5) & 0x1Fu))

#define VTYPE_EXP(uimm5, uimm3) \
    ((uintptr_t)(((uimm3) & 0x7u) << 3) | \
     (((uimm5) & 0x3u) ? (uintptr_t)0xC0u : (uintptr_t)0u))

#define VME_VTYPE(vsew_enc) \
    ((uintptr_t)(((vsew_enc) & 0x7u) << 3) | 0xC0u)

#define MTYPE_VAL(mtwiden) \
    ((uintptr_t)((mtwiden) & 0x3u))

// -------------------------------------------------------
// enable_vme — set mstatus.MS = Initial (01b)
//
//   mstatus.MS = bits[30:29]  (MSTATUS_MS = 0x60000000)
//   Initial state = 01b → set bit 29 only
//   Same pattern as enable_vec() / enable_fp().
// -------------------------------------------------------
#ifndef MSTATUS_MS
#define MSTATUS_MS 0x60000000UL
#endif

#define enable_vme() \
    do { \
        asm volatile("csrs mstatus, %[bits]" \
                     :: [bits]"r"(MSTATUS_MS & (MSTATUS_MS >> 1))); \
    } while (0)

#endif // __VECTOR_MATRIX_MACROS_H__
