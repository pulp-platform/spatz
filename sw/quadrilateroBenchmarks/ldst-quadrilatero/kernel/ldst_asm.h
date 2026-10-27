#ifndef LDST_ASM_H
#define LDST_ASM_H

#include <stdint.h>
#include "quadrilatero.h"   // __INT8__, __INT16__, __INT32__

// ---- Parameters from CMake ---------------------------------------------------
#ifndef QUAD_RLEN
  #error "QUAD_RLEN not defined"
#endif
#if !defined(SHAPE_M) || !defined(SHAPE_N)
  #error "SHAPE_M / SHAPE_N not defined"
#endif
#if SHAPE_M * SHAPE_N > 4
  #error "2x4, 4x2 and 4x4 are not supported"
#endif

#define ROWS   (QUAD_RLEN / 32)            
#define ENC(x) ((x) == 4 ? 3 : (x) - 1)    

// ---- Configuration -----------------------------------------------------------
#define SET_DT(dt) \
  asm volatile("mmac.dt %0, %1, %2" :: "i"(dt), "i"(dt), "i"(dt))
#define SET_DT32() SET_DT(__INT32__)

//Requesting 4096  (the maximum)
#define CFG(mm, mn, M, N, K)                                \
  asm volatile("mcfgm %0, %3, %4\n\t"                       \
               "mcfgn %1, %3, %5\n\t"                       \
               "mcfgk %2, %3"                               \
               : "=&r"(M), "=&r"(N), "=&r"(K)               \
               : "r"(4096), "i"(ENC(mm)), "i"(ENC(mn)))


#define CFG_REQ(mm, mn, reqM, reqN, reqK, M, N, K)          \
  asm volatile("mcfgm %0, %3, %6\n\t"                       \
               "mcfgn %1, %4, %7\n\t"                       \
               "mcfgk %2, %5"                               \
               : "=&r"(M), "=&r"(N), "=&r"(K)               \
               : "r"(reqM), "r"(reqN), "r"(reqK), "i"(ENC(mm)), "i"(ENC(mn)))

// ---- Load / store / zero (stride in bytes) -----------------------------------
#define LD_LHS(reg, src, st) asm volatile("mld.lhs " #reg ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define LD_RHS(reg, src, st) asm volatile("mld.rhs " #reg ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define ST(reg, dst, st)     asm volatile("mst "     #reg ", (%0), %1" :: "r"(dst), "r"(st) : "memory")
#define MZERO(reg)           asm volatile("mzero.m " #reg ::: "memory")   // zeroes a whole group
#define FENCE()              asm volatile("fence" ::: "memory")           

// ---- Register lists ----------------------------------------------------------
#define REGS_G1(X) X(m0) X(m1) X(m2)  X(m3)  X(m4)  X(m5)  X(m6)  X(m7) \
                   X(m8) X(m9) X(m10) X(m11) X(m12) X(m13) X(m14) X(m15)
#define REGS_G2(X) X(m0) X(m2) X(m4) X(m6) X(m8) X(m10) X(m12) X(m14)
#define REGS_G4(X) X(m0) X(m4) X(m8) X(m12)

// Zero the RF with mzero.m, in 1x1 as a function, not a macro
// used in REGS(...), preprocessor does not expand REGS_G1 again 
#define MZERO_ONE_(reg) MZERO(reg);
static inline void mzero_all(void) {
  unsigned m, n, k;
  CFG(1, 1, m, n, k);
  (void)m; (void)n; (void)k;
  REGS_G1(MZERO_ONE_)
}

// Clear the RF with 1x1 loads from a ROWS x ROWS buffer of zeros
#define LD_ZERO_ONE_(reg) LD_LHS(reg, zero, ROWS * 4);
static inline void clear_rf_by_load(const uint32_t *zero) {
  unsigned m, n, k;
  CFG(1, 1, m, n, k);
  (void)m; (void)n; (void)k;
  REGS_G1(LD_ZERO_ONE_)
}

// G: one ROWS x ROWS block of distinct words per register of the group (reg j <- block j)
#define G_BLOCK(G, j) ((G) + (j) * ROWS * ROWS)
#define LD_G(reg, j) LD_RHS(reg, G_BLOCK(G, j), ROWS * 4);

// Load m0..m(GROUP-1) one by one in 1x1 from G 
// leaves the 1x1 shape active
static inline void fill_group(const uint32_t *G) {
  unsigned m, n, k;
  CFG(1, 1, m, n, k);
  (void)m; (void)n; (void)k;
  LD_G(m0, 0)
#if SHAPE_M * SHAPE_N >= 2
  LD_G(m1, 1)
#endif
#if SHAPE_M * SHAPE_N >= 4
  LD_G(m2, 2)
  LD_G(m3, 3)
#endif
}

// Registers to test
// large configs test fewer groups
// define ALL_REGS to test them all
#define GROUP (SHAPE_M * SHAPE_N)
#if QUAD_RLEN >= 1024 && !defined(ALL_REGS)
  #define REGS(X) X(m0)
#elif QUAD_RLEN >= 256 && !defined(ALL_REGS)
  #if   GROUP == 1
    #define REGS(X) X(m0) X(m15)
  #elif GROUP == 2
    #define REGS(X) X(m0) X(m14)
  #else
    #define REGS(X) X(m0) X(m12)
  #endif
#else
  #if   GROUP == 1
    #define REGS REGS_G1
  #elif GROUP == 2
    #define REGS REGS_G2
  #else
    #define REGS REGS_G4
  #endif
#endif

#endif
