// Copyright 2024 ETH Zurich and University of Bologna.
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
// Internal helper: stringify a compile-time literal
// so it can be embedded in an inline-asm string.
// -------------------------------------------------------
#define _VME_STR(x) #x

// -------------------------------------------------------
// TSS (Tile Subset Specifier) — VME Spec Section 1.5
//   bits[30:27] = tile index  (0-15)
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
// TILE_CONFIG — configure matrix tile engine dimensions
//
//   mtwiden_imm  : widening factor (compile-time literal)
//     0x3 → TWIDEN=4  (e8→e32 or e16→fp32 widening path)
//     0x1 → TWIDEN=1  (no widening, direct readback path)
//     0x0 → TWIDEN=1  (no widening, e32 non-widen path)
//   vsew_enc_imm : SEW encoding (compile-time literal)
//     0x0 → e8
//     0x1 → e16
//     0x2 → e32
//
//   Both imm arguments MUST be integer literals so they can
//   be spliced directly into the msetmtypei asm string.
// -------------------------------------------------------
#define TILE_CONFIG(tn, tm, tk, mtwiden_imm, vsew_enc_imm) \
    do { asm volatile( \
        "msetmtypei " _VME_STR(mtwiden_imm) ", " _VME_STR(vsew_enc_imm) "\n\t" \
        "msettn x0, %0\n\t" \
        "msettm x0, %1\n\t" \
        "msettk x0, %2\n\t" \
        :: "r"((uintptr_t)(tn)), \
           "r"((uintptr_t)(tm)), \
           "r"((uintptr_t)(tk)) \
        : "memory"); } while (0)

// -------------------------------------------------------
// TILE_READBACK — switch tile engine to readback mode
//
//   Always uses mtwiden=1 (no widening during readback).
//   vsew_enc_imm selects the accumulator element width:
//     0x1 → e16  (fp16 accumulator)
//     0x2 → e32  (int32 or fp32 accumulator)
//
//   vsew_enc_imm MUST be a compile-time integer literal.
// -------------------------------------------------------
#define TILE_READBACK(tn, tm, vsew_enc_imm) \
    do { asm volatile( \
        "msetmtypei 1, " _VME_STR(vsew_enc_imm) "\n\t" \
        "msettn x0, %0\n\t" \
        "msettm x0, %1\n\t" \
        :: "r"((uintptr_t)(tn)), \
           "r"((uintptr_t)(tm)) \
        : "memory"); } while (0)

// -------------------------------------------------------
// Named TILE_CONFIG aliases
//
//   vme_cfg_e8   : int8/uint8 inputs → int32 accumulator
//                  mtwiden=0x3 (TWIDEN=4), SEW=e8, TEW=32
//   vme_cfg_e16 : int16, fp16 inputs → fp16 or fp32 accumulator
//                  mtwiden=0x2 (TWIDEN=2), SEW=e16, TEW=32
//   vme_cfg_e32  : int32 inputs → int32 accumulator
//                  mtwiden=0x1 (TWIDEN=1), SEW=e32, TEW=32
// -------------------------------------------------------
#define vme_cfg_e8(tn, tm, tk)    TILE_CONFIG(tn, tm, tk, 0x3, 0x0)
#define vme_cfg_e16(tn, tm, tk)  TILE_CONFIG(tn, tm, tk, 0x2, 0x1)
#define vme_cfg_e32(tn, tm, tk)   TILE_CONFIG(tn, tm, tk, 0x1, 0x2)
#ifdef TEW8
#define vme_cfg_e8(tn, tm, tk)  TILE_CONFIG(tn, tm, tk, 0x1, 0x0)
#endif
#ifdef TEW16
#define vme_cfg_e8(tn, tm, tk)  TILE_CONFIG(tn, tm, tk, 0x2, 0x0)
#define vme_cfg_e16(tn, tm, tk)   TILE_CONFIG(tn, tm, tk, 0x1, 0x1)
#endif
#ifdef TEW64
#define vme_cfg_e16(tn, tm, tk)   TILE_CONFIG(tn, tm, tk, 0x3, 0x1)
#define vme_cfg_e32(tn, tm, tk)   TILE_CONFIG(tn, tm, tk, 0x2, 0x2)
#define vme_cfg_e64(tn, tm, tk)   TILE_CONFIG(tn, tm, tk, 0x1, 0x3)
#endif
// -------------------------------------------------------
// Named TILE_READBACK aliases
//
//   vme_readback_e32  : readback int32 or fp32 accumulator
//                       mtwiden=1, SEW=e32
//   vme_readback_e16 : readback fp16 accumulator
//                       mtwiden=1, SEW=e16
// -------------------------------------------------------
#define vme_readback_e32(tn, tm)   TILE_READBACK(tn, tm, 0x2)
#define vme_readback_e16(tn, tm)  TILE_READBACK(tn, tm, 0x1)

// -------------------------------------------------------
// ALTFMT CSR (address 0xC21, bit[8])
//   ALTFMT=0 → vtmms: A=int8 * B=uint8
//   ALTFMT=1 → vtmms: A=int8 * B=int8
//            → vtfmm.alt: fp16 * fp16 → fp32 accumulator
// -------------------------------------------------------
#define read_altfmt(buf) \
    do { asm volatile("csrr %[B], 0xC21" : [B]"=r"(buf)); } while (0)

#define set_altfmt() \
    do { asm volatile("csrs 0xC21, %0" :: "r"(256) : "memory"); } while (0)

#define clear_altfmt() \
    do { asm volatile("csrc 0xC21, %0" :: "r"(256) : "memory"); } while (0)

// -------------------------------------------------------
// mtype CSR (address 0xC23) — read helper + field extractors
// -------------------------------------------------------
#define read_mtype(buf) \
    do { asm volatile("csrr %[B], 0xC23" : [B]"=r"(buf)); } while (0)

#define MTYPE_TK(mtype)      (((mtype) >>  5) & 0x7)
#define MTYPE_TM(mtype)      (((mtype) >> 10) & 0x3FFF)
#define MTYPE_MTWIDEN(mtype) (((mtype) >>  0) & 0x3)

#define KMAX 4

// -------------------------------------------------------
// Expected mtype / vtype values (for msetmtype tests)
//
//   mtype = uimm5 & 0x1F
//   vtype = (vsew << 2) | (mtwiden != 0 ? 0xC0 : 0x00)
//           where vsew = uimm2, mtwiden = uimm5 & 0x3
// -------------------------------------------------------
#define MTYPE_EXP(uimm5) \
    ((uintptr_t)((uimm5) & 0x1Fu))

#define VTYPE_EXP(uimm5, uimm2) \
    ((uintptr_t)(((uimm2) & 0x7u) << 2) | \
     (((uimm5) & 0x3u) ? (uintptr_t)0xC0u : (uintptr_t)0u))

// vtype encoding helpers
//   VME widening path: vta=1, vma=1 → bits[7:6] = 0b11
#define VME_VTYPE(vsew_enc)  ((uintptr_t)(((vsew_enc) & 0x7u) << 2) | 0xC0u)
#define STD_VTYPE(vsew_enc)  ((uintptr_t)(((vsew_enc) & 0x7u) << 2))

// mtype encoding: mtwiden in bits[1:0]
#define MTYPE_VAL(mtwiden)   ((uintptr_t)((mtwiden) & 0x3u))

// -------------------------------------------------------
// Instruction wrappers
//
//   msetmtypei(uimm5, uimm2)  — both args must be integer literals
//   msetmtype(mtype, vtype)   — register form, runtime values ok
//
//   msettn(rd, val)    write new tn; rd receives actual vl
//   msettn_x0(val)     write new tn; discard rd (x0)
//   msettm(rd, val)    write new tm; rd receives actual tm
//   msettm_x0(val)     write new tm; discard rd (x0)
//   msettk(rd, val)    write new tk; rd receives actual tk
//   msettk_x0(val)     write new tk; discard rd (x0)
// -------------------------------------------------------
#define msetmtypei(uimm5, uimm2) \
    do { asm volatile("msetmtypei %0, %1" \
                      :: "i"(uimm5), "i"(uimm2) \
                      : "memory"); } while (0)

#define msetmtype(mtype, vtype) \
    do { asm volatile("msetmtype %0, %1" \
                      :: "r"((uintptr_t)(mtype)), \
                         "r"((uintptr_t)(vtype)) \
                      : "memory"); } while (0)

#define msettn(rd, val) \
    asm volatile("msettn %0, %1" \
                 : "=r"(rd) \
                 : "r"((uintptr_t)(val)) \
                 : "memory")

#define msettn_x0(val) \
    do { asm volatile("msettn x0, %0" \
                      :: "r"((uintptr_t)(val)) \
                      : "memory"); } while (0)

#define msettm(rd, val) \
    asm volatile("msettm %0, %1" \
                 : "=r"(rd) \
                 : "r"((uintptr_t)(val)) \
                 : "memory")

#define msettm_x0(val) \
    do { asm volatile("msettm x0, %0" \
                      :: "r"((uintptr_t)(val)) \
                      : "memory"); } while (0)

#define msettk(rd, val) \
    asm volatile("msettk %0, %1" \
                 : "=r"(rd) \
                 : "r"((uintptr_t)(val)) \
                 : "memory")

#define msettk_x0(val) \
    do { asm volatile("msettk x0, %0" \
                      :: "r"((uintptr_t)(val)) \
                      : "memory"); } while (0)

// =============================================================
// ZVT context-status (ZVT spec §1.10)
//
// mstatus.MS = bits [30:29]  — analogous to:
//   mstatus.VS = bits [10:9]   (MSTATUS_VS  = 0x00000600)
//   mstatus.FS = bits [14:13]  (MSTATUS_FS  = 0x00006000)
//   mstatus.MS = bits [30:29]  (MSTATUS_MS  = 0x60000000)
//
// enable pattern (set MS = Initial = 01b):
//   MSTATUS_MS & (MSTATUS_MS >> 1) = 0x60000000 & 0x30000000
//                                   = 0x20000000  (bit 29 only)
//
// Same derivation used by enable_vec() / enable_fp():
//   MSTATUS_VS & (MSTATUS_VS >> 1) = 0x200  → VS = 01
//   MSTATUS_FS & (MSTATUS_FS >> 1) = 0x2000 → FS = 01
//   MSTATUS_MS & (MSTATUS_MS >> 1) = 0x20000000 → MS = 01
//
// vtdiscard requires MS ≠ Off; all tile instructions raise
// illegal-instruction if mstatus.MS = Off.
// vtfmm also requires FS ≠ Off (implicit fcsr access).
// =============================================================

#ifndef MSTATUS_MS
#define MSTATUS_MS 0x60000000UL   // bits [30:29]
#endif

#ifdef __SPIKE__
// Activate matrix tile state: set mstatus.MS = Initial (01b)
#define enable_vme()                                                          \
  do {                                                                        \
    asm volatile("csrs mstatus, %[bits];"                                     \
                 ::[bits] "r"(MSTATUS_MS & (MSTATUS_MS >> 1)));              \
  } while (0)
#else
// crt0 activates VS and FS but not MS (ZVT is a new extension).
// Write mstatus.MS = Initial explicitly so tile instructions
// are legal and vtdiscard changes MS back to Initial correctly.
#define enable_vme()                                                          \
  do {                                                                        \
    asm volatile("csrs mstatus, %[bits];"                                     \
                 ::[bits] "r"(MSTATUS_MS & (MSTATUS_MS >> 1)));              \
  } while (0)
#endif

#endif // __VECTOR_MATRIX_MACROS_H__
