#include "conv2d.h"
#include <stdio.h>

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

void conv2d_7x7(int32_t *o, int32_t *i, int32_t *f, int32_t num_rows, int32_t num_columns) {
  // We work on 2 rows of the output matrix at once
  int32_t block_size_o = 2;
  // We work on block_size_o + F - 1 rows of the input matrix at once

  int32_t block_size_c;

  int32_t *i_cpy = i;

  // Set the vector configuration
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(block_size_c) : "r"(num_columns));

  for (int32_t c = 0; c < num_columns; c += block_size_c) {

    int32_t c_ = MIN(num_columns - c, block_size_c);

    // First iteration round, r = 0
    int32_t *i = i_cpy + c;
    int32_t *o_ = o + c;

    int32_t lwi = (C + F - 1);

    // Preload the first two input rows
    int32_t *i__ = i;
    asm volatile("vle32.v v4,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v6,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v8,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v10,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v12,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v14,  (%0)" :: "r"(i__));

    int32_t t0, t1, t2, t3, t4, t5, t6;

    // Iterate over the output rows
    for (int32_t r = 0; r < num_rows; r += block_size_o) {

      int32_t *i_ = i + c_;
      int32_t *i__ = i + (F - 1) * (C + F - 1);
      int32_t sld_elem = *i_;

      // Helper variables
      int32_t lwo = C;
      int32_t lwi = (C + F - 1);
      int32_t lwf = F;
      int32_t *f_;
      int32_t *o__ = o_;


      // Fetch other 2 rows of the input matrix
      asm volatile("vle32.v v16, (%0)" :: "r"(i__));
      i__ += lwi;
      f_ = f;
      asm volatile("vmv.v.i v0,  0");
      asm volatile("vle32.v v18, (%0)" :: "r"(i__));
      t0 = *f_;
      f_ += lwf;
      asm volatile("vmv.v.i v2,  0");

      // Fetch the first column of the filter, and start calculating its
      // contribution on the two output rows (v0, v2)
      t1 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v4" ::"r"(t0));
      asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));

      t2 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
      asm volatile("vmv.v.v v9, v8");
      asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));

      t3 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v8" ::"r"(t2));
      asm volatile("vmv.v.v v11, v10");
      asm volatile("vmacc.vx v2, %0, v10" ::"r"(t2));

      t4 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v10" ::"r"(t3));
      asm volatile("vmv.v.v v13, v12");
      asm volatile("vmacc.vx v2, %0, v12" ::"r"(t3));

      t5 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
      asm volatile("vmv.v.v v15, v14");
      asm volatile("vmacc.vx v2, %0, v14" ::"r"(t4));

      t6 = *f_;
      asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
      asm volatile("vmv.v.v v17, v16");
      asm volatile("vmacc.vx v2, %0, v16" ::"r"(t5));

      asm volatile("vmv.v.v v19, v18");
      asm volatile("vmacc.vx v0, %0, v16" ::"r"(t6));
      asm volatile("vslide1down.vx v20, v4, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      f_ = f+1;
      t0 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v2, %0, v18" ::"r"(t6));

      for (int32_t idx = 1; idx < F - 1; idx += 2) {
        // Fetch the other columns of the filter (except for the last one), and
        // start calculating their contributions on the two output rows (v0, v2) To
        // do so, at each iteration slide down the input rows by one
        t1 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

        t2 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
        asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

        t3 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
        asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

        t4 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));
        asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));

        t5 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v30, v14, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v28" ::"r"(t4));
        asm volatile("vmacc.vx v2, %0, v28" ::"r"(t3));

        t6 = *f_;
        asm volatile("vslide1down.vx v29, v16, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v30" ::"r"(t5));
        asm volatile("vmacc.vx v2, %0, v30" ::"r"(t4));

        asm volatile("vslide1down.vx v31, v18, %0" ::"r"(sld_elem));
        i_ = i + c + idx;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t6));
        f_ = f+idx+1;
        t0 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v2, %0, v20" ::"r"(t5));

        asm volatile("vslide1down.vx v20, v4, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v22" ::"r"(t6));

        // Unroll 1

        t1 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v6, v22, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

        t2 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v8, v24, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
        asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));

        t3 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v10, v26, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v8" ::"r"(t2));
        asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));

        t4 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v12, v28, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v10" ::"r"(t3));
        asm volatile("vmacc.vx v2, %0, v10" ::"r"(t2));

        t5 = *f_;
        f_ += lwf;
        asm volatile("vslide1down.vx v14, v30, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
        asm volatile("vmacc.vx v2, %0, v12" ::"r"(t3));

        t6 = *f_;
        asm volatile("vslide1down.vx v16, v29, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
        asm volatile("vmacc.vx v2, %0, v14" ::"r"(t4));

        asm volatile("vslide1down.vx v18, v31, %0" ::"r"(sld_elem));
        i_ = i + c_ + idx + 1;
        sld_elem = *i_;
        asm volatile("vmacc.vx v0, %0, v16" ::"r"(t6));
        f_ = f+idx+2;
        t0 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v2, %0, v16" ::"r"(t5));

        asm volatile("vslide1down.vx v4, v20, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v18" ::"r"(t6));
      }

      //f_ = f + (F - 1) + lwf;
      //slamt = F - 1;
      // Repeat for the last filter column, and then store the output rows
      t1 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

      t2 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
      asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

      t3 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
      asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

      t4 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));
      asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));

      t5 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v30, v14, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v28" ::"r"(t4));
      asm volatile("vmacc.vx v2, %0, v28" ::"r"(t3));

      t6 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v29, v16, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v30" ::"r"(t5));
      asm volatile("vmacc.vx v2, %0, v30" ::"r"(t4));

      asm volatile("vslide1down.vx v31, v18, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t6));
      asm volatile("vse32.v  v0, (%0)" :: "r"(o__));
      o__ += lwo;
      asm volatile("vmacc.vx v2, %0, v20" ::"r"(t5));

      asm volatile("vmacc.vx v2, %0, v22" ::"r"(t6));
      asm volatile("vse32.v  v2, (%0)" :: "r"(o__));

      // Move the last F-1 input rows
      asm volatile("vmv.v.v v4, v9");
      asm volatile("vmv.v.v v6, v11");
      asm volatile("vmv.v.v v8, v13");
      asm volatile("vmv.v.v v10, v15");
      asm volatile("vmv.v.v v12, v17");
      asm volatile("vmv.v.v v14, v19");

      i += block_size_o * (C + F - 1);
      o_ += block_size_o * C;
    }
  }
}

// Load 4 rows of the output matrix
void conv2d_vec_4xC_slice_preload_7x7(int32_t *i, int32_t c) {
  // Helper variables
  int32_t lwi = (C + F - 1) << 2;

  // Fetch the first F-1 = 6 input rows
  asm volatile("vle32.v v4,  (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi));
  asm volatile("vle32.v v6,  (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi));
  asm volatile("vle32.v v8,  (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi));
  asm volatile("vle32.v v10, (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi));
  asm volatile("vle32.v v12, (%0); add %0, %0, %1" : "+&r"(i) : "r"(lwi));
  asm volatile("vle32.v v14, (%0)" :: "r"(i));
}

// Calculate 4 output matrix rows
void conv2d_vec_4xC_7x7(int32_t *o, int32_t *i, int32_t *f, int32_t c) {

  // Temporary variables (one filter column)
  int32_t t0, t1, t2, t3, t4, t5, t6;
  int32_t slamt = 1;

  int32_t *i_ = i + c;
  int32_t *i__ = i + (F - 1) * (C + F - 1);
  int32_t sld_elem = *i_;

  // Helper variables
  int32_t lwo = C;
  int32_t lwi = (C + F - 1);
  int32_t lwf = F;
  int32_t *f_;

  asm volatile("vmv.v.i v0,  0");

  // Fetch other 2 rows of the input matrix
  asm volatile("vle32.v v16, (%0)" :: "r"(i__));
  i__ += lwi;
  f_ = f;
  asm volatile("vle32.v v18, (%0)" :: "r"(i__));
  t0 = *f_;
  f_ += lwf;
  asm volatile("vmv.v.i v2,  0");

  // Fetch the first column of the filter, and start calculating its
  // contribution on the two output rows (v0, v2)
  t1 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v0, %0, v4" ::"r"(t0));
  asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));

  t2 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
  asm volatile("vmv.v.v v9, v8");
  asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));

  t3 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v0, %0, v8" ::"r"(t2));
  asm volatile("vmv.v.v v11, v10");
  asm volatile("vmacc.vx v2, %0, v10" ::"r"(t2));

  t4 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v0, %0, v10" ::"r"(t3));
  asm volatile("vmv.v.v v13, v12");
  asm volatile("vmacc.vx v2, %0, v12" ::"r"(t3));

  t5 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
  asm volatile("vmv.v.v v15, v14");
  asm volatile("vmacc.vx v2, %0, v14" ::"r"(t4));

  t6 = *f_;
  asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
  asm volatile("vmv.v.v v17, v16");
  asm volatile("vmacc.vx v2, %0, v16" ::"r"(t5));

  asm volatile("vmv.v.v v19, v18");
  asm volatile("vmacc.vx v0, %0, v16" ::"r"(t6));
  asm volatile("vslide1down.vx v20, v4, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  f_ = f+1;
  t0 = *f_;
  f_ += lwf;
  asm volatile("vmacc.vx v2, %0, v18" ::"r"(t6));

  for (int32_t idx = 1; idx < F - 1; idx += 2) {
    // Fetch the other columns of the filter (except for the last one), and
    // start calculating their contributions on the two output rows (v0, v2) To
    // do so, at each iteration slide down the input rows by one
    t1 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

    t2 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
    asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

    t3 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
    asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

    t4 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));
    asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));

    t5 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v30, v14, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v28" ::"r"(t4));
    asm volatile("vmacc.vx v2, %0, v28" ::"r"(t3));

    t6 = *f_;
    asm volatile("vslide1down.vx v29, v16, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v30" ::"r"(t5));
    asm volatile("vmacc.vx v2, %0, v30" ::"r"(t4));

    asm volatile("vslide1down.vx v31, v18, %0" ::"r"(sld_elem));
    i_ = i + c + idx;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t6));
    f_ = f+idx+1;
    t0 = *f_;
    f_ += lwf;
    asm volatile("vmacc.vx v2, %0, v20" ::"r"(t5));

    asm volatile("vslide1down.vx v20, v4, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v2, %0, v22" ::"r"(t6));

    // Unroll 1

    t1 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v6, v22, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

    t2 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v8, v24, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
    asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));

    t3 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v10, v26, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v8" ::"r"(t2));
    asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));

    t4 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v12, v28, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v10" ::"r"(t3));
    asm volatile("vmacc.vx v2, %0, v10" ::"r"(t2));

    t5 = *f_;
    f_ += lwf;
    asm volatile("vslide1down.vx v14, v30, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
    asm volatile("vmacc.vx v2, %0, v12" ::"r"(t3));

    t6 = *f_;
    asm volatile("vslide1down.vx v16, v29, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
    asm volatile("vmacc.vx v2, %0, v14" ::"r"(t4));

    asm volatile("vslide1down.vx v18, v31, %0" ::"r"(sld_elem));
    i_ = i + c + idx + 1;
    sld_elem = *i_;
    asm volatile("vmacc.vx v0, %0, v16" ::"r"(t6));
    f_ = f+idx+2;
    t0 = *f_;
    f_ += lwf;
    asm volatile("vmacc.vx v2, %0, v16" ::"r"(t5));

    asm volatile("vslide1down.vx v4, v20, %0" ::"r"(sld_elem));
    i_ += lwi;
    sld_elem = *i_;
    asm volatile("vmacc.vx v2, %0, v18" ::"r"(t6));
  }

  //f_ = f + (F - 1) + lwf;
  //slamt = F - 1;
  // Repeat for the last filter column, and then store the output rows
  t1 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));

  t2 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
  asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

  t3 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
  asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

  t4 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));
  asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));

  t5 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v30, v14, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v28" ::"r"(t4));
  asm volatile("vmacc.vx v2, %0, v28" ::"r"(t3));

  t6 = *f_;
  f_ += lwf;
  asm volatile("vslide1down.vx v29, v16, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v30" ::"r"(t5));
  asm volatile("vmacc.vx v2, %0, v30" ::"r"(t4));

  asm volatile("vslide1down.vx v31, v18, %0" ::"r"(sld_elem));
  i_ += lwi;
  sld_elem = *i_;
  asm volatile("vmacc.vx v0, %0, v20" ::"r"(t6));
  asm volatile("vse32.v  v0, (%0)" :: "r"(o));
  o += lwo;
  asm volatile("vmacc.vx v2, %0, v20" ::"r"(t5));

  asm volatile("vmacc.vx v2, %0, v22" ::"r"(t6));
  asm volatile("vse32.v  v2, (%0)" :: "r"(o));

  // Move the last F-1 input rows
  asm volatile("vmv.v.v v4, v9");
  asm volatile("vmv.v.v v6, v11");
  asm volatile("vmv.v.v v8, v13");
  asm volatile("vmv.v.v v10, v15");
  asm volatile("vmv.v.v v12, v17");
  asm volatile("vmv.v.v v14, v19");
}

#undef R
#undef C
#undef F
