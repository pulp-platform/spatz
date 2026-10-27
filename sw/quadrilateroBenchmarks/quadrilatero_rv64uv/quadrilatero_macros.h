// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Macros for the quadrilatero ISA tests (isa/quadrilatero), as vector_macros.h.
// Only core 1 is connected to quadrilatero: the other cores wait at a barrier.
// Memory layout (as the matmul kernels): mld.lhs reads A transposed (K x M), mld.rhs reads
// B (K x N), mst writes C (M x N); elements narrower than 32 bit are packed along K.

#ifndef __QUADRILATERO_MACROS_H__
#define __QUADRILATERO_MACROS_H__

#include <printf.h>
#include <snrt.h>
#include <stdint.h>

/**************
 *  Counters  *
 **************/

// Counter for how many tests have failed
int num_failed;
// Pointer to the current test case
int test_case;
// Rows of one matrix register (RLEN/32) and its log2
unsigned QROWS, QRSH;

// L1 buffers: stored results (qc), expected values (qe), large cases (qa, qb)
uint32_t *qa, *qb, *qc, *qe;
#define QBUF_WORDS (8 * QROWS * QROWS + 16 * QROWS)

/****************
 *  Constants   *
 ****************/

// Size of the small cases: M = N = K = QS
#define QS 4

// Data types (mmac.dt immediates): {is_mx, is_altfmt, is_float, datawidth[1:0]}
#define DT_INT8    0x00
#define DT_INT16   0x01
#define DT_INT32   0x02
#define DT_FP8     0x04 // E5M2
#define DT_FP16    0x05
#define DT_FP32    0x06
#define DT_UINT8   0x08
#define DT_UINT16  0x09
#define DT_UINT32  0x0A
#define DT_FP8ALT  0x0C // E4M3
#define DT_FP16ALT 0x0D // bfloat16
#define DT_FP32ALT 0x0E

// rmul / cmul immediates of mcfgm / mcfgn
#define MUL_1 0
#define MUL_2 1
#define MUL_4 3

// Request larger than any maximum: mcfg* returns the maximum
#define QMAX 4096

// Value of the words a store must not write
#define QSENTINEL 0xDEADBEEFu

/*********************************
 *  Instruction wrappers (asm)   *
 *********************************/

// Configuration: rd = value actually set
#define MCFGK(rd, k)                                                           \
  asm volatile("mcfgk %0, %1" : "=r"(rd) : "r"(k))
#define MCFGM(rd, m, rmul)                                                     \
  asm volatile("mcfgm %0, %1, %2" : "=r"(rd) : "r"(m), "i"(rmul))
#define MCFGN(rd, n, cmul)                                                     \
  asm volatile("mcfgn %0, %1, %2" : "=r"(rd) : "r"(n), "i"(cmul))
#define MMAC_DT(dtc, dta, dtb)                                                 \
  asm volatile("mmac.dt %0, %1, %2" ::"i"(dtc), "i"(dta), "i"(dtb))

// Zero
#define MZERO_M(md) asm volatile("mzero.m " #md ::: "memory")
#define MZERO_A(acc) asm volatile("mzero.a " #acc ::: "memory")

// Load / store (stride in bytes)
#define MLD_LHS(md, addr, stride)                                              \
  asm volatile("mld.lhs " #md ", (%0), %1" ::"r"(addr), "r"(stride) : "memory")
#define MLD_RHS(md, addr, stride)                                              \
  asm volatile("mld.rhs " #md ", (%0), %1" ::"r"(addr), "r"(stride) : "memory")
#define MST(ms, addr, stride)                                                  \
  asm volatile("mst " #ms ", (%0), %1" ::"r"(addr), "r"(stride) : "memory")

// Move / multiply-accumulate
#define MMOV_AM(md, acc) asm volatile("mmov.am " #md ", " #acc ::: "memory")
#define MMACC(acc, rhs, lhs)                                                   \
  asm volatile("mmacc " #acc ", " #rhs ", " #lhs ::: "memory")

// Shape rmul x cmul, M, N, K requested; values set in QM, QN, QK
#define QCFG3(rmul, cmul, m, n, k)                                             \
  do {                                                                         \
    MCFGM(QM, m, rmul);                                                        \
    MCFGN(QN, n, cmul);                                                        \
    MCFGK(QK, k);                                                              \
  } while (0)

// Before reading memory written by mst: fence does not wait for quadrilatero stores
// (the XIF decoder answers loadstore = 0), so QFENCE also waits QUAD_ST_WAIT cycles
#ifndef QUAD_ST_WAIT
#define QUAD_ST_WAIT 1000
#endif
#define QFENCE()                                                               \
  do {                                                                         \
    asm volatile("fence" ::: "memory");                                        \
    for (unsigned _w = 0; _w < QUAD_ST_WAIT; _w++)                             \
      asm volatile("nop");                                                     \
  } while (0)

/*************
 *  Shapes   *
 *************/

#define QSHAPE_1X1 0
#define QSHAPE_1X2 1
#define QSHAPE_2X1 2
#define QSHAPE_1X4 3
#define QSHAPE_4X1 4
#define QSHAPE_2X2 5


// Current configuration, set by QSET
unsigned QM, QN, QK, QD;

/****************
 *  Data types  *
 ****************/

// Type of A and B (C is INT32 / UINT32 / FP32)
#define QNDTYPES 12
static const unsigned QDT[QNDTYPES] = {DT_INT8,  DT_INT16,   DT_INT32,  DT_UINT8,
                                       DT_UINT16, DT_UINT32, DT_FP8,    DT_FP8ALT,
                                       DT_FP16,  DT_FP16ALT, DT_FP32,   DT_FP32ALT};
#define QDT_INT8        0 // indices in QDT
#define QDT_INT16       1
#define QDT_INT32       2
#define QDT_UINT8       3
#define QDT_UINT16      4
#define QDT_UINT32      5
#define QDT_FP8         6
#define QDT_FP8ALT      7
#define QDT_FP16        8
#define QDT_BF16        9
#define QDT_FP32        10
#define QDT_FP32ALT     11
#define QDT_SIZE(d)     (1u << (QDT[d] & 3)) // bytes per element
#define QDT_FLOAT(d)    ((QDT[d] & 4) != 0)
#define QDT_UNSIGNED(d) ((QDT[d] & 0xC) == 0x8)

// mmac.dt with A = B = data type d
static inline void qset_dt(unsigned d) {
  switch (d) {
  case 0:  MMAC_DT(DT_INT32,  DT_INT8,    DT_INT8);    break;
  case 1:  MMAC_DT(DT_INT32,  DT_INT16,   DT_INT16);   break;
  case 2:  MMAC_DT(DT_INT32,  DT_INT32,   DT_INT32);   break;
  case 3:  MMAC_DT(DT_UINT32, DT_UINT8,   DT_UINT8);   break;
  case 4:  MMAC_DT(DT_UINT32, DT_UINT16,  DT_UINT16);  break;
  case 5:  MMAC_DT(DT_UINT32, DT_UINT32,  DT_UINT32);  break;
  case 6:  MMAC_DT(DT_FP32,   DT_FP8,     DT_FP8);     break;
  case 7:  MMAC_DT(DT_FP32,   DT_FP8ALT,  DT_FP8ALT);  break;
  case 8:  MMAC_DT(DT_FP32,   DT_FP16,    DT_FP16);    break;
  case 9:  MMAC_DT(DT_FP32,   DT_FP16ALT, DT_FP16ALT); break;
  case 10: MMAC_DT(DT_FP32,   DT_FP32,    DT_FP32);    break;
  default: MMAC_DT(DT_FP32,   DT_FP32ALT, DT_FP32ALT); break;
  }
}

// Shape s, data type d (index in QDT), M, N, K requested (QMAX: maximum)
static inline void qset(unsigned s, unsigned d, unsigned m, unsigned n, unsigned k) {
  qset_dt(d);
  QD = d;
  switch (s) {
  case 0:  QCFG3(MUL_1, MUL_1, m, n, k); break;
  case 1:  QCFG3(MUL_1, MUL_2, m, n, k); break;
  case 2:  QCFG3(MUL_2, MUL_1, m, n, k); break;
  case 3:  QCFG3(MUL_1, MUL_4, m, n, k); break;
  case 4:  QCFG3(MUL_4, MUL_1, m, n, k); break;
  default: QCFG3(MUL_2, MUL_2, m, n, k); break;
  }
}
#define QSET(s, d, m, n, k) qset((s), (d), (m), (n), (k))

// Bits of the integer v in a float format (eb exponent, mb mantissa bits)
static inline uint32_t qenc_fp(int32_t v, unsigned eb, unsigned mb) {
  if (v == 0)
    return 0;
  uint32_t sgn = v < 0;
  uint32_t a = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
  unsigned e = 0;
  while ((a >> (e + 1)) != 0)
    e++;
  uint32_t frac = a - (1u << e); // bits below the leading one
  uint32_t man = e >= mb ? frac >> (e - mb) : frac << (mb - e);
  uint32_t exp = e + (1u << (eb - 1)) - 1;
  return (sgn << (eb + mb)) | (exp << mb) | man;
}

// Element bits of the integer v in data type d
static inline uint32_t qenc(unsigned d, int32_t v) {
  switch (QDT[d]) {
  case DT_FP8:     return qenc_fp(v, 5, 2);
  case DT_FP8ALT:  return qenc_fp(v, 4, 3);
  case DT_FP16:    return qenc_fp(v, 5, 10);
  case DT_FP16ALT: return qenc_fp(v, 8, 7);
  case DT_FP32:
  case DT_FP32ALT: return qenc_fp(v, 8, 23);
  default:         return (uint32_t)v; // two's complement, truncated by qput
  }
}

// Word of C for the integer result v
static inline uint32_t qres(unsigned d, int32_t v) {
  return QDT_FLOAT(d) ? qenc_fp(v, 8, 23) : (uint32_t)v;
}

/*************
 *  Helpers  *
 *************/

static inline void qfill(uint32_t *buf, unsigned words, uint32_t val) {
  for (unsigned i = 0; i < words; i++)
    buf[i] = val;
}

// Distinct words: base + i
static inline void qfill_seq(uint32_t *buf, unsigned words, uint32_t base) {
  for (unsigned i = 0; i < words; i++)
    buf[i] = base + i;
}

// Element k of column col in a lhs/rhs buffer of W words per row
static inline void qput(uint32_t *buf, unsigned W, unsigned d, unsigned k, unsigned col,
                        int32_t v) {
  unsigned sz = QDT_SIZE(d), epw = 4 / sz;
  uint8_t *p = (uint8_t *)buf + ((k / epw) * W + col) * 4 + (k % epw) * sz;
  uint32_t e = qenc(d, v);
  for (unsigned i = 0; i < sz; i++)
    p[i] = (uint8_t)(e >> (8 * i));
}

// Memory rows of a lhs/rhs buffer holding K elements of type d
#define QROWS_K(d, K) (((K) * QDT_SIZE(d) + 3) / 4)

// New L1 buffer of n words (as VLOAD, one per load)
#define QALLOC(n) ((uint32_t *)snrt_l1alloc((n) * sizeof(uint32_t)))

// ptr = new L1 buffer with the listed 32-bit words
#define QDATA_32(ptr, ...)                                                     \
  static const uint32_t ptr##_init[] = {__VA_ARGS__};                          \
  uint32_t *ptr = QALLOC(sizeof(ptr##_init) / 4);                              \
  for (unsigned _i = 0; _i < sizeof(ptr##_init) / 4; _i++)                     \
    ptr[_i] = ptr##_init[_i];                                                  \
  asm volatile("fence" ::: "memory")

// Load the listed 32-bit words into md
#define MLOAD_RHS_32(md, ...)                                                  \
  do {                                                                         \
    QDATA_32(_src, __VA_ARGS__);                                               \
    MLD_RHS(md, _src, QN * 4);                                                 \
  } while (0)

// Load A (QM x QK, row-major) in data type QD into md
#define MLOAD_A(md, ...)                                                       \
  do {                                                                         \
    static const int32_t _a[] = {__VA_ARGS__};                                 \
    uint32_t *_p = QALLOC(QROWS_K(QD, QK) * QM);                               \
    qfill(_p, QROWS_K(QD, QK) * QM, 0);                                        \
    for (unsigned _m = 0; _m < QM; _m++)                                       \
      for (unsigned _k = 0; _k < QK; _k++)                                     \
        qput(_p, QM, QD, _k, _m, _a[_m * QK + _k]);                            \
    asm volatile("fence" ::: "memory");                                        \
    MLD_LHS(md, _p, QM * 4);                                                   \
  } while (0)

// Load B (QK x QN, row-major) in data type QD into md
#define MLOAD_B(md, ...)                                                       \
  do {                                                                         \
    static const int32_t _b[] = {__VA_ARGS__};                                 \
    uint32_t *_p = QALLOC(QROWS_K(QD, QK) * QN);                               \
    qfill(_p, QROWS_K(QD, QK) * QN, 0);                                        \
    for (unsigned _k = 0; _k < QK; _k++)                                       \
      for (unsigned _n = 0; _n < QN; _n++)                                     \
        qput(_p, QN, QD, _k, _n, _b[_k * QN + _n]);                            \
    asm volatile("fence" ::: "memory");                                        \
    MLD_RHS(md, _p, QN * 4);                                                   \
  } while (0)

// Reset the accumulators
#define MCLEAR_ACC() MZERO_A(acc0)

// Large cases: load m0 .. m(n-1) in 1x1, register j <- block j of G
static inline void qload_regs(unsigned n, const uint32_t *G) {
  unsigned R = QROWS;
  qset(QSHAPE_1X1, QDT_INT32, QMAX, QMAX, QMAX);
  asm volatile("fence" ::: "memory");
  switch (n) {
  case 8: MLD_RHS(m7, G + 7 * R * R, R * 4); /* fall through */
  case 7: MLD_RHS(m6, G + 6 * R * R, R * 4); /* fall through */
  case 6: MLD_RHS(m5, G + 5 * R * R, R * 4); /* fall through */
  case 5: MLD_RHS(m4, G + 4 * R * R, R * 4); /* fall through */
  case 4: MLD_RHS(m3, G + 3 * R * R, R * 4); /* fall through */
  case 3: MLD_RHS(m2, G + 2 * R * R, R * 4); /* fall through */
  case 2: MLD_RHS(m1, G + 1 * R * R, R * 4); /* fall through */
  case 1: MLD_RHS(m0, G + 0 * R * R, R * 4); /* fall through */
  default: break;
  }
}

// Large cases: word (r, c) stored from a group holding blocks base.. of G
#define QDIVR(x) ((x) >> QRSH)
#define QMODR(x) ((x) & (QROWS - 1))
#define QGROUP_WORD(G, base, C, r, c)                                          \
  ((G)[(((base) + QDIVR(r) * (C) + QDIVR(c)) << (2 * QRSH)) +                 \
       (QMODR(r) << QRSH) + QMODR(c)])

/******************
 *  Init / exit   *
 ******************/

// Only core 1 runs the tests; the other cores wait for it
#define INIT_CHECK()                                                           \
  if (snrt_cluster_core_idx() != 1) {                                          \
    snrt_cluster_hw_barrier();                                                 \
    return num_failed;                                                         \
  }                                                                            \
  num_failed = 0;                                                              \
  test_case = 0;                                                               \
  MMAC_DT(DT_INT32, DT_INT32, DT_INT32);                                       \
  MCFGK(QROWS, QMAX);                                                          \
  for (QRSH = 0; (1u << QRSH) < QROWS; QRSH++)                                 \
    ;                                                                          \
  qa = (uint32_t *)snrt_l1alloc(QBUF_WORDS * sizeof(uint32_t));                \
  qb = (uint32_t *)snrt_l1alloc(QBUF_WORDS * sizeof(uint32_t));                \
  qc = (uint32_t *)snrt_l1alloc(QBUF_WORDS * sizeof(uint32_t));                \
  qe = (uint32_t *)snrt_l1alloc(QBUF_WORDS * sizeof(uint32_t));

// Check at the end of the execution whether all the tests passed or not.
// Returns the number of failed tests (only here "PASS" is printed).
#define EXIT_CHECK()                                                           \
  snrt_cluster_hw_barrier();                                                   \
  if (num_failed > 0) {                                                        \
    printf("FAILURE: %s failed %d tests!\n", __FILE__, num_failed);            \
    return num_failed;                                                         \
  } else {                                                                     \
    printf("PASSED: %s!\n", __FILE__);                                         \
    return 0;                                                                  \
  }

/************
 *  Checks  *
 ************/

// Check the result against a scalar golden value
#define QXCMP(casenum, act, exp)                                               \
  if ((act) != (exp)) {                                                        \
    printf("[TC %d] FAILURE. Got %u, expected %u.\n", casenum,                 \
           (unsigned)(act), (unsigned)(exp));                                  \
    num_failed++;                                                              \
    return;                                                                    \
  }                                                                            \
  printf("[TC %d] OK.\n", casenum);

// Check words of memory against exp_ptr
#define QCMP_INT32(casenum, actual_ptr, exp_ptr, words)                        \
  do {                                                                         \
    volatile int32_t *_act = (volatile int32_t *)(actual_ptr);                 \
    const int32_t *_exp = (const int32_t *)(exp_ptr);                          \
    for (unsigned _i = 0; _i < (unsigned)(words); _i++) {                      \
      if (_act[_i] != _exp[_i]) {                                              \
        printf("[TC %d] FAILURE. Word %u: got 0x%08x, expected 0x%08x.\n",     \
               casenum, _i, (unsigned)_act[_i], (unsigned)_exp[_i]);           \
        num_failed++;                                                          \
        return;                                                                \
      }                                                                        \
    }                                                                          \
    printf("[TC %d] OK.\n", casenum);                                      \
  } while (0)

// Check memory against the listed 32-bit words
#define QCMP_32(casenum, actual_ptr, ...)                                      \
  do {                                                                         \
    static const uint32_t _e[] = {__VA_ARGS__};                                \
    QCMP_INT32(casenum, actual_ptr, _e, sizeof(_e) / 4);                       \
  } while (0)

// Store ms and check it against exp_ptr
#define MCMP_PTR(casenum, ms, exp_ptr)                                         \
  do {                                                                         \
    qfill(qc, QM * QN, QSENTINEL);                                             \
    asm volatile("fence" ::: "memory");                                        \
    MST(ms, qc, QN * 4);                                                       \
    QFENCE();                                                                  \
    QCMP_INT32(casenum, qc, exp_ptr, QM * QN);                                 \
  } while (0)

// Check ms against the listed 32-bit words
#define MCMP_32(casenum, ms, ...)                                              \
  do {                                                                         \
    static const uint32_t _e32[] = {__VA_ARGS__};                              \
    MCMP_PTR(casenum, ms, _e32);                                               \
  } while (0)

// Check ms against the listed results (C of data type QD)
#define MCMP_C(casenum, ms, ...)                                               \
  do {                                                                         \
    static const int32_t _ec[] = {__VA_ARGS__};                                \
    for (unsigned _i = 0; _i < sizeof(_ec) / 4; _i++)                          \
      qe[_i] = qres(QD, _ec[_i]);                                              \
    MCMP_PTR(casenum, ms, qe);                                                 \
  } while (0)

// Check the accumulators (moved to m12) against the listed results
#define MCMP_ACC(casenum, ...)                                                 \
  do {                                                                         \
    MMOV_AM(m12, acc0);                                                        \
    MCMP_C(casenum, m12, __VA_ARGS__);                                         \
  } while (0)

#endif // __QUADRILATERO_MACROS_H__
