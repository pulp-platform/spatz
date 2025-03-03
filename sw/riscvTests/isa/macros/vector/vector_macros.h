// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

#ifndef __VECTOR_MACROS_H__
#define __VECTOR_MACROS_H__

#include "dataset.h"
#include "encoding.h"
#include <stdint.h>
#include <string.h>

#ifdef __SPIKE__
#include <stdio.h>

// We need to activate the FP and V extensions manually
#define enable_vec()                                                           \
  do {                                                                         \
    asm volatile("csrs mstatus, %[bits];" ::[bits] "r"(MSTATUS_VS &            \
                                                       (MSTATUS_VS >> 1)));    \
  } while (0);
#define enable_fp()                                                            \
  do {                                                                         \
    asm volatile("csrs mstatus, %[bits];" ::[bits] "r"(MSTATUS_FS &            \
                                                       (MSTATUS_FS >> 1)));    \
  } while (0);
#else
#include <printf.h>
#include <snrt.h>

// The FP and V extensions are activated in the crt0 script
#define enable_vec()
#define enable_fp()
#endif

/**************
 *  Counters  *
 **************/

// Counter for how many tests have failed
int num_failed;
// Pointer to the current test case
int test_case;

/************
 *  Macros  *
 ************/

#define read_vtype(buf)                                                        \
  do {                                                                         \
    asm volatile("csrr %[BUF], vtype" : [BUF] "=r"(buf));                      \
  } while (0);
#define read_vl(buf)                                                           \
  do {                                                                         \
    asm volatile("csrr %[BUF], vl" : [BUF] "=r"(buf));                         \
  } while (0);

#define vtype(golden_vtype, vlmul, vsew, vta, vma)                             \
  (golden_vtype = vlmul << 0 | vsew << 3 | vta << 6 | vma << 7)

#define check_vtype_vl(casenum, vtype, golden_vtype, avl, vl)                  \
  printf("Checking vtype and vl #%d...\n", casenum);                           \
  if (vtype != golden_vtype || avl != vl) {                                    \
    printf("FAILED. Got vtype = %lx, expected vtype = %lx. avl = %lx, vl = "   \
           "%lx.\n",                                                           \
           vtype, golden_vtype, avl, vl);                                      \
    num_failed++;                                                              \
    return;                                                                    \
  }                                                                            \
  printf("PASSED.\n");

// Zero-initialized variables can be problematic on bare-metal.
// Therefore, initialize them during runtime.
#ifdef __SPIKE__
#define INIT_CHECK()                                                           \
  num_failed = 0;                                                              \
  test_case = 0;
#else
#define INIT_CHECK()                                                           \
  if (snrt_cluster_core_idx() != 0) {                                          \
    snrt_cluster_hw_barrier();                                                 \
  }                                                                            \
  num_failed = 0;                                                              \
  test_case = 0;                                                               \
  Au64 = (uint64_t *)snrt_l1alloc(32 * sizeof(uint64_t));                      \
  Au32 = (uint32_t *)snrt_l1alloc(32 * sizeof(uint32_t));                      \
  Au16 = (uint16_t *)snrt_l1alloc(32 * sizeof(uint16_t));                      \
  Au8 = (uint8_t *)snrt_l1alloc(32 * sizeof(uint8_t));                         \
  Ai64 = (int64_t *)snrt_l1alloc(32 * sizeof(int64_t));                        \
  Ai32 = (int32_t *)snrt_l1alloc(32 * sizeof(int32_t));                        \
  Ai16 = (int16_t *)snrt_l1alloc(32 * sizeof(int16_t));                        \
  Ai8 = (int8_t *)snrt_l1alloc(32 * sizeof(int8_t));                           \
  Af64 = (uint64_t *)snrt_l1alloc(32 * sizeof(uint64_t));                      \
  Af32 = (uint32_t *)snrt_l1alloc(32 * sizeof(uint32_t));                      \
  Af16 = (uint16_t *)snrt_l1alloc(32 * sizeof(uint16_t));                      \
  Ru64 = (uint64_t *)snrt_l1alloc(32 * sizeof(uint64_t));                      \
  Ru32 = (uint32_t *)snrt_l1alloc(32 * sizeof(uint32_t));                      \
  Ru16 = (uint16_t *)snrt_l1alloc(32 * sizeof(uint16_t));                      \
  Ru8 = (uint8_t *)snrt_l1alloc(32 * sizeof(uint8_t));                         \
  Ri64 = (int64_t *)snrt_l1alloc(32 * sizeof(int64_t));                        \
  Ri32 = (int32_t *)snrt_l1alloc(32 * sizeof(int32_t));                        \
  Ri16 = (int16_t *)snrt_l1alloc(32 * sizeof(int16_t));                        \
  Ri8 = (int8_t *)snrt_l1alloc(32 * sizeof(int8_t));                           \
  Rf64 = (uint64_t *)snrt_l1alloc(32 * sizeof(uint64_t));                      \
  Rf32 = (uint32_t *)snrt_l1alloc(32 * sizeof(uint32_t));                      \
  Rf16 = (uint16_t *)snrt_l1alloc(32 * sizeof(uint16_t));
#endif

// Check at the final of the execution whether all the tests passed or not.
// Returns the number of failed tests.
#define EXIT_CHECK()                                                           \
  if (num_failed > 0) {                                                        \
    printf("ERROR: %s failed %d tests!\n", __FILE__, num_failed);              \
    return num_failed;                                                         \
  } else {                                                                     \
    printf("PASSED: %s!\n", __FILE__);                                         \
    return 0;                                                                  \
  }

// Check the result against a scalar golden value
#define XCMP(casenum, act, exp)                                                \
  if (act != exp) {                                                            \
    printf("[TC %d] FAILED. Got %d, expected %d.\n", casenum, act, exp);       \
    num_failed++;                                                              \
    return;                                                                    \
  }                                                                            \
  printf("PASSED.\n");

// Check the result against a floating-point scalar golden value
#define FCMP(casenum, act, exp)                                                \
  if (act != exp) {                                                            \
    printf("[TC %d] FAILED. Got %lf, expected %lf.\n", casenum, act, exp);     \
    num_failed++;                                                              \
    return;                                                                    \
  }                                                                            \
  printf("PASSED.\n");

// Check the results against a vector of golden values
#define VCMP(T, str, casenum, vexp, act...)                                    \
  do {                                                                         \
    const T vact[] = {act};                                                    \
    for (unsigned int i = 0; i < sizeof(vact) / sizeof(T); i++) {              \
      if (vexp[i] != vact[i]) {                                                \
        printf("[TC %d] Index %d FAILED. Got " #str ", expected " #str ".\n",  \
               casenum, i, vexp[i], vact[i]);                                  \
        num_failed++;                                                          \
        return;                                                                \
      }                                                                        \
    }                                                                          \
    printf("PASSED.\n");                                                       \
  } while (0)

// Check the results against an in-memory vector of golden values
#define VMCMP(T, str, casenum, vexp, vgold, size)                              \
  do {                                                                         \
    for (unsigned int i = 0; i < size; i++) {                                  \
      if (vexp[i] != vgold[i]) {                                               \
        printf("[TC %d] Index %d FAILED. Got " #str ", expected " #str ".\n",  \
               casenum, i, vexp[i], vgold[i]);                                 \
        num_failed++;                                                          \
        return;                                                                \
      }                                                                        \
    }                                                                          \
    printf("PASSED.\n");                                                       \
  } while (0)

// Macros to set vector length, type and multiplier
#define VSET(VLEN, VTYPE, LMUL)                                                \
  do {                                                                         \
    unsigned int vl;                                                           \
    asm volatile("vsetvli %[vl], %[A]," #VTYPE "," #LMUL ", ta, ma \n"         \
                 : [vl] "+r"(vl)                                               \
                 : [A] "r"(VLEN));                                             \
  } while (0)

#define VSETMAX(VTYPE, LMUL)                                                   \
  do {                                                                         \
    unsigned int vl;                                                           \
    int64_t scalar = -1;                                                       \
    asm volatile("vsetvli %[vl], %[A]," #VTYPE "," #LMUL ", ta, ma \n"         \
                 : [vl] "+r"(vl)                                               \
                 : [A] "r"(scalar));                                           \
  } while (0)

// Macro to load a vector register with data from the stack
#define VLOAD(datatype, loadtype, vreg, vec...)                                \
  do {                                                                         \
    datatype tmpV##vreg[] = {vec};                                             \
    size_t len = sizeof(tmpV##vreg) / sizeof(datatype);                        \
    datatype *V##vreg = (datatype *)snrt_l1alloc(len * sizeof(datatype));      \
    memcpy(V##vreg, tmpV##vreg, len * sizeof(datatype));                       \
    asm volatile("vl" #loadtype ".v " #vreg ", (%0)  \n" ::[V] "r"(V##vreg));  \
  } while (0)

// Macro to store a vector register into the pointer vec
#define VSTORE(T, storetype, vreg, vec)                                        \
  do {                                                                         \
    asm volatile("vs" #storetype ".v " #vreg ", (%0)\n" : "+r"(vec));          \
  } while (0)

// Macro to reset the whole register back to zero
#define VCLEAR(register)                                                       \
  do {                                                                         \
    uint64_t vtype;                                                            \
    uint64_t vl;                                                               \
    uint64_t vlmax;                                                            \
    asm volatile("csrr %[vtype], vtype" : [vtype] "=r"(vtype));                \
    asm volatile("csrr %[vl], vl" : [vl] "=r"(vl));                            \
    asm volatile("vsetvl %[vlmax], zero, %[vtype]"                             \
                 : [vlmax] "=r"(vlmax)                                         \
                 : [vtype] "r"(vtype));                                        \
    asm volatile("vmv.v.i " #register ", 0");                                  \
    asm volatile(                                                              \
        "vsetvl zero, %[vl], %[vtype]" ::[vl] "r"(vl), [vtype] "r"(vtype));    \
  } while (0)

// Macro to initialize a vector with progressive values from a counter
#define INIT_MEM_CNT(vec_name, size)                                           \
  counter = 0;                                                                 \
  for (int i = 0; i < size; i++) {                                             \
    vec_name[i] = counter;                                                     \
    counter++;                                                                 \
  }

// Macro to initialize a vector with zeroes
// The vector is initialized on the stack, use this function with caution
// Easy to go in the UART address space
#define INIT_MEM_ZEROES(vec_name, size)                                        \
  for (int i = 0; i < size; i++) {                                             \
    vec_name[i] = 0;                                                           \
  }

/***************************
 *  Type-dependant macros  *
 ***************************/

// Vector comparison
#define VCMP_U64(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_U64(vect);                                                          \
    VCMP(uint64_t, "%lu", casenum, Ru64, act);                                 \
  }
#define VCMP_U32(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_U32(vect);                                                          \
    VCMP(uint32_t, "%u", casenum, Ru32, act);                                  \
  }
#define VCMP_U16(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_U16(vect);                                                          \
    VCMP(uint16_t, "%hu", casenum, Ru16, act);                                 \
  }
#define VCMP_U8(casenum, vect, act...)                                         \
  {                                                                            \
    VSTORE_U8(vect);                                                           \
    VCMP(uint8_t, "%hhu", casenum, Ru8, act);                                  \
  }

#define VVCMP_U64(casenum, ptr64, act...)                                      \
  { VCMP(uint64_t, "%lu", casenum, ptr64, act); }
#define VVCMP_U32(casenum, ptr32, act...)                                      \
  { VCMP(uint32_t, "%u", casenum, ptr32, act); }
#define VVCMP_U16(casenum, ptr16, act...)                                      \
  { VCMP(uint16_t, "%hu", casenum, ptr16, act); }
#define VVCMP_U8(casenum, ptr8, act...)                                        \
  { VCMP(uint8_t, "%hhu", casenum, ptr8, act); }

#define VCMP_I64(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_I64(vect);                                                          \
    VCMP(int64_t, "%ld", casenum, Ri64, act);                                  \
  }
#define VCMP_I32(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_I32(vect);                                                          \
    VCMP(int32_t, "%d", casenum, Ri32, act);                                   \
  }
#define VCMP_I16(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_I16(vect);                                                          \
    VCMP(int16_t, "%hd", casenum, Ri16, act);                                  \
  }
#define VCMP_I8(casenum, vect, act...)                                         \
  {                                                                            \
    VSTORE_I8(vect);                                                           \
    VCMP(int8_t, "%hhd", casenum, Ri8, act);                                   \
  }

#define VCMP_F64(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_F64(vect);                                                          \
    VCMP(double, "%lf", casenum, Rf64, act);                                   \
  }
#define VCMP_F32(casenum, vect, act...)                                        \
  {                                                                            \
    VSTORE_F32(vect);                                                          \
    VCMP(float, "%f", casenum, Rf32, act);                                     \
  }

// Vector load
#define VLOAD_64(vreg, vec...) VLOAD(int64_t, e64, vreg, vec)
#define VLOAD_32(vreg, vec...) VLOAD(int32_t, e32, vreg, vec)
#define VLOAD_16(vreg, vec...) VLOAD(int16_t, e16, vreg, vec)
#define VLOAD_8(vreg, vec...) VLOAD(int8_t, e8, vreg, vec)

// Vector store
#define VSTORE_U64(vreg) VSTORE(uint64_t, e64, vreg, Ru64)
#define VSTORE_U32(vreg) VSTORE(uint32_t, e32, vreg, Ru32)
#define VSTORE_U16(vreg) VSTORE(uint16_t, e16, vreg, Ru16)
#define VSTORE_U8(vreg) VSTORE(uint8_t, e8, vreg, Ru8)

#define VSTORE_I64(vreg) VSTORE(int64_t, e64, vreg, Ri64)
#define VSTORE_I32(vreg) VSTORE(int32_t, e32, vreg, Ri32)
#define VSTORE_I16(vreg) VSTORE(int16_t, e16, vreg, Ri16)
#define VSTORE_I8(vreg) VSTORE(int8_t, e8, vreg, Ri8)

#define VSTORE_F64(vreg) VSTORE(double, e64, vreg, Rf64)
#define VSTORE_F32(vreg) VSTORE(float, e32, vreg, Rf32)

#endif // __VECTOR_MACROS_H__
