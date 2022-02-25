#include "conv2d.h"
#include <stdio.h>
#include "runtime.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#ifndef MATRIX_DIM
#define MATRIX_DIM 64
#endif
#ifndef KERNEL_M
#define KERNEL_M 3
#endif

#define R MATRIX_DIM
#define C MATRIX_DIM
#define F KERNEL_M

void conv2d_3x3(int32_t *o, int32_t *i, int32_t *f, int32_t num_rows, int32_t num_columns) {
  // We work on 4 rows of the output matrix at once
  int32_t block_size_o = 4;
  // We work on block_size_o + F - 1 rows of the input matrix at once

  int32_t block_size_c;
  int32_t *i_cpy = i;

  // Set the vector configuration
  asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(block_size_c) : "r"(num_columns));

  for (int32_t c = 0; c < num_columns; c += block_size_c) {

    int32_t c_ = MIN(num_columns - c, block_size_c);

    // First iteration round, r = 0
    int32_t *i = i_cpy + c;
    int32_t *o_ = o + c;

    // Preload the first two input rows -> This is not needed in the other rounds
    int32_t *i__ = i;
    asm volatile("vle32.v v8,  (%0)" :: "r"(i__));
    i__ += (C + F - 1);
    asm volatile("vle32.v v10, (%0)" :: "r"(i__));

    // Preload t0 and t1
    int32_t t0, t1, t2;
    t0 = *f;
    t1 = *(f+F);

    // Iterate over the output rows
    for (int32_t r = 0; r < num_rows; r += block_size_o) {
      // Temporary filter variables

      // Slide element insert
      int32_t *i_ = i + c_;
      int32_t *i__ = i + (F - 1) * (C + F - 1);
      int32_t sld_elem;

      // Helper variables
      int32_t lwi = C + F - 1;
      int32_t lwf = F;
      int32_t lwo = C;
      int32_t *f_ = f + (lwf << 1);
      int32_t *o__ = o_;

      // Instructions in first part are ordered in a messy way, so
      // that we are able to hide latency of vector loads and stores

      asm volatile("vmv.v.i v0,  0");
      asm volatile("vmacc.vx v0, %0, v8" ::"r"(t0));
      asm volatile("vmv.v.i v2,  0");
      asm volatile("vmacc.vx v2, %0, v10" ::"r"(t0));

      asm volatile("vle32.v v12, (%0)" :: "r"(i__));
      i__ += lwi;

      asm volatile("vmacc.vx v0, %0, v10" ::"r"(t1));
      sld_elem = *i_;
      asm volatile("vmv.v.i v4,  0");
      asm volatile("vmacc.vx v4, %0, v12" ::"r"(t0));

      asm volatile("vle32.v v14, (%0)" :: "r"(i__));
      i__ += lwi;

      t2 = *f_;
      asm volatile("vslide1down.vx v20, v8, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;

      asm volatile("vmacc.vx v2, %0, v12" ::"r"(t1));
      asm volatile("vmacc.vx v0, %0, v12" ::"r"(t2));
      asm volatile("vmv.v.i v6,  0");

      asm volatile("vle32.v v16, (%0)" :: "r"(i__));
      i__ += lwi;

      asm volatile("vmacc.vx v6, %0, v14" ::"r"(t0));
      asm volatile("vmacc.vx v4, %0, v14" ::"r"(t1));
      asm volatile("vmacc.vx v2, %0, v14" ::"r"(t2));

      asm volatile("vle32.v v18, (%0)" :: "r"(i__));

      asm volatile("vmacc.vx v6, %0, v16" ::"r"(t1));

      f_ = f + 1;
      t0 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v4, %0, v16" ::"r"(t2));
      asm volatile("vmacc.vx v6, %0, v18" ::"r"(t2));

      // Fetch the middle column of the filter, and start calculating its
      // contributions on the output rows To do so, slide down the input rows by one
      t1 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v22, v10, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

      t2 = *f_;
      asm volatile("vslide1down.vx v24, v12, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
      asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

      asm volatile("vslide1down.vx v26, v14, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
      asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));
      asm volatile("vmacc.vx v4, %0, v24" ::"r"(t0));

      asm volatile("vslide1down.vx v28, v16, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));
      asm volatile("vmacc.vx v4, %0, v26" ::"r"(t1));
      asm volatile("vmacc.vx v6, %0, v26" ::"r"(t0));

      asm volatile("vslide1down.vx v30, v18, %0" ::"r"(sld_elem));
      i_ = i + c_ + 1;
      sld_elem = *i_;
      asm volatile("vmacc.vx v4, %0, v28" ::"r"(t2));
      asm volatile("vmacc.vx v6, %0, v28" ::"r"(t1));

      f_ = f + 2;
      t0 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v8, v20, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v6, %0, v30" ::"r"(t2));

      // Repeat for the last filter column, and then store the output rows
      t1 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v10, v22, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v8" ::"r"(t0));

      t2 = *f_;
      asm volatile("vslide1down.vx v12, v24, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v10" ::"r"(t1));
      asm volatile("vmacc.vx v0, %0, v12" ::"r"(t2));
      i_ += lwi;
      int32_t sld_elem_tmp = *i_;
      asm volatile("vse32.v  v0, (%0)" :: "r"(o__));
      o__ += lwo;
      asm volatile("vmv.v.v v8, v16");
      asm volatile("vmacc.vx v2, %0, v10" ::"r"(t0));
      asm volatile("vmv.v.v v10, v18");

      asm volatile("vmacc.vx v2, %0, v12" ::"r"(t1));
      asm volatile("vmacc.vx v4, %0, v12" ::"r"(t0));

      asm volatile("vslide1down.vx v14, v26, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v2, %0, v14" ::"r"(t2));
      asm volatile("vslide1down.vx v16, v28, %0" ::"r"(sld_elem_tmp));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vse32.v  v2, (%0)" :: "r"(o__));
      o__ += lwo;
      asm volatile("vmacc.vx v4, %0, v14" ::"r"(t1));
      asm volatile("vmacc.vx v6, %0, v14" ::"r"(t0));

      asm volatile("vmacc.vx v4, %0, v16" ::"r"(t2));
      f_ = f;
      t0 = *f_;
      f_ += lwf;
      asm volatile("vse32.v  v4, (%0)" :: "r"(o__));
      o__ += lwo;
      asm volatile("vmacc.vx v6, %0, v16" ::"r"(t1));
      asm volatile("vslide1down.vx v18, v30, %0" ::"r"(sld_elem));

      asm volatile("vmacc.vx v6, %0, v18" ::"r"(t2));
      t1 = *f_;
      asm volatile("vse32.v  v6, (%0);" : "+r"(o__));

      i += block_size_o * (C + F - 1);
      o_ += block_size_o * C;
    }
  }
}

#undef R
#undef C
#undef F
