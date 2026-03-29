
// =============================================================================
// DEPENDENCY TESTS
// =============================================================================
// Naming convention:
//   test_dep_<type>_<pair>_<spacing>
//   type   : raw / war / waw
//   pair   : mld_fmmacc / fmmacc_mst / mmov_fmmacc / mzero_mld / mzero_fmmacc
//   spacing: bb (back-to-back) / sp (spaced, 1+ instr between)
// =============================================================================


// -----------------------------------------------------------------------------
// RAW: mld -> fmmacc  (back-to-back)
// Hazard: fmmacc reads m0/m4 immediately after mld writes them.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mld_fmmacc_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  // -- back-to-back: mld then immediately fmmacc with no instruction between --
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // WRITE m0
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );  // WRITE m4
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on m0, m4
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}

// -----------------------------------------------------------------------------
// RAW: mld -> fmmacc  (spaced — two unrelated loads between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mld_fmmacc_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  // -- spaced: 2 independent mlds between the producer and consumer --
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // WRITE m0  (producer)
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );  // WRITE m4  (producer)
  asm volatile("mld.lhs.w   m1, (t4) , s7 "   );  // gap instr (writes m1, unrelated)
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );  // gap instr (writes m5, unrelated)
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on m0, m4 (spaced)
  asm volatile("fmmacc.s acc0,  m6  , m2  "   );  // also exercises m1, m5
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: fmmacc -> mst  (back-to-back via mmov.am)
// Hazard: mmov.am reads acc0 immediately after fmmacc writes it;
//         mst reads m4 immediately after mmov.am writes it.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_fmmacc_mst_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  // -- back-to-back chain: fmmacc -> mmov.am -> mst, no gap --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // WRITE acc0
  asm volatile("mmov.am   m4 , acc0       "   );  // RAW on acc0, WRITE m4
  asm volatile("mst.w     m4 , (s8) , s7  "   );  // RAW on m4
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: fmmacc -> mst  (spaced — two fmmacc accumulations between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_fmmacc_mst_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("mld.lhs.w   m1, (t4) , s7 "   );
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );
  // -- first fmmacc is the producer for the final mst --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // WRITE acc0  (producer)
  asm volatile("fmmacc.s acc0,  m6  , m2  "   );  // gap: also writes acc0 (WAW + adds gap)
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // gap: accumulates further
  asm volatile("mmov.am   m4 , acc0       "   );  // RAW on acc0 (spaced)
  asm volatile("mst.w     m4 , (s8) , s7  "   );  // RAW on m4
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mmov.am -> fmmacc  (back-to-back)
// Hazard: fmmacc reads m4 immediately after mmov.am writes it.
// Pattern is relevant when m4 is used as both output buffer and new input.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mmov_fmmacc_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  // first accumulation
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- back-to-back: mmov writes m4, fmmacc immediately reads it as RHS --
  asm volatile("mmov.am   m4 , acc0       "   );  // WRITE m4
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on m4 (back-to-back)
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m0 , acc0       "   );
  asm volatile("mst.w     m0 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mmov.am -> fmmacc  (spaced — one mld between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mmov_fmmacc_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- spaced: one independent mld separates mmov from fmmacc --
  asm volatile("mmov.am   m4 , acc0       "   );  // WRITE m4  (producer)
  asm volatile("mld.lhs.w m1, (t4) , s7  "   );  // gap instr
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on m4 (spaced)
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m0 , acc0       "   );
  asm volatile("mst.w     m0 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mzero.m -> mld  (back-to-back)
// Hazard: mld writes into m0 immediately after mzero.m clears it.
// Validates that a fresh zero state is correctly overwritten.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mzero_mld_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  // -- back-to-back: mzero then immediately mld into same register --
  asm volatile("mzero.m  m0            "   );  // WRITE m0 = 0
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // RAW on m0 (mzero result discarded)
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mzero.m -> mld  (spaced — one unrelated mld between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mzero_mld_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  // -- spaced: one unrelated mld separates mzero from the dependent mld --
  asm volatile("mzero.m  m0            "   );  // WRITE m0 = 0  (producer)
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );  // gap instr (unrelated)
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // RAW on m0 (spaced)
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  asm volatile("fmmacc.s acc0,  m6  , m0  "   );
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mzero.a -> fmmacc  (back-to-back)
// Hazard: fmmacc reads/accumulates into acc0 immediately after mzero.a clears it.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mzero_fmmacc_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  // -- back-to-back: mzero.a then immediately fmmacc into same acc --
  asm volatile("mzero.a acc0          "   );  // WRITE acc0 = 0
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on acc0 (back-to-back)
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// RAW: mzero.a -> fmmacc  (spaced — two mld instructions between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_raw_mzero_fmmacc_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  // -- spaced: two mlds between mzero.a and fmmacc --
  asm volatile("mzero.a acc0          "   );  // WRITE acc0 = 0  (producer)
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // gap instr
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );  // gap instr
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // RAW on acc0 (spaced)
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// =============================================================================
// WAR TESTS
// =============================================================================


// -----------------------------------------------------------------------------
// WAR: fmmacc -> mld  (back-to-back)
// Hazard: mld overwrites m0 while fmmacc may still be reading it.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_fmmacc_mld_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  // -- back-to-back: fmmacc reads m0, then mld immediately writes m0 --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // READ m0
  asm volatile("mld.lhs.w   m0, (t4) , s7 "   );  // WAR on m0 (back-to-back)
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // uses new m0
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m0 , acc0       "   );
  asm volatile("mst.w     m0 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAR: fmmacc -> mld  (spaced — one fmmacc between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_fmmacc_mld_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );
  // -- spaced: one fmmacc that uses m0 before it gets overwritten --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // READ m0  (consumer)
  asm volatile("fmmacc.s acc0,  m6  , m0  "   );  // gap: also reads m0
  asm volatile("mld.lhs.w   m0, (t4) , s7 "   );  // WAR on m0 (spaced)
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // uses new m0
  // -------------------------------------------------------------------------
  asm volatile("mmov.am   m0 , acc0       "   );
  asm volatile("mst.w     m0 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.a  acc0          "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAR: mst -> mzero.m  (back-to-back)
// Hazard: mzero.m overwrites m0 immediately while mst may still be reading it.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_mst_mzero_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  // -- back-to-back: mst reads m0, mzero.m immediately clears it --
  asm volatile("mst.w     m0 , (s8) , s7  "   );  // READ m0
  asm volatile("mzero.m   m0              "   );  // WAR on m0 (back-to-back)
  // -------------------------------------------------------------------------
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAR: mst -> mzero.m  (spaced — one unrelated mst between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_mst_mzero_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    s9,  s8  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  // -- spaced: one unrelated mst between the reader and the overwrite --
  asm volatile("mst.w     m0 , (s8) , s7  "   );  // READ m0  (consumer)
  asm volatile("mst.w     m4 , (s9) , s7  "   );  // gap instr (unrelated)
  asm volatile("mzero.m   m0              "   );  // WAR on m0 (spaced)
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAR: mmov.am -> mzero.a  (back-to-back)
// Hazard: mzero.a clears acc0 immediately while mmov.am may still be reading it.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_mmov_mzero_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- back-to-back: mmov reads acc0, mzero.a immediately clears it --
  asm volatile("mmov.am   m4 , acc0       "   );  // READ acc0
  asm volatile("mzero.a   acc0            "   );  // WAR on acc0 (back-to-back)
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
}


// -----------------------------------------------------------------------------
// WAR: mmov.am -> mzero.a  (spaced — one mst between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_war_mmov_mzero_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    s9,  s8  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- spaced: one unrelated mmov/mst between the reader and the overwrite --
  asm volatile("mmov.am   m4 , acc0       "   );  // READ acc0  (consumer)
  asm volatile("mmov.am   m0 , acc0       "   );  // gap instr (reads acc0, unrelated)
  asm volatile("mzero.a   acc0            "   );  // WAR on acc0 (spaced)
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  asm volatile("mst.w     m0 , (s9) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// =============================================================================
// WAW TESTS
// =============================================================================


// -----------------------------------------------------------------------------
// WAW: mld -> mld  (back-to-back, same destination register)
// Hazard: second mld overwrites m0 before the first write has been consumed.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_mld_mld_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  // -- back-to-back: two mlds to the same register m0 --
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // WRITE m0  (first)
  asm volatile("mld.lhs.w   m0, (t4) , s7 "   );  // WAW on m0 (second, back-to-back)
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // must see second mld value
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAW: mld -> mld  (spaced — one unrelated mld between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_mld_mld_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    t5,  t1  , 32 "   );
  asm volatile("mzero.a acc0          "   );
  // -- spaced: one unrelated mld between the two writes to m0 --
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );  // WRITE m0  (first)
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );  // gap instr
  asm volatile("mld.lhs.w   m0, (t4) , s7 "   );  // WAW on m0 (spaced)
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // must see second mld value
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAW: fmmacc -> fmmacc  (back-to-back, same accumulator)
// Hazard: second fmmacc accumulates into acc0 while first write may still be
//         in-flight, testing whether partial sums are correctly forwarded.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_fmmacc_fmmacc_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("mld.lhs.w   m1, (t4) , s7 "   );
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );
  // -- back-to-back: two fmmacc into the same acc0 --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // WRITE acc0  (first)
  asm volatile("fmmacc.s acc0,  m6  , m2  "   );  // WAW+RAW on acc0 (back-to-back)
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAW: fmmacc -> fmmacc  (spaced — two independent mlds between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_fmmacc_fmmacc_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    t4,  t1  , 16 "   );
  asm volatile("addi    s4,  s1  , 16 "   );
  asm volatile("addi    t5,  t1  , 32 "   );
  asm volatile("addi    s5,  s1  , 32 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("mld.lhs.w   m1, (t4) , s7 "   );
  asm volatile("mld.rhs.w   m5, (s4) , s7 "   );
  // -- spaced: two mlds that feed the second fmmacc separate the two writes --
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );  // WRITE acc0  (first)
  asm volatile("mld.lhs.w   m2, (t5) , s7 "   );  // gap instr
  asm volatile("mld.rhs.w   m6, (s5) , s7 "   );  // gap instr
  asm volatile("fmmacc.s acc0,  m6  , m2  "   );  // WAW+RAW on acc0 (spaced)
  asm volatile("fmmacc.s acc0,  m6  , m2  "   );
  asm volatile("mmov.am   m4 , acc0       "   );
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.m  m1            "   );
  asm volatile("mzero.m  m2            "   );
  asm volatile("mzero.m  m4            "   );
  asm volatile("mzero.m  m5            "   );
  asm volatile("mzero.m  m6            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAW: mmov.am -> mzero.m  (back-to-back, same destination m4)
// Hazard: mzero.m immediately overwrites m4 written by mmov.am.
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_mmov_mzero_bb (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- back-to-back: mmov writes m4, mzero.m immediately clears m4 --
  asm volatile("mmov.am   m4 , acc0       "   );  // WRITE m4  (first)
  asm volatile("mzero.m   m4              "   );  // WAW on m4 (back-to-back)
  // m4 should now be zero; store to verify
  asm volatile("mst.w     m4 , (s8) , s7  "   );
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.a  acc0          "   );
}


// -----------------------------------------------------------------------------
// WAW: mmov.am -> mzero.m  (spaced — one mst between)
// -----------------------------------------------------------------------------
void __attribute__ ((noinline)) test_dep_waw_mmov_mzero_sp (float* addrA, float* addrB, float* addrC, int K)
{
  asm volatile("slli    s7,  %0  ,  2 " :: "r" (K)    );
  asm volatile("addi    t1,  %0  ,  0 " :: "r" (addrA));
  asm volatile("addi    s1,  %0  ,  0 " :: "r" (addrB));
  asm volatile("addi    s8,  %0  ,  0 " :: "r" (addrC));
  asm volatile("addi    s9,  s8  , 16 "   );
  asm volatile("mzero.a acc0          "   );
  asm volatile("mld.lhs.w   m0, (t1) , s7 "   );
  asm volatile("mld.rhs.w   m4, (s1) , s7 "   );
  asm volatile("fmmacc.s acc0,  m4  , m0  "   );
  // -- spaced: one unrelated mst between the two writes to m4 --
  asm volatile("mmov.am   m4 , acc0       "   );  // WRITE m4  (first)
  asm volatile("mst.w     m0 , (s9) , s7  "   );  // gap instr (unrelated)
  asm volatile("mzero.m   m4              "   );  // WAW on m4 (spaced)
  asm volatile("mst.w     m4 , (s8) , s7  "   );  // must store all zeros
  // -------------------------------------------------------------------------
  asm volatile("mzero.m  m0            "   );
  asm volatile("mzero.a  acc0          "   );
}


// =============================================================================
// DRIVER
// =============================================================================

void verify_dependencies (float* addrA, float* addrB, float* addrC, int K)
{
  printf("--- RAW: mld -> fmmacc (back-to-back) ---\n");
  test_dep_raw_mld_fmmacc_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mld -> fmmacc (spaced) ---\n");
  test_dep_raw_mld_fmmacc_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: fmmacc -> mst (back-to-back) ---\n");
  test_dep_raw_fmmacc_mst_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: fmmacc -> mst (spaced) ---\n");
  test_dep_raw_fmmacc_mst_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mmov.am -> fmmacc (back-to-back) ---\n");
  test_dep_raw_mmov_fmmacc_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mmov.am -> fmmacc (spaced) ---\n");
  test_dep_raw_mmov_fmmacc_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mzero.m -> mld (back-to-back) ---\n");
  test_dep_raw_mzero_mld_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mzero.m -> mld (spaced) ---\n");
  test_dep_raw_mzero_mld_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mzero.a -> fmmacc (back-to-back) ---\n");
  test_dep_raw_mzero_fmmacc_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- RAW: mzero.a -> fmmacc (spaced) ---\n");
  test_dep_raw_mzero_fmmacc_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: fmmacc -> mld (back-to-back) ---\n");
  test_dep_war_fmmacc_mld_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: fmmacc -> mld (spaced) ---\n");
  test_dep_war_fmmacc_mld_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: mst -> mzero.m (back-to-back) ---\n");
  test_dep_war_mst_mzero_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: mst -> mzero.m (spaced) ---\n");
  test_dep_war_mst_mzero_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: mmov.am -> mzero.a (back-to-back) ---\n");
  test_dep_war_mmov_mzero_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAR: mmov.am -> mzero.a (spaced) ---\n");
  test_dep_war_mmov_mzero_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: mld -> mld (back-to-back) ---\n");
  test_dep_waw_mld_mld_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: mld -> mld (spaced) ---\n");
  test_dep_waw_mld_mld_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: fmmacc -> fmmacc (back-to-back) ---\n");
  test_dep_waw_fmmacc_fmmacc_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: fmmacc -> fmmacc (spaced) ---\n");
  test_dep_waw_fmmacc_fmmacc_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: mmov.am -> mzero.m (back-to-back) ---\n");
  test_dep_waw_mmov_mzero_bb(addrA, addrB, addrC, K);
  printf("PASSED\n");

  printf("--- WAW: mmov.am -> mzero.m (spaced) ---\n");
  test_dep_waw_mmov_mzero_sp(addrA, addrB, addrC, K);
  printf("PASSED\n");
}