// Copyright 2026 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Bowen Wang, ETH Zurich
//
// Test for custom post-increment load instructions:
//   RVV:
//     P_VLE8_V_RRPOST
//     P_VLE16_V_RRPOST
//     P_VLE32_V_RRPOST
//     P_VLE64_V_RRPOST
//   Scalar FP:
//     P_FLB_RRPOST
//     P_FLH_RRPOST
//     P_FLW_RRPOST
//     P_FLD_RRPOST
//
// For each test, we compare against the corresponding standard load and check:
//   1) payload equality
//   2) post-increment pointer update: rs1_after == rs1_before + rs2
//
// Notes:
// - Raw .word encodings are used (no compiler changes required).
// - RVV custom instructions use the former VLE128/256/512/1024 slots.
// - Scalar FP custom instructions use a custom-0 opcode R-type-like encoding.
// - rs1 is hard-bound to t3, rs2 (increment) is hard-bound to t2.

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>


#define RVX_T2   7u   // x7  = t2 (increment)
#define RVX_T3   28u  // x28 = t3 (base pointer)

// FP register indices used in raw scalar FP RRPOST encodings
#define FREG_F1  1u

// ---------------------------
// Scalar FP P_FL*_RRPOST encs
// ---------------------------
// Custom-1 opcode (0x2b), R-type-like:
//   [31:25] funct7 = 0b1010000
//   [24:20] rs2    = increment GPR
//   [19:15] rs1    = base pointer GPR
//   [14:12] funct3 = width (FLB/FLH/FLW/FLD -> 000/001/010/011)
//   [11:7]  rd     = FP destination register index
//   [6:0]   opcode = 0x2b
#define _P_FL_RRPOST_ENC(frd, rs2, rs1, width3)                                           \
  (((uint32_t)0x50u << 25) |                                                               \
   ((uint32_t)((rs2)   & 0x1Fu) << 20) |                                                   \
   ((uint32_t)((rs1)   & 0x1Fu) << 15) |                                                   \
   ((uint32_t)((width3) & 0x07u) << 12) |                                                  \
   ((uint32_t)((frd)   & 0x1Fu) << 7)  |                                                   \
   (uint32_t)0x2Bu)

#define P_FLB_RRPOST(frd, rs2, rs1)  _P_FL_RRPOST_ENC((frd), (rs2), (rs1), 0u)

// Use a reasonably long buffer so we can request longer VLs safely.
#define BUF_BYTES   512u
#define REQ_VL      16u

// allocate extra space so we can align to 8B for FLD safely
#define ALLOC_BYTES (BUF_BYTES + 16u)

static uint8_t *src_bytes_raw;
static uint8_t *out_ref_bytes_raw;
static uint8_t *out_post_bytes_raw;

static uint8_t *src_bytes;
static uint8_t *out_ref_bytes;
static uint8_t *out_post_bytes;

static inline uintptr_t align_up_uintptr(uintptr_t x, uintptr_t a) {
  return (x + (a - 1u)) & ~(a - 1u);
}

static void clear_out_buffers(void) {
  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }
}

static void init_buffers(void) {
  src_bytes_raw      = (uint8_t *)snrt_l1alloc(ALLOC_BYTES);
  out_ref_bytes_raw  = (uint8_t *)snrt_l1alloc(ALLOC_BYTES);
  out_post_bytes_raw = (uint8_t *)snrt_l1alloc(ALLOC_BYTES);

  src_bytes      = (uint8_t *)align_up_uintptr((uintptr_t)src_bytes_raw, 8u);
  out_ref_bytes  = (uint8_t *)align_up_uintptr((uintptr_t)out_ref_bytes_raw, 8u);
  out_post_bytes = (uint8_t *)align_up_uintptr((uintptr_t)out_post_bytes_raw, 8u);

  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    src_bytes[i]      = (uint8_t)((0x40u + i) & 0xffu);
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }
}

// -------------------
// RVV post-inc tests
// -------------------

static int test_vle8_rrpost(void) {
  uintptr_t vl = 0;
  uintptr_t req_vl = REQ_VL;
  uint8_t *addr_ori = src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = REQ_VL * sizeof(uint8_t);

  asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(req_vl));

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle8.v v8, (%0)" :: "r"(addr_ori) : "memory");
  // asm volatile(".word %2"
  //              : "+r"(addr_p_reg)
  //              : "r"(inc_reg), "i"(P_VLE8_V_RRPOST(RVV_V16, RVX_T2, RVX_T3, 1))
  //              : "memory");
  asm volatile("p.vle8.v.rrpost v16, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("vse8.v v8, (%0)"  :: "r"(out_ref_bytes)  : "memory");
  asm volatile("vse8.v v16, (%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  for (uintptr_t i = 0; i < vl; i++) {
    if (out_ref_bytes[i] != out_post_bytes[i]) {
      printf("E8  mismatch lane %u: ref=0x%02x post=0x%02x\n",
             (unsigned)i, out_ref_bytes[i], out_post_bytes[i]);
      errors++;
    }
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("E8  addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[E8 ] vl=%lu inc=%lu bytes -> %s\n",
         (unsigned long)vl, (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_vle16_rrpost(void) {
  uintptr_t vl = 0;
  uintptr_t req_vl = REQ_VL;
  uint16_t *addr_ori = (uint16_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = REQ_VL * sizeof(uint16_t);

  asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(req_vl));

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle16.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile("p.vle16.v.rrpost v16, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("vse16.v v8, (%0)"  :: "r"(out_ref_bytes)  : "memory");
  asm volatile("vse16.v v16, (%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint16_t *ref = (uint16_t *)out_ref_bytes;
  uint16_t *pst = (uint16_t *)out_post_bytes;
  for (uintptr_t i = 0; i < vl; i++) {
    if (ref[i] != pst[i]) {
      printf("E16 mismatch lane %u: ref=0x%04x post=0x%04x\n",
             (unsigned)i, ref[i], pst[i]);
      errors++;
    }
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("E16 addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[E16] vl=%lu inc=%lu bytes -> %s\n",
         (unsigned long)vl, (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_vle32_rrpost(void) {
  uintptr_t vl = 0;
  uintptr_t req_vl = REQ_VL;
  uint32_t *addr_ori = (uint32_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = REQ_VL * sizeof(uint32_t);

  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(req_vl));

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle32.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile("p.vle32.v.rrpost v16, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("vse32.v v8, (%0)"  :: "r"(out_ref_bytes)  : "memory");
  asm volatile("vse32.v v16, (%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint32_t *ref = (uint32_t *)out_ref_bytes;
  uint32_t *pst = (uint32_t *)out_post_bytes;
  for (uintptr_t i = 0; i < vl; i++) {
    if (ref[i] != pst[i]) {
      printf("E32 mismatch lane %u: ref=0x%08x post=0x%08x\n",
             (unsigned)i, ref[i], pst[i]);
      errors++;
    }
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("E32 addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[E32] vl=%lu inc=%lu bytes -> %s\n",
         (unsigned long)vl, (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_vle64_rrpost(void) {
  uintptr_t vl = 0;
  uintptr_t req_vl = REQ_VL;
  uint64_t *addr_ori = (uint64_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = REQ_VL * sizeof(uint64_t);

  asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(req_vl));

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle64.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile("p.vle64.v.rrpost v16, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("vse64.v v8, (%0)"  :: "r"(out_ref_bytes)  : "memory");
  asm volatile("vse64.v v16, (%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint64_t *ref = (uint64_t *)out_ref_bytes;
  uint64_t *pst = (uint64_t *)out_post_bytes;
  for (uintptr_t i = 0; i < vl; i++) {
    if (ref[i] != pst[i]) {
      printf("E64 mismatch lane %u: ref=0x%016llx post=0x%016llx\n",
             (unsigned)i, (unsigned long long)ref[i], (unsigned long long)pst[i]);
      errors++;
    }
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("E64 addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[E64] vl=%lu inc=%lu bytes -> %s\n",
         (unsigned long)vl, (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

// -----------------------
// Scalar FP RRPOST tests
// -----------------------

static int test_flb_rrpost(void) {
  uint8_t *addr_ori = src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = sizeof(uint8_t);

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  // Reference and custom loads into f0/f1, then store back
  asm volatile("flb f0, 0(%0)" :: "r"(addr_ori) : "memory");
  asm volatile(".word %2"
               : "+r"(addr_p_reg)
               : "r"(inc_reg), "i"(P_FLB_RRPOST(FREG_F1, RVX_T2, RVX_T3))
               : "memory");
  asm volatile("fsb f0, 0(%0)" :: "r"(out_ref_bytes) : "memory");
  asm volatile("fsb f1, 0(%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  if (out_ref_bytes[0] != out_post_bytes[0]) {
    printf("FLB payload mismatch: ref=0x%02x post=0x%02x\n",
           out_ref_bytes[0], out_post_bytes[0]);
    errors++;
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("FLB addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[FLB] inc=%lu bytes -> %s\n",
         (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_flh_rrpost(void) {
  uint16_t *addr_ori = (uint16_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = sizeof(uint16_t);

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("flh f0, 0(%0)" :: "r"(addr_ori) : "memory");
  asm volatile("pflh.rrpost f1, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("fsh f0, 0(%0)" :: "r"(out_ref_bytes) : "memory");
  asm volatile("fsh f1, 0(%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint16_t ref = *(uint16_t *)out_ref_bytes;
  uint16_t pst = *(uint16_t *)out_post_bytes;
  if (ref != pst) {
    printf("FLH payload mismatch: ref=0x%04x post=0x%04x\n", ref, pst);
    errors++;
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("FLH addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[FLH] inc=%lu bytes -> %s\n",
         (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_flw_rrpost(void) {
  uint32_t *addr_ori = (uint32_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = sizeof(uint32_t);

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("flw f0, 0(%0)" :: "r"(addr_ori) : "memory");
  asm volatile("pflw.rrpost f1, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("fsw f0, 0(%0)" :: "r"(out_ref_bytes) : "memory");
  asm volatile("fsw f1, 0(%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint32_t ref = *(uint32_t *)out_ref_bytes;
  uint32_t pst = *(uint32_t *)out_post_bytes;
  if (ref != pst) {
    printf("FLW payload mismatch: ref=0x%08x post=0x%08x\n", ref, pst);
    errors++;
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("FLW addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[FLW] inc=%lu bytes -> %s\n",
         (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

static int test_fld_rrpost(void) {
  uint64_t *addr_ori = (uint64_t *)src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = sizeof(uint64_t);

  clear_out_buffers();

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("fld f0, 0(%0)" :: "r"(addr_ori) : "memory");
  asm volatile("pfld.rrpost f1, (%0), %1" : "+r"(addr_p_reg) : "r"(inc_reg) : "memory");
  asm volatile("fsd f0, 0(%0)" :: "r"(out_ref_bytes) : "memory");
  asm volatile("fsd f1, 0(%0)" :: "r"(out_post_bytes) : "memory");

  int errors = 0;
  uint64_t ref = *(uint64_t *)out_ref_bytes;
  uint64_t pst = *(uint64_t *)out_post_bytes;
  if (ref != pst) {
    printf("FLD payload mismatch: ref=0x%016llx post=0x%016llx\n",
           (unsigned long long)ref, (unsigned long long)pst);
    errors++;
  }

  uintptr_t expected = addr_ori_u + inc_bytes;
  if (addr_p_reg != expected) {
    printf("FLD addr mismatch: got=0x%08lx expected=0x%08lx\n",
           (unsigned long)addr_p_reg, (unsigned long)expected);
    errors++;
  }

  printf("[FLD] inc=%lu bytes -> %s\n",
         (unsigned long)inc_bytes, errors ? "FAIL" : "PASS");
  return errors;
}

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid == 0) {
    init_buffers();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    int total_errors = 0;

    printf("\n===== Post-Increment Load Tests =====\n");
    printf("Requested RVV VL = %u\n", (unsigned)REQ_VL);

    printf("\n-- RVV unit-stride post-inc loads --\n");
    total_errors += test_vle8_rrpost();
    total_errors += test_vle16_rrpost();
    total_errors += test_vle32_rrpost();
    total_errors += test_vle64_rrpost();

    printf("\n-- Scalar FP post-inc loads --\n");
    total_errors += test_flb_rrpost();
    total_errors += test_flh_rrpost();
    total_errors += test_flw_rrpost();
    total_errors += test_fld_rrpost();

    if (total_errors == 0) {
      printf("\nALL PASS\n");
    } else {
      printf("\nTOTAL FAILURES: %d\n", total_errors);
    }
  }

  snrt_cluster_hw_barrier();
  return 0;
}