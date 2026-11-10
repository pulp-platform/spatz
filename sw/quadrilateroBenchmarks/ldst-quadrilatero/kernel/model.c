// ---- Model -----------------------------------------------------------------------------
// w_part < W only for  partial M/N tests 

//for mix:C tests 
// used by build_expected 
static inline uint32_t exp_load(unsigned r, unsigned c, const uint32_t *src, unsigned pitch_src,
                                unsigned W, unsigned w_part, unsigned N, unsigned K) {
  unsigned b   = (r / ROWS) * (N / ROWS) + c / ROWS;   // register of the group (block)
  unsigned col = b * ROWS + c % ROWS;                  // word of the memory row
  if (b >= W / ROWS || r % ROWS >= K || col >= w_part) return 0u;
  return src[(r % ROWS) * pitch_src + col];
}

//  for mix:A/B mpart tests
//  zero_mask is mzeroed regs
// used by build_group
static inline uint32_t exp_group(unsigned r, unsigned c, const uint32_t *G, unsigned zero_mask,
                                 unsigned N) {
  unsigned j = (r / ROWS) * (N / ROWS) + c / ROWS;
  return (zero_mask >> j) & 1u ? 0u : G_BLOCK(G, j)[(r % ROWS) * ROWS + c % ROWS];
}

// ---- Checks -----------------------------------------------------------

static inline int word_err(uint32_t got, uint32_t want, int inside) {
  if (got == want) return ERR_NONE;
  if (!inside)     return ERR_OVERFLOW;
  return got == SENTINEL ? ERR_UNWRITTEN : ERR_MISMATCH;
}

// outside rows x cols blockC must be SENTINEL
#define CHECK_OUTSIDE(C, i, n, bad_idx)                                           \
  for (unsigned _k = 0; _k < (n); _k++, (i)++)                                    \
    if ((C)[i] != SENTINEL) { *(bad_idx) = (i); return ERR_OVERFLOW; }

// inside rows x cols block C must equal the model
#define CHECK_INSIDE(C, i, e, bad_idx)                                            \
  do {                                                                            \
    uint32_t _e = (e);                                                            \
    if ((C)[i] != _e) { *(bad_idx) = (i); return word_err((C)[i], _e, 1); }       \
    (i)++;                                                                        \
  } while (0)

// for every test except mix:A/B mpart
static int check_load(const uint32_t *C, unsigned rows, unsigned cols, unsigned pitch_c,
                      unsigned words_c, const uint32_t *src, unsigned pitch_src, unsigned W,
                      unsigned w_part, unsigned N, unsigned K, unsigned *bad_idx) {
  unsigned i = 0;
  for (unsigned r = 0; i < words_c; r++) {
    unsigned c = 0;
    if (r < rows) {
      for (; c < cols; c += ROWS) {                       // one register of the group
        unsigned b  = (r / ROWS) * (N / ROWS) + c / ROWS; // which register (block)
        unsigned n  = cols - c < ROWS ? cols - c : ROWS;  // its words inside the block
        unsigned nd = 0;                                  // of them, words with data
        if (b < W / ROWS && r % ROWS < K && w_part > b * ROWS)
          nd = w_part - b * ROWS < n ? w_part - b * ROWS : n;
        const uint32_t *s = src + (r % ROWS) * pitch_src + b * ROWS;
        for (unsigned k = 0;  k < nd; k++) CHECK_INSIDE(C, i, s[k], bad_idx);
        for (unsigned k = nd; k < n;  k++) CHECK_INSIDE(C, i, 0u,   bad_idx);
      }
      c = cols;
    }
    unsigned rest = pitch_c - c < words_c - i ? pitch_c - c : words_c - i;
    CHECK_OUTSIDE(C, i, rest, bad_idx)
  }
  return ERR_NONE;
}
// only for mix:A/B mpart tests
static int check_group(const uint32_t *C, unsigned rows, unsigned N, unsigned pitch_c,
                       unsigned words_c, const uint32_t *G, unsigned zero_mask,
                       unsigned *bad_idx) {
  unsigned i = 0;
  for (unsigned r = 0; i < words_c; r++) {
    unsigned c = 0;
    if (r < rows) {
      for (; c < N; c += ROWS) {                          // one register of the group
        unsigned j = (r / ROWS) * (N / ROWS) + c / ROWS;
        const uint32_t *g = G_BLOCK(G, j) + (r % ROWS) * ROWS;
        if ((zero_mask >> j) & 1u) for (unsigned k = 0; k < ROWS; k++) CHECK_INSIDE(C, i, 0u, bad_idx);
        else                       for (unsigned k = 0; k < ROWS; k++) CHECK_INSIDE(C, i, g[k], bad_idx);
      }
    }
    unsigned rest = pitch_c - c < words_c - i ? pitch_c - c : words_c - i;
    CHECK_OUTSIDE(C, i, rest, bad_idx)
  }
  return ERR_NONE;
}

// ---- Expected images (only for the DUMP lines of a failed test)
// only for mix:C 
static inline void build_expected(uint32_t *X, const uint32_t *src, unsigned pitch_src, unsigned W,
                           unsigned w_part, unsigned rows, unsigned cols, unsigned N, unsigned K,
                           unsigned pitch_c, unsigned words_c) {
  unsigned i = 0;
  for (unsigned r = 0; i < words_c; r++)
    for (unsigned c = 0; c < pitch_c && i < words_c; c++, i++)
      X[i] = (r < rows && c < cols) ? exp_load(r, c, src, pitch_src, W, w_part, N, K) : SENTINEL;
}
// only for mix:A, mix:B, mpart
static inline void build_group(uint32_t *X, const uint32_t *G, unsigned zero_mask, unsigned rows,
                        unsigned N, unsigned pitch_c, unsigned words_c) {
  unsigned i = 0;
  for (unsigned r = 0; i < words_c; r++)
    for (unsigned c = 0; c < pitch_c && i < words_c; c++, i++)
      X[i] = (r < rows && c < N) ? exp_group(r, c, G, zero_mask, N) : SENTINEL;
}
