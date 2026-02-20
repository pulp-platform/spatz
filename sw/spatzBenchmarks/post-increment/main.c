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
// Test for custom post-increment RVV load instructions:
//   P_VLE8_V_RRPOST
//   P_VLE16_V_RRPOST
//   P_VLE32_V_RRPOST
//   P_VLE64_V_RRPOST
//
// For each width, the test does:
//   1) v8  <- vleX.v      (addr_ori)
//   2) v16 <- p.vleX.rrpost(addr_p, inc)
//   3) checks:
//      - vector contents in v8 and v16 are identical
//      - addr_p == addr_ori + inc
//
// Note:
// - Raw .word encodings are used (no compiler changes required).
// - Custom instructions use the former VLE128/256/512/1024 slots.
// - rs1 is hard-bound to t3, rs2 (increment) is hard-bound to t2.

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdint.h>
#include <stdio.h>

#define RVV_V8   8u
#define RVV_V16 16u

#define RVX_T2   7u   // x7  = t2 (increment)
#define RVX_T3   28u  // x28 = t3 (base pointer)

// Common custom load encoding template:
//   bits [31:29] = 3'b000
//   bit  [28]    = 1'b1   (custom slot in former VLE128/256/512/1024 space)
//   bits [27:26] = 2'b00
//   bit  [25]    = vm
//   bits [24:20] = rs2 (post-increment register)
//   bits [19:15] = rs1 (base pointer register)
//   bits [14:12] = width code
//   bits [11:7]  = vd
//   bits [6:0]   = 0x07 (vector load opcode)

#define _P_VLE_RRPOST_ENC(vd, rs2, rs1, vm, width3)                                      \
  (((uint32_t)0u << 29) | ((uint32_t)1u << 28) | ((uint32_t)0u << 27) |                  \
   ((uint32_t)0u << 26) | ((uint32_t)((vm)   & 0x01u) << 25) |                           \
   ((uint32_t)((rs2)  & 0x1Fu) << 20) | ((uint32_t)((rs1)  & 0x1Fu) << 15) |             \
   ((uint32_t)((width3) & 0x07u) << 12) | ((uint32_t)((vd)   & 0x1Fu) << 7) |            \
   (uint32_t)0x07u)

#define P_VLE8_V_RRPOST(vd, rs2, rs1, vm)   _P_VLE_RRPOST_ENC((vd), (rs2), (rs1), (vm), 0u)
#define P_VLE16_V_RRPOST(vd, rs2, rs1, vm)  _P_VLE_RRPOST_ENC((vd), (rs2), (rs1), (vm), 5u)
#define P_VLE32_V_RRPOST(vd, rs2, rs1, vm)  _P_VLE_RRPOST_ENC((vd), (rs2), (rs1), (vm), 6u)
#define P_VLE64_V_RRPOST(vd, rs2, rs1, vm)  _P_VLE_RRPOST_ENC((vd), (rs2), (rs1), (vm), 7u)

// Use a reasonably long buffer so we can request longer VLs safely.
#define BUF_BYTES   512u
#define REQ_VL      16u

static uint8_t *src_bytes;
static uint8_t *out_ref_bytes;
static uint8_t *out_post_bytes;

static void init_buffers(void) {
  src_bytes      = (uint8_t *)snrt_l1alloc(BUF_BYTES);
  out_ref_bytes  = (uint8_t *)snrt_l1alloc(BUF_BYTES);
  out_post_bytes = (uint8_t *)snrt_l1alloc(BUF_BYTES);

  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    src_bytes[i]      = (uint8_t)((0x40u + i) & 0xffu);
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }
}

static int test_vle8_rrpost(void) {
  uintptr_t vl = 0;
  uintptr_t req_vl = REQ_VL;
  uint8_t *addr_ori = src_bytes;
  uintptr_t addr_ori_u = (uintptr_t)addr_ori;
  uintptr_t inc_bytes = REQ_VL * sizeof(uint8_t);  // exact byte increment

  asm volatile("vsetvli %0, %1, e8, m1, ta, ma"
               : "=r"(vl)
               : "r"(req_vl));

  // Clear outputs
  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle8.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile(".word %2"
               : "+r"(addr_p_reg)
               : "r"(inc_reg),
                 "i"(P_VLE8_V_RRPOST(RVV_V16, RVX_T2, RVX_T3, 1))
               : "memory");
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

  asm volatile("vsetvli %0, %1, e16, m1, ta, ma"
               : "=r"(vl)
               : "r"(req_vl));

  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle16.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile(".word %2"
               : "+r"(addr_p_reg)
               : "r"(inc_reg),
                 "i"(P_VLE16_V_RRPOST(RVV_V16, RVX_T2, RVX_T3, 1))
               : "memory");
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

  asm volatile("vsetvli %0, %1, e32, m1, ta, ma"
               : "=r"(vl)
               : "r"(req_vl));

  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle32.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile(".word %2"
               : "+r"(addr_p_reg)
               : "r"(inc_reg),
                 "i"(P_VLE32_V_RRPOST(RVV_V16, RVX_T2, RVX_T3, 1))
               : "memory");
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

  asm volatile("vsetvli %0, %1, e64, m1, ta, ma"
               : "=r"(vl)
               : "r"(req_vl));

  for (uint32_t i = 0; i < BUF_BYTES; i++) {
    out_ref_bytes[i]  = (uint8_t)0xA5u;
    out_post_bytes[i] = (uint8_t)0x5Au;
  }

  register uintptr_t addr_p_reg asm("t3") = (uintptr_t)addr_ori;
  register uintptr_t inc_reg    asm("t2") = (uintptr_t)inc_bytes;

  asm volatile("vle64.v v8, (%0)" :: "r"(addr_ori) : "memory");
  asm volatile(".word %2"
               : "+r"(addr_p_reg)
               : "r"(inc_reg),
                 "i"(P_VLE64_V_RRPOST(RVV_V16, RVX_T2, RVX_T3, 1))
               : "memory");
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

int main() {
  const unsigned int cid = snrt_cluster_core_idx();

  if (cid == 0) {
    init_buffers();
  }

  snrt_cluster_hw_barrier();

  if (cid == 0) {
    int total_errors = 0;

    printf("\n===== RVV Post-Increment Load Tests =====\n");
    printf("Requested VL = %u\n", (unsigned)REQ_VL);

    total_errors += test_vle8_rrpost();
    total_errors += test_vle16_rrpost();
    total_errors += test_vle32_rrpost();
    total_errors += test_vle64_rrpost();

    if (total_errors == 0) {
      printf("ALL PASS\n");
    } else {
      printf("TOTAL FAILURES: %d\n", total_errors);
    }
  }

  snrt_cluster_hw_barrier();
  return 0;
}