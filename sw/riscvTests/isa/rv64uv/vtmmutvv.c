#include <stdint.h>
#include "vector_macros.h"
#include "float_macros.h"   // ok to include (keeps same style as other tests)

// ------------------------------
// Zvt helpers
// ------------------------------

// TSS encoding from Zvt spec:
// bits 30:27 = tile specifier (0..15)
// bits 26:24 = pattern (0=row, 1=col; >1 reserved)
// bits 23:0  = index within pattern (row/col index)
static inline uint64_t ZVT_TSS_ROW(uint32_t tile, uint32_t row_idx) {
  return ((uint64_t)tile << 27) | ((uint64_t)0 << 24) | (uint64_t)row_idx;
}

// Configure matrix engine for unsigned int8 multiplicands with TEW=32 accumulate.
// Zvt spec: TEW = SEW * TWIDEN; choose SEW=8 and TWIDEN=4 => TEW=32.

static inline void zvt_cfg_u8_tew32(uint64_t tn, uint64_t tm, uint64_t tk) {
  asm volatile(
    // mtypei=3 -> mtwiden=3 -> TWIDEN=4; vsewi=0 -> SEW=8 (vsew[1:0]=0).
    "msetmtypei 3, 0\n\t"

    // msettn sets tn/vl (bounded by configuration); tn is derived from vl.
    "mv a0, %[tn]\n\t"
    "msettn x0, a0\n\t"

    // msettm sets tm, msettk sets tk.
    "mv a0, %[tm]\n\t"
    "msettm x0, a0\n\t"
    "mv a0, %[tk]\n\t"
    "msettk x0, a0\n\t"
    :
    : [tn] "r"(tn), [tm] "r"(tm), [tk] "r"(tk)
    : "a0", "memory"
  );
}

// Configure matrix engine for TEW=32 transfers (move tile rows into vectors).
// Choose SEW=32 and TWIDEN=1 so TEW remains 32.
static inline void zvt_cfg_tew32_for_move(uint64_t tn, uint64_t tm) {
  asm volatile(
    // mtypei=1 -> mtwiden=1 -> TWIDEN=1; vsewi=2 -> SEW=32. TEW=32.
    "msetmtypei 1, 2\n\t"
    "mv a0, %[tn]\n\t"
    "msettn x0, a0\n\t"
    "mv a0, %[tm]\n\t"
    "msettm x0, a0\n\t"
    :
    : [tn] "r"(tn), [tm] "r"(tm)
    : "a0", "memory"
  );
}

// ------------------------------
// Test case
// ------------------------------
void TEST_CASE1(void) {
  // Pick small safe dimensions TE>=4; use tn=tm=tk=4.
  // Zvt spec constrains TE such that TE >= 4 and TE is power-of-2.
  const uint64_t tn = 4;
  const uint64_t tm = 4;
  const uint64_t tk = 4;

  // Step 1: Use VSET only to make VLOAD_8 macros happy for loading bytes.
  // Then immediately configure Zvt again (vset* instructions write zero to mtype per spec).
  VSET(16, e8, m1);

  // Prepare A (tk x tm = 4x4) and B (tk x tn = 4x4).
  //
  // We build B as Identity so result C becomes A^T:
  //   C[tm,tn] += A^T * I  => C = A^T
  //
  // Register layout: for KMAX=4, successive rows are separated by (8/KMAX)=2 vector regs.
  // A rows at v8, v10, v12, v14 (base = v8)
  // B rows at v16, v18, v20, v22 (base = v16)

  // A row0 = [ 1,  2,  3,  4]
  VLOAD_8(v8,  1, 2, 3, 4,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  // A row1 = [ 5,  6,  7,  8]
  VLOAD_8(v10, 5, 6, 7, 8,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  // A row2 = [ 9, 10, 11, 12]
  VLOAD_8(v12, 9,10,11,12,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  // A row3 = [13, 14, 15, 16]
  VLOAD_8(v14,13,14,15,16,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  // B = Identity (4x4), in row-major rows:
  VLOAD_8(v16, 1,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v18, 0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v20, 0,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0);
  VLOAD_8(v22, 0,0,0,1,  0,0,0,0,  0,0,0,0,  0,0,0,0);

  // Step 2: Configure Zvt for vtmmu (SEW=8, TWIDEN=4 -> TEW=32).
  zvt_cfg_u8_tew32(tn, tm, tk);

  // Clear destination tile and perform unsigned integer matmul accumulate:
  // vtmmu.tvv accumulates into matrix tile state.
  asm volatile("vtzero mt0");
  asm volatile("vtmmu.tvv mt0, v8, v16");

  // Step 3: Reconfigure for TEW=32 transfers (SEW=32, TWIDEN=1).
  // Then extract rows of mt0 using TSS + vtmv.v.t.
  zvt_cfg_tew32_for_move(tn, tm);

  uint64_t tss;

  tss = ZVT_TSS_ROW(0, 0);
  asm volatile("vtmv.v.t v1, %[tss]" :: [tss] "r"(tss) : "memory");
  tss = ZVT_TSS_ROW(0, 1);
  asm volatile("vtmv.v.t v2, %[tss]" :: [tss] "r"(tss) : "memory");
  tss = ZVT_TSS_ROW(0, 2);
  asm volatile("vtmv.v.t v3, %[tss]" :: [tss] "r"(tss) : "memory");
  tss = ZVT_TSS_ROW(0, 3);
  asm volatile("vtmv.v.t v4, %[tss]" :: [tss] "r"(tss) : "memory");

  // Step 4: Compare against expected C = A^T:
  // row0: [1, 5,  9, 13]
  // row1: [2, 6, 10, 14]
  // row2: [3, 7, 11, 15]
  // row3: [4, 8, 12, 16]
  //
  // Use VSET to set vector state for comparisons (ok; after extraction we don't need mtype configured).
  VSET(4, e32, m1);
  VCMP_U32(1, v1, 0x00000001, 0x00000005, 0x00000009, 0x0000000d);
  VCMP_U32(2, v2, 0x00000002, 0x00000006, 0x0000000a, 0x0000000e);
  VCMP_U32(3, v3, 0x00000003, 0x00000007, 0x0000000b, 0x0000000f);
  VCMP_U32(4, v4, 0x00000004, 0x00000008, 0x0000000c, 0x00000010);

  // Optional: discard Zvt context after test.
  asm volatile("vtdiscard");
}

int main(void) {
  INIT_CHECK();
  enable_vec();

  TEST_CASE1();

  EXIT_CHECK();
}