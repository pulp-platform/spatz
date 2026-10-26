// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// An instruction held in decode by a stall must take effect only once.
//
// Snitch keeps an instruction in decode while the accelerator memory counter
// is full (7 FP or vector loads/stores in flight), and while it reads an FP
// CSR and Spatz is busy. The instruction stays valid in every stalled cycle,
// so anything it does per valid cycle instead of once is repeated:
//  - a register write: `addi a0, a0, 1` adds once per stalled cycle,
//  - an offload: `vadd.vi` or `fadd.s` reaches Spatz once per stalled cycle,
//  - a CSR write: `csrrw t0, frm, t1` writes frm in the first stalled cycle
//    and then returns the new value in t0.
// Each case first issues FP and vector loads to fill the counter, then checks
// the result of the instructions after them. The counter only fills for some
// mixes of loads, so the cases sweep them. The loads read the TCDM: the vector
// unit cannot reach external memory.

#include "printf.h"
#include "snrt.h"

// e32, m8 and VLEN = 512: one vector load moves 512 bytes.
#define VL 128

// Load sources in the TCDM, set up by main().
static float *src_f;
static uint32_t *src_v;

// Store target of drain().
static volatile uint32_t drain_slot;

// FP loads into ft0..ft6 and vector loads into v0, v8, v16, v24.
#define FL0 ""
#define FL1 FL0 "flw ft0, 0(%[f])\n"
#define FL2 FL1 "flw ft1, 4(%[f])\n"
#define FL3 FL2 "flw ft2, 8(%[f])\n"
#define FL4 FL3 "flw ft3, 12(%[f])\n"
#define FL5 FL4 "flw ft4, 16(%[f])\n"
#define FL6 FL5 "flw ft5, 20(%[f])\n"
#define FL7 FL6 "flw ft6, 24(%[f])\n"
#define VL0 ""
#define VL1 VL0 "vle32.v v0, (%[v])\n"
#define VL2 VL1 "vle32.v v8, (%[v])\n"
#define VL3 VL2 "vle32.v v16, (%[v])\n"
#define VL4 VL3 "vle32.v v24, (%[v])\n"

#define LOAD_INPUTS [f] "r"(src_f), [v] "r"(src_v), [n] "r"(VL)
#define CLOBBERS                                                               \
  "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft8", "v0", "v1", "v2",    \
      "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13",    \
      "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",    \
      "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "memory"

static uint32_t errors;
static uint32_t case_idx;

static void check(const char *what, uint32_t got, uint32_t expected) {
  if (got != expected) {
    printf("case %u: %s = %#x, expected %#x\n", case_idx, what, got, expected);
    errors++;
  }
}

// A scalar store waits in decode until no accelerator load/store is in
// flight, so every case starts with an empty counter.
static inline void drain(void) {
  asm volatile("sw zero, 0(%0)" ::"r"(&drain_slot) : "memory");
}

// An ALU instruction with rd == rs1 right after the loads.
#define ALU_CASE(LOADS)                                                        \
  do {                                                                         \
    uint32_t acc = 0;                                                          \
    asm volatile("vsetvli zero, %[n], e32, m8, ta, ma\n" LOADS                 \
                 ".rept 8\n"                                                   \
                 "addi %[acc], %[acc], 1\n"                                    \
                 ".endr\n"                                                     \
                 : [acc] "+r"(acc)                                             \
                 : LOAD_INPUTS                                                 \
                 : CLOBBERS);                                                  \
    drain();                                                                   \
    check("addi", acc, 8);                                                     \
    case_idx++;                                                                \
  } while (0)

// Accumulating vector offloads right after the loads. v24 is not loaded, so
// this uses at most three vector loads.
#define VADD_CASE(LOADS)                                                       \
  do {                                                                         \
    uint32_t vsum;                                                             \
    asm volatile("vsetvli zero, %[n], e32, m8, ta, ma\n"                       \
                 "vmv.v.i v24, 0\n" LOADS                                      \
                 ".rept 4\n"                                                   \
                 "vadd.vi v24, v24, 1\n"                                       \
                 ".endr\n"                                                     \
                 "vmv.x.s %[vsum], v24\n"                                      \
                 : [vsum] "=&r"(vsum)                                          \
                 : LOAD_INPUTS                                                 \
                 : CLOBBERS);                                                  \
    drain();                                                                   \
    check("vadd.vi", vsum, 4);                                                 \
    case_idx++;                                                                \
  } while (0)

// Accumulating FP offloads right after the loads.
#define FADD_CASE(LOADS)                                                       \
  do {                                                                         \
    uint32_t fsum;                                                             \
    asm volatile("vsetvli zero, %[n], e32, m8, ta, ma\n"                       \
                 "fmv.w.x ft8, zero\n" LOADS                                   \
                 ".rept 4\n"                                                   \
                 "fadd.s ft8, ft8, %[one]\n"                                   \
                 ".endr\n"                                                     \
                 "fmv.x.w %[fsum], ft8\n"                                      \
                 : [fsum] "=&r"(fsum)                                          \
                 : LOAD_INPUTS, [one] "f"(1.0f)                                \
                 : CLOBBERS);                                                  \
    drain();                                                                   \
    check("fadd.s", fsum, 0x40800000); /* 4.0f */                              \
    case_idx++;                                                                \
  } while (0)

#define ALU_BOTH(FL, VL) ALU_CASE(FL VL); ALU_CASE(VL FL)
#define VADD_BOTH(FL, VL) VADD_CASE(FL VL); VADD_CASE(VL FL)
#define FADD_BOTH(FL, VL) FADD_CASE(FL VL); FADD_CASE(VL FL)

#define FOR_VL4(M, FL) M(FL, VL0) M(FL, VL1) M(FL, VL2) M(FL, VL3) M(FL, VL4)
#define FOR_VL3(M, FL) M(FL, VL0) M(FL, VL1) M(FL, VL2) M(FL, VL3)
#define FOR_FL(M, FOR_VL)                                                      \
  FOR_VL(M, FL0) FOR_VL(M, FL1) FOR_VL(M, FL2) FOR_VL(M, FL3) FOR_VL(M, FL4)   \
      FOR_VL(M, FL5) FOR_VL(M, FL6) FOR_VL(M, FL7)

// The cases expand to separate statements.
#define STMT(FL, VL) ALU_BOTH(FL, VL);
#define VSTMT(FL, VL) VADD_BOTH(FL, VL);
#define FSTMT(FL, VL) FADD_BOTH(FL, VL);

// csrrw and csrrs on frm and fflags while vector loads keep Spatz busy. Each
// must return the old value and write the new one.
static void fcsr_case(void) {
  uint32_t old_frm, old_fflags, frm, fflags;
  asm volatile("csrwi frm, 0\n"
               "csrwi fflags, 0\n"
               "vsetvli zero, %[n], e32, m8, ta, ma\n"
               "vle32.v v0, (%[v])\n"
               "vle32.v v8, (%[v])\n"
               "csrrw %[old_frm], frm, %[new_frm]\n"
               "csrrs %[old_fflags], fflags, %[new_fflags]\n"
               "csrr %[frm], frm\n"
               "csrr %[fflags], fflags\n"
               "csrwi frm, 0\n"
               "csrwi fflags, 0\n"
               : [old_frm] "=&r"(old_frm), [old_fflags] "=&r"(old_fflags),
                 [frm] "=&r"(frm), [fflags] "=&r"(fflags)
               : LOAD_INPUTS, [new_frm] "r"(3), [new_fflags] "r"(1)
               : CLOBBERS);
  drain();
  check("csrrw frm", old_frm, 0);
  check("csrrs fflags", old_fflags, 0);
  check("frm", frm, 3);
  check("fflags", fflags, 1);
  case_idx++;
}

int main() {
  if (snrt_cluster_core_idx() != 0)
    return 0;

  src_f = (float *)snrt_l1alloc(8 * sizeof(float));
  src_v = (uint32_t *)snrt_l1alloc(VL * sizeof(uint32_t));
  for (int i = 0; i < 8; i++)
    src_f[i] = i + 1;
  for (int i = 0; i < VL; i++)
    src_v[i] = i + 1;

  FOR_FL(STMT, FOR_VL4)
  FOR_FL(VSTMT, FOR_VL3)
  FOR_FL(FSTMT, FOR_VL3)
  fcsr_case();

  if (errors)
    printf("stall-replay: %u errors in %u cases\n", errors, case_idx);
  return errors != 0;
}
