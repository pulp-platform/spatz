
// One binary per shape (SHAPE_M x SHAPE_N: 1x1, 1x2, 1x4, 2x1, 4x1, 2x2, from CMake),

// ---- Test Suite Summary ----------------------------------------------------------------
// 1. Basic Load/Store & Zero (per register in REGS):
//   rhs+mst, lhs+mst : mld.{rhs|lhs} + mst, compare C with source data.
//   zero+mst         : mzero.m + mst, C must be all zeros.
//
// 2. Sequential & Bug checks (post-zero behavior):
//   seq:mz->ld       : mzero.m -> load + mst. Control test: checks if mzero alone breaks loads.
//   seq:zst          : The zero-store itself; waits and asserts all zeros.
//   seq:zst->ld      : zero-store -> load + mst. Tests HW bug: load drops row 0 after a zero-store.
//
// 3. Partial Shapes (on m0):
//   part:K/<type>    : Partial K (1 row, max-1, full K) for int8/16/32 (K scales with data type).
//   part:N, part:M   : Partial N (rhs) and M (lhs) around register boundaries (int32 words).
//   part:MNK/i8      : Combined partial M, N, and K simultaneously (int8).
//
// 4. Group & Mixed-State Tests (m0 group, checks RTL hangs/scoreboard leaks):
//   mix:A            : m0 loaded, m1 zeroed -> mst. 
//   mix:B            : m0 zeroed, others loaded -> mst. Asserts [0 | data] 
//   mix:C            : mzero -> mld.lhs (writes base only) -> mst. Realistic mixed group 
//   mpart:M=<m>      : Group loaded -> mst of M rows. Tests LSU rmul decoding 
//   mpart:follow     : 16 loads after mpart. Detects stale/leaked scoreboard entries 


#include <snrt.h>
#include <stdint.h>
#include <stdio.h>
#include "printf.h"  

#include "kernel/ldst_asm.h"
#include "kernel/checks.c"
#include "kernel/model.c"

#define PAD_W  4     // extra words per memory row: stride != row width, catches stride bugs
#define SETTLE 200   // cycles to wait after clearing the RF

static int n_err = 0;
static volatile int g_err = 0;    // shared: every core returns it

static void delay(unsigned n) {
  for (unsigned i = 0; i < n; i++) asm volatile("nop");
}

int main() {
  const unsigned cid = snrt_cluster_core_idx();

  if (cid == 1) {   
    unsigned M, N, K;
    SET_DT32();                        
    CFG(SHAPE_M, SHAPE_N, M, N, K);
    printf("CFG rlen=%d shape=%dx%d M=%u N=%u K=%u\n", QUAD_RLEN, SHAPE_M, SHAPE_N, M, N, K);
    if (K != ROWS || M != SHAPE_M * ROWS || N != SHAPE_N * ROWS) {
      printf("FAILURE unexpected configuration\n");
      g_err = 1;
    } else {
      const unsigned pitch_rhs = N + PAD_W, pitch_lhs = M + PAD_W, pitch_c = N + PAD_W;
      const unsigned pitch_s   = pitch_rhs > pitch_lhs ? pitch_rhs : pitch_lhs;
      const unsigned words_s   = K * pitch_s;
      const unsigned words_c   = M * pitch_c + GUARD_WORDS;

      uint32_t *S = (uint32_t *)snrt_l1alloc(words_s * 4);       // source
      uint32_t *C = (uint32_t *)snrt_l1alloc(words_c * 4);       // written by mst
      uint32_t *X = (uint32_t *)snrt_l1alloc(words_c * 4);       // expected C (only for DUMP)
      uint32_t *Z = (uint32_t *)snrt_l1alloc(ROWS * ROWS * 4);   // zeros
      fill_src(S, words_s);
      for (unsigned i = 0; i < ROWS * ROWS; i++) Z[i] = 0;
      uint32_t *G = (uint32_t *)snrt_l1alloc(GROUP * ROWS * ROWS * 4);   // group data for mix tests
      for (unsigned i = 0; i < GROUP * ROWS * ROWS; i++) G[i] = 0x200000u + i;


#define SYNC_MAX 64
#define WAIT_C_MN(rows, cols)                                                     \
      do {                                                                        \
        unsigned _polls = wait_block_written(C, (rows), (cols), pitch_c, SYNC_MAX, 500); \
        if (_polls == SYNC_MAX)                                                   \
          printf("SYNC timeout: block still not completely written after %u polls\n", _polls); \
        else if (_polls)                                                          \
          printf("SYNC polls=%u (store still running after fence)\n", _polls);   \
      } while (0)
#define WAIT_C() WAIT_C_MN(M, N)


#define RESET_STATE()                                                             \
      clear_rf_by_load(Z);                                                        \
      CFG(SHAPE_M, SHAPE_N, M, N, K);                                             \
      delay(SETTLE)


#define RESET_STATE_MZ()                                                          \
      mzero_all();                                                                \
      CFG(SHAPE_M, SHAPE_N, M, N, K);                                             \
      delay(SETTLE)

#define LDST_TEST(LD, opname, reg, pitch_src, W)                                  \
      {                                                                           \
        unsigned bad = 0;                                                         \
        prep_C(C, words_c);                                                       \
        printf("START op=" opname " reg=" #reg "\n");                             \
        LD(reg, S, (pitch_src) * 4);                                              \
        ST(reg, C, pitch_c * 4);                                                  \
        FENCE();                                                                  \
        WAIT_C();                                                                 \
        int e = check_load(C, M, N, pitch_c, words_c, S, (pitch_src), (W), (W),   \
                           N, K, &bad);                                           \
        n_err += report(opname, #reg, e, bad, (W) / ROWS);                                 \
        if (e) dump(opname, S, (pitch_src), C, pitch_c, M, N);                    \
      }

#define ZERO_STORE(reg)                                                           \
      MZERO(reg);                                                                 \
      delay(SETTLE);                                                              \
      ST(reg, C, pitch_c * 4);                                                    \
      FENCE();                                                                    \
      WAIT_C()

#define DO_LOADS(reg)                                                             \
      RESET_STATE();                                                              \
      LDST_TEST(LD_RHS, "rhs+mst", reg, pitch_rhs, N)                             \
      RESET_STATE();                                                              \
      LDST_TEST(LD_LHS, "lhs+mst", reg, pitch_lhs, M)

#define DO_ZST(reg)                                                               \
      {                                                                           \
        unsigned bad = 0;                                                         \
        RESET_STATE();                                                            \
        prep_C(C, words_c);                                                       \
        printf("START op=zero+mst reg=" #reg "\n");                               \
        ZERO_STORE(reg);                                                          \
        n_err += report("zero+mst", #reg, check_copy(NULL, C, M, N, pitch_c, words_c, &bad), bad, 0); \
      }

#define DO_MZ(reg)                                                                \
      RESET_STATE_MZ();                                                           \
      if (SHAPE_M == 1) LDST_TEST(LD_RHS, "seq:mz->rhs", reg, pitch_rhs, N)       \
      else              LDST_TEST(LD_LHS, "seq:mz->lhs", reg, pitch_lhs, M)

#define DO_SEQ(reg)                                                               \
      RESET_STATE();                                                              \
      {                                                                           \
        unsigned bad = 0;                                                         \
        prep_C(C, words_c);                                                       \
        printf("START op=seq:zst reg=" #reg "\n");                                \
        ZERO_STORE(reg);                                                          \
        n_err += report("seq:zst", #reg, check_copy(NULL, C, M, N, pitch_c, words_c, &bad), bad, 0); \
      }                                                                           \
      RESET_STATE_MZ();                                                           \
      if (SHAPE_M == 1) LDST_TEST(LD_RHS, "seq:zst->rhs", reg, pitch_rhs, N)      \
      else              LDST_TEST(LD_LHS, "seq:zst->lhs", reg, pitch_lhs, M)

      // partial: load + mst of m0, both with partial M/N/K at data type dt 
#define DO_PART(LD, opname, dt, sz, reqM, reqN, reqK, pitch_src, W, is_lhs)       \
      {                                                                           \
        unsigned bad = 0, mm, nn, kk;                                             \
        RESET_STATE();                                                            \
        SET_DT(dt);                            /* before CFG_REQ: maxK depends on it */ \
        CFG_REQ(SHAPE_M, SHAPE_N, (reqM), (reqN), (reqK), mm, nn, kk);            \
        prep_C(C, words_c);                                                       \
        printf("START op=" opname " reg=m0 M=%u N=%u K=%u\n", mm, nn, kk);       \
        LD(m0, S, (pitch_src) * 4);                                               \
        ST(m0, C, pitch_c * 4);                                                   \
        FENCE();                                                                  \
        WAIT_C_MN(mm, nn);                                                        \
        int e = check_load(C, mm, nn, pitch_c, words_c, S, (pitch_src), (W),      \
                           (is_lhs) ? mm : nn, N, kk * (sz) / 4, &bad);           \
        n_err += report(opname, "m0", e, bad, (W) / ROWS);                        \
        if (e) dump(opname, S, (pitch_src), C, pitch_c, mm, nn);                  \
        SET_DT32();                                                               \
      }

      // Simulation time grows quickly with RLEN: at RLEN >= 1024 one case per axis
      // (as REGS does for the registers). Define ALL_REGS to run them all.
#if QUAD_RLEN >= 1024 && !defined(ALL_REGS)
  #define PART_ALL 0
#else
  #define PART_ALL 1
#endif

      // K: the only dimension that depends on the data width -> same cases at every width
#if PART_ALL
#define PART_K(dt, sz, lbl)                                                                     \
      DO_PART(LD_RHS, "part:K/" lbl, dt, sz, M, N, 4 / (sz),              pitch_rhs, N, 0) /* 1 row     */ \
      DO_PART(LD_RHS, "part:K/" lbl, dt, sz, M, N, (ROWS - 1) * 4 / (sz), pitch_rhs, N, 0) /* all but 1 */ \
      DO_PART(LD_RHS, "part:K/" lbl, dt, sz, M, N, ROWS * 4 / (sz),       pitch_rhs, N, 0) /* full K    */
#else
#define PART_K(dt, sz, lbl)                                                                     \
      DO_PART(LD_RHS, "part:K/" lbl, dt, sz, M, N, (ROWS - 1) * 4 / (sz), pitch_rhs, N, 0) /* all but 1 */
#endif

      // M and N always count 32-bit words: one data width is enough
#define PART_N(n) DO_PART(LD_RHS, "part:N", __INT32__, 4, M, (n), ROWS, pitch_rhs, N, 0)
#define PART_M(m) DO_PART(LD_LHS, "part:M", __INT32__, 4, (m), N, ROWS, pitch_lhs, M, 1)

      // ---------------- tests on every register (see REGS) ----------------
//       REGS(DO_LOADS)
// #if SHAPE_M == 1 || SHAPE_N == 1
//       REGS(DO_MZ)          // before any zero-store has run
// #endif
//       REGS(DO_ZST)
// #if SHAPE_M == 1 || SHAPE_N == 1
//       REGS(DO_SEQ)
// #endif

      // ---------------- partial M, N, K on m0 ----------------
      PART_K(__INT8__,  1, "i8")
      PART_K(__INT16__, 2, "i16")
      PART_K(__INT32__, 4, "i32")
      PART_N(N - 1)
#if PART_ALL
      PART_N(1)
#if SHAPE_N > 1
      PART_N(ROWS)         // exactly the first register of the group
      PART_N(ROWS + 1)     // one word into the second register
#endif
#endif
      PART_M(M - 1)
#if PART_ALL
      PART_M(1)
#if SHAPE_M > 1
      PART_M(ROWS)
      PART_M(ROWS + 1)
#endif
#endif
      DO_PART(LD_LHS, "part:MNK/i8", __INT8__, 1, M - 1, N - 1, 4, pitch_lhs, M, 1)

      // ------- extra tests on the group at m0
#define GROUP_ST_CHECK(opname, zero_mask, m_rows)                                 \
      {                                                                           \
        unsigned bad = 0;                                                         \
        ST(m0, C, pitch_c * 4);                                                   \
        FENCE();                                                                  \
        WAIT_C_MN((m_rows), N);                                                   \
        int e = check_group(C, (m_rows), N, pitch_c, words_c, G, (zero_mask), &bad); \
        n_err += report(opname, "m0", e, bad, GROUP);                                      \
        if (e) {                                                                  \
          build_group(X, G, (zero_mask), (m_rows), N, pitch_c, words_c);         \
          dump(opname, X, pitch_c, C, pitch_c, (m_rows), N);                      \
        }                                                                         \
      }

      // mix:B : m0 zeroed, the other registers loaded 
#define DO_MIX_B()                                                                \
      {                                                                           \
        unsigned mm, nn, kk;                                                      \
        RESET_STATE();                                                            \
        fill_group(G);                        /* leaves the 1x1 shape active */  \
        MZERO(m0);                             /* in 1x1: only m0 */              \
        delay(SETTLE);                                                            \
        CFG(SHAPE_M, SHAPE_N, mm, nn, kk);                                        \
        (void)mm; (void)nn; (void)kk;                                             \
        prep_C(C, words_c);                                                       \
        printf("START op=mix:B reg=m0\n");                                       \
        GROUP_ST_CHECK("mix:B", 1u, M)                                            \
      }

      // mpart:  full group loaded, mst of only reqM rows
#define DO_MPART(reqM)                                                            \
      {                                                                           \
        unsigned mm, nn, kk;                                                      \
        RESET_STATE();                                                            \
        fill_group(G);                                                           \
        CFG_REQ(SHAPE_M, SHAPE_N, (reqM), 4096, 4096, mm, nn, kk);                \
        (void)nn; (void)kk;                                                       \
        prep_C(C, words_c);                                                       \
        printf("START op=mpart:M=%u reg=m0 (got M=%u)\n", (unsigned)(reqM), mm);  \
        if (mm != (unsigned)(reqM)) printf("NOTE mcfgm returned M=%u\n", mm);     \
        GROUP_ST_CHECK("mpart", 0u, mm)                                           \
        printf("START op=mpart:follow reg=m0\n");                                \
        RESET_STATE();                         /* 16 loads: hang = stale entries */ \
        n_err += report("mpart:follow", "m0", 0, 0, GROUP);                                \
      }

      // mix:C: realistic mixed group: mzero of the group, lhs writes only the base
#define DO_MIX_C()                                                                \
      {                                                                           \
        RESET_STATE();                         /* test shape active */            \
        MZERO(m0);                             /* whole group zero-flagged */     \
        delay(SETTLE);                                                            \
        prep_C(C, words_c);                                                       \
        printf("START op=mix:C reg=m0\n");                                       \
        LD_LHS(m0, S, pitch_lhs * 4);          /* 1xC: writes only m0 */          \
        {                                                                         \
          unsigned bad = 0;                                                       \
          ST(m0, C, pitch_c * 4);                                                 \
          FENCE();                                                                \
          WAIT_C();                                                               \
          int e = check_load(C, M, N, pitch_c, words_c, S, pitch_lhs, M, M, N, K, &bad); \
          n_err += report("mix:C", "m0", e, bad, GROUP);                                   \
          if (e) {                                                                \
            build_expected(X, S, pitch_lhs, M, M, M, N, N, K, pitch_c, words_c);  \
            dump("mix:C", X, pitch_c, C, pitch_c, M, N);                          \
          }                                                                       \
        }                                                                         \
      }

      // mix:A: m0 loaded, m1 zeroed
#define DO_MIX_A()                                                                \
      {                                                                           \
        unsigned mm, nn, kk;                                                      \
        RESET_STATE();                                                            \
        fill_group(G);                                                           \
        MZERO(m1);                             /* in 1x1: only m1 */              \
        delay(SETTLE);                                                            \
        CFG(SHAPE_M, SHAPE_N, mm, nn, kk);                                        \
        (void)mm; (void)nn; (void)kk;                                             \
        prep_C(C, words_c);                                                       \
        printf("START op=mix:A reg=m0\n");                                       \
        GROUP_ST_CHECK("mix:A", 2u, M)                                            \
      }

// #if GROUP > 1
//       DO_MIX_B()
// #endif
//       DO_MPART(M - 1)
// #if SHAPE_M > 1
//       DO_MPART((SHAPE_M - 1) * ROWS + 1)
// #endif
// #if SHAPE_M == 4
//       DO_MPART(3 * ROWS)                       /* rmul = 2'b10 */
//       DO_MPART(2 * ROWS + 1)                   /* rmul = 2'b10 */
// #endif
// #if SHAPE_N > 1
//       DO_MIX_C()
// #endif
// #if GROUP > 1
//       DO_MIX_A()
// #endif

      printf("%s rlen=%d shape=%dx%d errors=%d\n",
             n_err ? "FAILURE" : "SUCCESS", QUAD_RLEN, SHAPE_M, SHAPE_N, n_err);
      g_err = n_err;
    }
  }

  snrt_cluster_hw_barrier();
  return g_err;
}
