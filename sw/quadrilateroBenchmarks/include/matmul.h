
void __attribute__ ((noinline)) FUNC_NAME(void* addrA,void* addrB, void* addrC, int K, int N, int M, int widening)
{
    asm volatile(  

        // 1. Data Types Configuration
        "mmac.dt %[dt_typeC], %[dt_typeA], %[dt_typeB] \n\t"
        "mcfgm t3, %[M], %[rmul]    \n\t"
        "mcfgn t4, %[N], %[cmul]    \n\t"
        "mcfgk t5, %[K]             \n\t"   // t5 = first block depth
        "slli   s0, %[M], 2         \n\t"   // Compute strideA = M * 2^shift
        "slli   t0, %[N], 2         \n\t"   // Compute strideB = M * 2^shift
        "mld.lhs m0, (%[addrA]), s0 \n\t"
        "mld.rhs m4, (%[addrB]), t0 \n\t"
        "mzero.a acc0               \n\t"
        "mmacc   acc0, m4, m0       \n\t"
        "sub  t2, %[K], t5          \n\t"
        "srl  t5, t5, %[widening]   \n\t"
        "blez t2, 2f                \n\t"

        "mul  s8, t5, s0            \n\t"
        "mul  s9, t5, t0            \n\t"
        "mcfgk t5, t2               \n\t"
        "add  s6, %[addrA], s8      \n\t"
        "add  s7, %[addrB], s9      \n\t"
        "mld.lhs m2, (s6), s0       \n\t"
        "mld.rhs m6, (s7), t0       \n\t"
        "mmacc   acc0, m6, m2       \n\t"
        "sub  t2, t2, t5            \n\t"
        "add  t6, s8, s8            \n\t"
        "add  s4, %[addrA], t6      \n\t"
        "add  t6, s9, s9            \n\t"
        "add  s5, %[addrB], t6      \n\t"
        "blez t2, 2f                \n\t"
        "mcfgk t5, t2               \n\t"

        // =====================================================================
        // K-loop for FIRST tile of this M-row
        // Invariant at top: t5=cfgK for current block, t2=remaining K BEFORE
        //                   this block, m0/m4 not yet loaded.
        // =====================================================================
        "1: \n\t"
        // Load block A (first of pair)
        "mld.lhs m0, (s4), s0 \n\t"
        "mld.rhs m4, (s5), t0 \n\t"
        "mmacc   acc0, m4, m0         \n\t"
        // Compute stride for block A (using current t5, BEFORE updating t5)
        "sub  t2, t2, t5              \n\t"
        "srl  t5, t5, %[widening] \n\t" 
        "mul  s8, t5, s0      \n\t"
        "mul  s9, t5, t0      \n\t"
        // If no more blocks, done
        "blez t2, 2f                  \n\t"
        // Block B pointers (offset from s4/s5 by exactly one block = s8/s9)
        "add  s6, s4, s8              \n\t"
        "add  s7, s5, s9              \n\t"
        // Update cfgK for block B (t2 already has remaining after block A)
        "mcfgk t5, t2                 \n\t"
        // Load block B
        "mld.lhs m2, (s6), s0 \n\t"
        "mld.rhs m6, (s7), t0 \n\t"
        "mmacc   acc0, m6, m2         \n\t"
        // Advance A/B ptrs by TWO blocks
        // FIX: advance BEFORE mcfgk so s8/s9 still hold blockA size * stride
        "add  t6, s8, s8              \n\t"
        "add  s4, s4, t6              \n\t"
        "add  t6, s9, s9              \n\t"
        "add  s5, s5, t6              \n\t"
        // Consume block B from remaining K
        "sub  t2, t2, t5              \n\t"
        // Update cfgK for next iteration's block A
        "mcfgk t5, t2                 \n\t"
        "bgtz t2, 1b         \n\t"

        "2: \n\t"
        "mmov.am m8, acc0             \n\t"
        "mzero.a acc0                 \n\t"

        "sub  t1, %[N], t4              \n\t"
        "slli t6, t4, 2               \n\t"
        "add  s10, x0, %[addrC]       \n\t"
        "add  s2, %[addrC], t6        \n\t"
        "add  s3, %[addrB], t6        \n\t" //addrB
        "add  s11, x0, t4             \n\t"

        "blez t1, 8f      \n\t"

        // =====================================================================
        // PIPELINED N LOOP
        // =====================================================================
        "3: \n\t"

        "add  t2, x0, %[K]            \n\t"
        "mcfgk t5, t2                 \n\t"   // t5 = first block depth
        "mcfgn t4, t1, %[cmul]        \n\t"
        "add   s4, x0, %[addrA]       \n\t"
        "add   s5, x0, s3             \n\t"

        // Load first block (block A of first pair)
        "mld.lhs m0, (s4), s0 \n\t"
        "mld.rhs m4, (s5), t0 \n\t"
        // Stride for block A (using current t5, BEFORE consuming it)
        "sub  t2, t2, t5              \n\t"
        "srl  t5, t5, %[widening] \n\t" 
        "mul  s8, t5, s0      \n\t"
        "mul  s9, t5, t0      \n\t"
        "mmacc   acc0, m4, m0         \n\t"
        // Consume block A
        // Block B pointers
        "add  s6, s4, s8              \n\t"
        "add  s7, s5, s9              \n\t"

        // If only one block total: mmacc it, issue store, skip to done
        "blez t2, 4f  \n\t"

        // Load block B
        "mld.lhs m2, (s6), s0 \n\t"
        "mld.rhs m6, (s7), t0 \n\t"
        // Second mmacc
        "mmacc   acc0, m6, m2         \n\t"

        // Advance A/B ptrs by TWO blocks
        "add  t6, s8, s8              \n\t"
        "add  s4, s4, t6              \n\t"
        "add  t6, s9, s9              \n\t"
        "add  s5, s5, t6              \n\t"

        // Update cfgK for block B, consume it
        "mcfgk t5, t2                 \n\t"
        "sub  t2, t2, t5              \n\t"

        // Update cfgK for next block A
        "mcfgk t5, t2                 \n\t"
        "bgtz t2, 5f     \n\t"

        // --- Single-block K path (K fits in one mcfgk tile) ---
        "4: \n\t"
        "mcfgn s11, s11, %[cmul]      \n\t"
        "mst m8, (s10), t0    \n\t"
        "mcfgn t4, t4, %[cmul]        \n\t"
        "j    7f         \n\t"

        // =====================================================================
        // K continuation loop
        // Invariant at top: t5=cfgK for current block A, t2=remaining K BEFORE
        //                   this block A, s4/s5 point to current block A.
        // =====================================================================

        // Issue store of PREVIOUS tile (scoreboard stalls until mmov done)
        "5: \n\t"
        "mcfgn s11, s11, %[cmul]      \n\t"
        "mst m8, (s10), t0    \n\t"
        "mcfgn t4, t4, %[cmul]        \n\t"
        
        "6: \n\t"
        // Load block A
        "mld.lhs m0, (s4), s0 \n\t"
        "mld.rhs m4, (s5), t0 \n\t"
        // Stride for block A (using current t5, BEFORE consuming)
        "sub  t2, t2, t5         \n\t"
        "srl  t5, t5, %[widening] \n\t" 
        "mul  s8, t5, s0      \n\t"
        "mul  s9, t5, t0      \n\t"
        // Consume block A
        "mmacc   acc0, m4, m0         \n\t"
        // If no block B remains, exit
        "blez t2, 7f     \n\t"  // Bug 2 fixed: was branching to self
        // Block B pointers
        "add  s6, s4, s8              \n\t"
        "add  s7, s5, s9              \n\t"
        // Advance A/B ptrs by TWO blocks
        "add  t6, s8, s8              \n\t"
        "add  s4, s4, t6              \n\t"
        "add  t6, s9, s9              \n\t"
        "add  s5, s5, t6              \n\t"
        // Update cfgK for block B, consume it
        "mcfgk t5, t2                 \n\t"
        "mld.lhs m2, (s6), s0 \n\t"
        "mld.rhs m6, (s7), t0 \n\t"
        "sub  t2, t2, t5              \n\t"
        "mmacc   acc0, m6, m2         \n\t"
        // Update cfgK for next block A
        "mcfgk t5, t2                 \n\t"
        "bgtz t2, 6b     \n\t"

        "7: \n\t"
        "mmov.am m8, acc0             \n\t"
        "mzero.a acc0                 \n\t"

        "sub  t1, t1, t4              \n\t"
        "slli t6, t4, 2               \n\t"
        "add  s10, x0, s2             \n\t"
        "add  s2, s2, t6              \n\t"
        "add  s3, s3, t6              \n\t"
        "add  s11, x0, t4             \n\t"

        "bgtz t1, 3b          \n\t"

        // =====================================================================
        // EPILOGUE: end of N-loop for one M-row.
        // mmov.am m8 is in-flight (issued at 7f or 2).
        // t3  = processed M of this row (old mcfgm value)
        // s11 = processed N of last tile (old n for mst)
        // s10 = C ptr of last tile
        //
        // Strategy:
        //   If NOT last M-row:
        //     1. mst with OLD mcfgm(t3)/mcfgn(s11) — scoreboard stalls ~20c
        //     2. Advance M pointers
        //     3. NEW mcfgm/mcfgn/mcfgk + mzero.a
        //     4. Load first block-pair of next M-row + mmacc
        //        → buried in the mst stall window
        //     5. Re-enter 1b (if more K blocks remain)
        //        or 2 (if K exhausted)
        //   If last M-row:
        //     plain mst + exit
        // =====================================================================
        "8: \n\t"

        // Advance M row pointers (scalar, free while mmov burns its 20c)
        "mul  t6, t3, t0              \n\t"
        "add  %[addrC], %[addrC], t6  \n\t"
        "slli t6, t3, 2               \n\t"
        "add  %[addrA], %[addrA], t6  \n\t"
        "add  t6, x0, %[M]            \n\t" // -- My addition --
        "sub  %[M], %[M], t3          \n\t"

        // Last M-row? → plain store and exit
        "blez %[M], 10f       \n\t"

        // -----------------------------------------------------------------
        // NOT last M-row: issue mst with OLD config, then prefetch next row
        // -----------------------------------------------------------------

        // New mcfgm/mcfgn/mcfgk — mst is queued but not executed yet,
        // mcfg decodes immediately so new config is live for loads/mmacc
        "mcfgm t3, %[M], %[rmul]      \n\t"
        "mcfgn t4, %[N], %[cmul]      \n\t"
        "mcfgk t5, %[K]               \n\t"   // t5 = first block depth of new row

        // Load first block A of next M-row's first N-tile
        // Both LSUs active; mst has one locked but loads use the other,
        // and mst won't execute for ~20c (waiting for mmov)
        // Load block A (first of pair)
        "mld.lhs m0, (%[addrA]), s0 \n\t"
        "mld.rhs m4, (%[addrB]), t0 \n\t"
        // Compute stride for block A (using current t5, BEFORE updating t5)
        "sub  t2, %[K], t5        \n\t"
        "srl  t5, t5, %[widening] \n\t" 
        "mul  s8, t5, s0      \n\t"
        "mul  s9, t5, t0      \n\t"
        "mzero.a acc0         \n\t"
        "mmacc   acc0, m4, m0         \n\t"
        // If no more blocks, done
        "blez t2, 9f   \n\t"
        // Block B pointers (offset from s4/s5 by exactly one block = s8/s9)
        "add  s6, %[addrA], s8              \n\t"
        "add  s7, %[addrB], s9              \n\t"
        // Update cfgK for block B (t2 already has remaining after block A)
        "mcfgk t5, t2                 \n\t"
        // Load block B
        "mld.lhs m2, (s6), s0 \n\t"
        "mld.rhs m6, (s7), t0 \n\t"
        "mmacc   acc0, m6, m2         \n\t"
        // Consume block B from remaining K
        "sub  t2, t2, t5              \n\t"
        "mcfgn s11, s11, %[cmul]      \n\t"   // restore old n for store
        "mcfgm t6 , t6 , %[rmul]      \n\t"   // restore old n for store
        "mst   m8, (s10), t0  \n\t"   // issued now, stalls on scoreboard
        "mcfgm t3, t3, %[rmul]        \n\t"
        "mcfgn t4, t4, %[cmul]        \n\t"
        // Advance A/B ptrs by TWO blocks
        // FIX: advance BEFORE mcfgk so s8/s9 still hold blockA size * stride
        "add  t6, s8, s8              \n\t"
        "add  s4, %[addrA], t6              \n\t"
        "add  t6, s9, s9              \n\t"
        "add  s5, %[addrB], t6              \n\t"
        // Update cfgK for next iteration's block A
        "mcfgk t5, t2                 \n\t"
        "bgtz t2, 1b         \n\t"

        "9: \n\t"
        "mcfgn s11, s11, %[cmul]      \n\t"   // restore old n for store
        "mcfgm  t6,  t6, %[rmul]      \n\t"   // restore old n for store
        "mst   m8, (s10), t0  \n\t"   // issued now, stalls on scoreboard
        "mcfgm t3, t3, %[rmul]        \n\t"
        "mcfgn t4, t4, %[cmul]        \n\t"
        "j    2b                      \n\t"

        // -----------------------------------------------------------------
        // Last M-row: plain store, no prefetch
        // -----------------------------------------------------------------
        "10: \n\t"
        "mcfgn s11, s11, %[cmul]      \n\t"   // restore old n for store
        "mst   m8, (s10), t0  \n\t"

        : [addrA] "+r" (addrA), [addrB] "+r" (addrB), [addrC] "+r" (addrC),
          [M] "+r" (M), [N] "+r" (N), [K] "+r" (K)
        : [dt_typeC] "i" (DTC), [dt_typeA] "i" (DTA), [dt_typeB] "i" (DTB),
          [rmul] "i" (RMUL_2), [cmul] "i" (CMUL_2), [widening] "r" (widening)
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "memory"
    );
}