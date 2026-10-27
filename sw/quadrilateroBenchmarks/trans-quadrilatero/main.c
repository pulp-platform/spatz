// Transposed loads/store (mldt.lhs, mldt.rhs, mstt) against the normal ones (mld.lhs, mld.rhs, mst)
// on one tile of shape SHAPE_M x SHAPE_N (from CMake): M = SHAPE_M*L, N = SHAPE_N*L, K = 4L int8,
// L = RLEN/32. Only loads and stores.
//
// Isolated (IDLE_CYC idle cycles between two instructions, no TCDM traffic):
//   trans:  mldt.lhs A, mldt.rhs Bt (data not checked); mldt.rhs S -> m8.., mstt -> Ct;   check Ct == S
//   normal: mld.lhs Ap, mld.rhs Bp  (data not checked); mld.rhs S2 -> m12.., mst -> C;    check C == S2
// Noise (back to back, no idle):
//   N1: lhs + rhs load together (both LSU ports)
//   N2: store right after its loads;                                             check Ct2 == S, C2 == S2
//
// no explicit latency measurement, deduced from the waveforms

#include <snrt.h>
#include <stdint.h>
#include "printf.h"
#include "quadrilatero.h"   // __INT8__, __INT32__

#if !((SHAPE_M == 1 || SHAPE_M == 2 || SHAPE_M == 4) && (SHAPE_N == 1 || SHAPE_N == 2 || SHAPE_N == 4) && SHAPE_M * SHAPE_N <= 4)
  #error "shapes: 1x1, 1x2, 2x1, 2x2, 1x4, 4x1"
#endif
#define ENC(x) ((x) == 4 ? 3 : (x) - 1)    // shape 1/2/4 -> immediate of mcfgm/mcfgn

#define LANES (QUAD_RLEN / 32)             // lanes = rows of a register
#ifndef IDLE_CYC
  #define IDLE_CYC 3000
#endif

// Loads/stores by mnemonic (md = matrix register number)
#define STR(x) #x
#define XSTR(x) STR(x)
#define LDT_LHS(md, src, st) asm volatile("mldt.lhs m" XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define LDT_RHS(md, src, st) asm volatile("mldt.rhs m" XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define STT(md, dst, st)     asm volatile("mstt m"     XSTR(md) ", (%0), %1" :: "r"(dst), "r"(st) : "memory")
#define MLD_LHS(md, src, st) asm volatile("mld.lhs m"  XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define MLD_RHS(md, src, st) asm volatile("mld.rhs m"  XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define MST(md, dst, st)     asm volatile("mst m"      XSTR(md) ", (%0), %1" :: "r"(dst), "r"(st) : "memory")

#define IDLE  idle(IDLE_CYC);
#define NOGAP

// Fill the SHAPE_M row groups of the store registers (m8.. trans / m12.. normal);
// P = pitch in words, GAP = IDLE or NOGAP after every load
#if SHAPE_M == 1
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP
  #define FILL_N(P, GAP) MLD_RHS(12, S2, (P) * 4); GAP
#elif SHAPE_M == 2 && SHAPE_N == 1
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(9, S + LANES, (P) * 4); GAP
  #define FILL_N(P, GAP) MLD_RHS(12, S2, (P) * 4); GAP MLD_RHS(13, S2 + LANES * (P), (P) * 4); GAP
#elif SHAPE_M == 2 && SHAPE_N == 2
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(10, S + LANES, (P) * 4); GAP
  #define FILL_N(P, GAP) MLD_RHS(12, S2, (P) * 4); GAP MLD_RHS(14, S2 + LANES * (P), (P) * 4); GAP
#else  /* 4x1 */
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(9, S + LANES, (P) * 4); GAP \
                         LDT_RHS(10, S + 2 * LANES, (P) * 4); GAP LDT_RHS(11, S + 3 * LANES, (P) * 4); GAP
  #define FILL_N(P, GAP) MLD_RHS(12, S2, (P) * 4); GAP MLD_RHS(13, S2 + LANES * (P), (P) * 4); GAP \
                         MLD_RHS(14, S2 + 2 * LANES * (P), (P) * 4); GAP MLD_RHS(15, S2 + 3 * LANES * (P), (P) * 4); GAP
#endif

static volatile int g_err = 0;

// Busy wait of n cycles using only registers and mcycle: no load/store, no TCDM traffic
static inline void idle(uint32_t n) {
  uint32_t t0, t;
  asm volatile("csrr %0, mcycle      \n\t"
               "1: csrr %1, mcycle   \n\t"
               "sub  %1, %1, %0      \n\t"
               "bltu %1, %2, 1b      \n\t"
               : "=&r"(t0), "=&r"(t) : "r"(n) : "memory");
}

static int check_eq(const char *name, const uint32_t *X, const uint32_t *ref, unsigned words) {
  int err = 0;
  for (unsigned i = 0; i < words; i++)
    if (X[i] != ref[i]) {
      if (err < 8) printf("  %s[%u] = 0x%08x exp 0x%08x\n", name, i, X[i], ref[i]);
      err++;
    }
  printf("RES %s %s errors=%d\n", name, err ? "FAILURE" : "OK", err);
  return err;
}

int main() {
  const unsigned cid = snrt_cluster_core_idx();

  if (cid == 1) {   // the core attached to quadrilatero
    unsigned M, N, K;
    asm volatile("mmac.dt %0, %1, %2" :: "i"(__INT32__), "i"(__INT8__), "i"(__INT8__));  // C, A, B
    asm volatile("mcfgm %0, %3, %4\n\t"
                 "mcfgn %1, %3, %5\n\t"
                 "mcfgk %2, %3"
                 : "=&r"(M), "=&r"(N), "=&r"(K)
                 : "r"(4096), "i"(ENC(SHAPE_M)), "i"(ENC(SHAPE_N)));   // 4096 = maximum
    printf("CFG rlen=%d shape=%dx%d M=%u N=%u K=%u IDLE_CYC=%d\n",
           QUAD_RLEN, SHAPE_M, SHAPE_N, M, N, K, IDLE_CYC);

    if (M != SHAPE_M * LANES || N != SHAPE_N * LANES || K != 4 * LANES) {
      printf("FAILURE unexpected configuration\n");
      g_err = 1;
    } else {
      // All pitches in words (LANES words = K int8).
      // The benchmark loads (A, Bt, Ap, Bp) do not check data, so they read from S
      // (N*M words >= M*LANES and >= N*LANES): separate buffers overflow the 128 KiB L1 at
      // RLEN=1024 for 1x4 / 4x1.
      uint32_t *S   = (uint32_t *)snrt_l1alloc(N * M * 4);   // source of the trans round trip (N x M)
      uint32_t *A   = S;                                     // M x K int8, row-major
      uint32_t *Bt  = S;                                     // N x K int8, row-major
      uint32_t *Ap  = S;                                     // K x M, mld layout
      uint32_t *Bp  = S;                                     // K x N, mld layout
      uint32_t *Ct  = (uint32_t *)snrt_l1alloc(N * M * 4);
      uint32_t *S2  = (uint32_t *)snrt_l1alloc(M * N * 4);   // source of the normal round trip (M x N)
      uint32_t *C   = (uint32_t *)snrt_l1alloc(M * N * 4);
      uint32_t *Ct2 = (uint32_t *)snrt_l1alloc(N * M * 4);   // outputs of the noise round trip
      uint32_t *C2  = (uint32_t *)snrt_l1alloc(M * N * 4);

      for (unsigned i = 0; i < N * M; i++) {
        S[i]  = i + 1;       // every word different and != 0 (= memory never written)
        S2[i] = 2 * i + 1;
      }
      idle(IDLE_CYC);

      // ---- Part 1: isolated, transposed ----
      LDT_LHS(0, A,  LANES * 4);   IDLE      // benchmark
      LDT_RHS(4, Bt, LANES * 4);   IDLE      // benchmark
      FILL_T(M, IDLE)                        // row groups of the store
      STT(8, Ct, M * 4);           IDLE

      // ---- Part 1: isolated, normal ----
      MLD_LHS(0, Ap, M * 4);       IDLE      // benchmark
      MLD_RHS(4, Bp, N * 4);       IDLE      // benchmark
      FILL_N(N, IDLE)
      MST(12, C, N * 4);           IDLE

      // ---- Part 2: noise, back to back, no idle inside a group ----
      LDT_LHS(0, A,  LANES * 4);             // N1 trans
      LDT_RHS(4, Bt, LANES * 4);   IDLE
      FILL_T(M, NOGAP)                       // N2 trans
      STT(8, Ct2, M * 4);          IDLE
      MLD_LHS(0, Ap, M * 4);                 // N1 normal
      MLD_RHS(4, Bp, N * 4);       IDLE
      FILL_N(N, NOGAP)                       // N2 normal
      MST(12, C2, N * 4);          IDLE

      int n_err = check_eq("Ct=S", Ct, S, N * M) + check_eq("C=S2", C, S2, M * N)
                + check_eq("Ct2=S", Ct2, S, N * M) + check_eq("C2=S2", C2, S2, M * N);
      printf("%s rlen=%d trans %dx%d errors=%d\n", n_err ? "FAILURE" : "SUCCESS", QUAD_RLEN, SHAPE_M, SHAPE_N, n_err);
      g_err = n_err;
    }
  }

  snrt_cluster_hw_barrier();
  return g_err;
}