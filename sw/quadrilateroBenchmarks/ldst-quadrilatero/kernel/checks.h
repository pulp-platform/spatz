#ifndef CHECKS_H
#define CHECKS_H
#include <stdint.h>

#define SENTINEL    0xDEADBEEFu   // "never written" marker
#define GUARD_WORDS 16            // guard zone after the block

enum { ERR_NONE = 0, ERR_MISMATCH, ERR_UNWRITTEN, ERR_OVERFLOW };

void     fill_src(uint32_t *S, unsigned n_words);
void     prep_C(uint32_t *C, unsigned n_words);
int      check_copy(const uint32_t *exp, const uint32_t *C, unsigned rows, unsigned cols,
                    unsigned pitch_w, unsigned total_words, unsigned *bad_idx);
unsigned wait_block_written(const uint32_t *C, unsigned rows, unsigned cols,
                            unsigned pitch_w, unsigned max_iter, unsigned pause);
int      report(const char *op, const char *reg, int e, unsigned idx, unsigned loaded);
void     dump(const char *op, const uint32_t *src, unsigned pitch_src,
              const uint32_t *C, unsigned pitch_c, unsigned rows, unsigned cols);
#endif
