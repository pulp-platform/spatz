#include "checks.h"

void fill_src(uint32_t *S, unsigned n_words) {
  for (unsigned i = 0; i < n_words; i++) S[i] = 0x1000u + i;
}

void prep_C(uint32_t *C, unsigned n_words) {
  for (unsigned i = 0; i < n_words; i++) C[i] = SENTINEL;
}

//only for zero-store tests
int check_copy(const uint32_t *exp, const uint32_t *C, unsigned rows, unsigned cols,
               unsigned pitch_w, unsigned total_words, unsigned *bad_idx) {
  unsigned i = 0;
  for (unsigned r = 0; i < total_words; r++)
  for (unsigned c = 0; c < pitch_w && i < total_words; c++, i++) {
    if (r < rows && c < cols) {
      uint32_t e = exp ? exp[i] : 0u;
      if (C[i] != e) {
        *bad_idx = i;
        return (C[i] == SENTINEL) ? ERR_UNWRITTEN : ERR_MISMATCH;
      }
    } else if (C[i] != SENTINEL) {
      *bad_idx = i;
      return ERR_OVERFLOW;
    }
  }
  return ERR_NONE;
}

unsigned wait_block_written(const uint32_t *C, unsigned rows, unsigned cols,
                            unsigned pitch_w, unsigned max_iter, unsigned pause) {
  const volatile uint32_t *V = (const volatile uint32_t *)C;
  for (unsigned it = 0; it < max_iter; it++) {
    int pending = 0;
    for (unsigned r = 0; r < rows && !pending; r++)
      for (unsigned c = 0; c < cols; c++)
        if (V[r * pitch_w + c] == SENTINEL) { pending = 1; break; }
    if (!pending) return it;
    for (unsigned i = 0; i < pause; i++) asm volatile("nop" ::: "memory");
  }
  return max_iter;
}

// returns 1 if the test failed
int report(const char *op, const char *reg, int e, unsigned idx, unsigned loaded) {
  printf("RES rlen=%d shape=%dx%d op=%s reg=%s %s err=%d idx=%u load=%u/%u\n",
         QUAD_RLEN, SHAPE_M, SHAPE_N, op, reg, e ? "FAILURE" : "OK", e, idx, loaded, GROUP);
  return e != 0;
}

// Only onn failure
void dump(const char *op, const uint32_t *src, unsigned pitch_src,
          const uint32_t *C, unsigned pitch_c, unsigned rows, unsigned cols) {
  unsigned r_max = rows < 4 ? rows : 4, c_max = cols < 8 ? cols : 8;
  for (unsigned r = 0; r < r_max; r++) {
    printf("DUMP %s row %u  src:", op, r);
    for (unsigned c = 0; c < c_max; c++) printf(" %x", src[r * pitch_src + c]);
    printf("   C:");
    for (unsigned c = 0; c < c_max; c++) printf(" %x", C[r * pitch_c + c]);
    printf("\n");
  }
}
