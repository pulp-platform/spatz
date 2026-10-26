// Transposed loads/store (mldt.lhs, mldt.rhs, mstt) with different row pitches P (in words) on one
// tile of shape SHAPE_M x SHAPE_N (from CMake): M = SHAPE_M*L, N = SHAPE_N*L, K = 4L int8,
// L = RLEN/32. Tests the stride-dependent rotation (rot_k from stride[3:2]).
//
// For every P >= M in {L, L+1, L+2, L+3, 2L-1, 2L, 2L+1, 3L, 4L} (4L skipped with SKIP_4L):
//   isolated: mldt.lhs A, mldt.rhs Bt (data not checked); mldt.rhs S -> m8.., mstt -> Ct;   check Ct == S
//   N1: lhs + rhs load together, Bt at the base of A        (P % B == 0: same banks)
//   N1: the same with Bt shifted by L words
// "STRIDE P=<p> off=<o>" marks each block in the log.
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

// SKIP_4L: drop the pitch 4L. Default on for 1x4 (S and Ct of 128 rows x 4L words = 128 KiB at
// RLEN=1024, the whole L1). Not for 4x1: there M = 4L, so 4L is the only valid pitch (P >= M).
#ifndef SKIP_4L
  #define SKIP_4L (SHAPE_N == 4)
#endif
#define MAXP (SKIP_4L ? 3 * LANES : 4 * LANES)

// Loads/stores by mnemonic (md = matrix register number)
#define STR(x) #x
#define XSTR(x) STR(x)
#define LDT_LHS(md, src, st) asm volatile("mldt.lhs m" XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define LDT_RHS(md, src, st) asm volatile("mldt.rhs m" XSTR(md) ", (%0), %1" :: "r"(src), "r"(st) : "memory")
#define STT(md, dst, st)     asm volatile("mstt m"     XSTR(md) ", (%0), %1" :: "r"(dst), "r"(st) : "memory")

#define IDLE  idle(IDLE_CYC);
#define NOGAP

// Fill the SHAPE_M row groups of the store registers (m8..);
// P = pitch in words, GAP = IDLE or NOGAP after every load
#if SHAPE_M == 1
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP
#elif SHAPE_M == 2 && SHAPE_N == 1
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(9, S + LANES, (P) * 4); GAP
#elif SHAPE_M == 2 && SHAPE_N == 2
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(10, S + LANES, (P) * 4); GAP
#else  /* 4x1 */
  #define FILL_T(P, GAP) LDT_RHS(8, S, (P) * 4); GAP LDT_RHS(9, S + LANES, (P) * 4); GAP \
                         LDT_RHS(10, S + 2 * LANES, (P) * 4); GAP LDT_RHS(11, S + 3 * LANES, (P) * 4); GAP
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

// Ct == S on the first M words of each of the N rows of pitch P
static int check_pitch(const uint32_t *Ct, const uint32_t *S, unsigned N, unsigned M, unsigned P) {
  int err = 0;
  for (unsigned n = 0; n < N; n++)
    for (unsigned j = 0; j < M; j++)
      if (Ct[n * P + j] != S[n * P + j]) {
        if (err < 8) printf("  P=%u Ct[%u][%u] = 0x%08x exp 0x%08x\n", P, n, j, Ct[n * P + j], S[n * P + j]);
        err++;
      }
  printf("RES P=%u %s errors=%d\n", P, err ? "FAILURE" : "OK", err);
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
      // S: max(M, N) rows of pitch MAXP (mldt.lhs reads M rows, mldt.rhs N rows) + LANES words
      // for the shifted Bt;  Ct: N rows of pitch MAXP.
      // The benchmark loads (A, Bt) do not check data, so they read from S.
      const unsigned rows = (N > M ? N : M);
      uint32_t *S  = (uint32_t *)snrt_l1alloc((rows * MAXP + LANES) * 4);
      uint32_t *Ct = (uint32_t *)snrt_l1alloc(N * MAXP * 4);
      uint32_t *A  = S;
      uint32_t *Bt = S;
      for (unsigned i = 0; i < rows * MAXP + LANES; i++) S[i] = i + 1;   // every word different and != 0

      const unsigned plist[] = {LANES, LANES + 1, LANES + 2, LANES + 3, 2 * LANES - 1,
                                2 * LANES, 2 * LANES + 1, 3 * LANES, 4 * LANES};
      int n_err = 0;
      for (unsigned k = 0; k < sizeof(plist) / sizeof(plist[0]); k++) {
        const unsigned P = plist[k];
        if (P < M || P > MAXP) continue;     // the store needs rows of M words
        for (unsigned i = 0; i < N * P; i++) Ct[i] = 0;

        printf("STRIDE P=%u off=0\n", P);
        idle(IDLE_CYC);

        // ---- isolated ----
        LDT_LHS(0, A,  P * 4);       IDLE    // benchmark
        LDT_RHS(4, Bt, P * 4);       IDLE    // benchmark
        FILL_T(P, IDLE)                      // row groups of the store
        STT(8, Ct, P * 4);           IDLE

        // ---- N1, same base ----
        LDT_LHS(0, A,  P * 4);
        LDT_RHS(4, Bt, P * 4);       IDLE

        printf("STRIDE P=%u off=%u\n", P, LANES);
        idle(IDLE_CYC);

        // ---- N1, Bt shifted by LANES words ----
        LDT_LHS(0, A,          P * 4);
        LDT_RHS(4, Bt + LANES, P * 4); IDLE

        n_err += check_pitch(Ct, S, N, M, P);
      }
      printf("%s rlen=%d stride %dx%d errors=%d\n", n_err ? "FAILURE" : "SUCCESS", QUAD_RLEN, SHAPE_M, SHAPE_N, n_err);
      g_err = n_err;
    }
  }

  snrt_cluster_hw_barrier();
  return g_err;
}