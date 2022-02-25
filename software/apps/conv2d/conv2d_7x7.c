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

    int32_t t0, t1, t2, t3, t4, t5, t6;
    t0 = *f;

    // Preload rows
    int32_t *i__ = i;
    asm volatile("vle32.v v4,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v6,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v8,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vmv.v.i v0,  0");
    asm volatile("vmacc.vx v0, %0, v4" ::"r"(t0));
    asm volatile("vle32.v v10,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vmv.v.i v2,  0");
    asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));
    asm volatile("vle32.v v12,  (%0)" :: "r"(i__));
    i__ += lwi;
    asm volatile("vle32.v v14,  (%0)" :: "r"(i__));

    t1 = *f + F;

    // Iterate over the output rows
    for (int32_t r = 0; r < num_rows; r += block_size_o) {
      // Helper variables
      int32_t lwo = C;
      int32_t lwi = (C + F - 1);
      int32_t lwf = F;
      int32_t *f_ = f;
      int32_t *o__ = o_;
      int32_t sld_elem;

      asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));
      asm volatile("vmv.v.v v9, v8");

      int32_t *i_ = i + c_;
      int32_t *i__ = i + (F - 1) * (C + F - 1);

      t2 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
      asm volatile("vle32.v v16, (%0)" :: "r"(i__));
      i__ += lwi;


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

      asm volatile("vle32.v v18, (%0)" :: "r"(i__));

      t5 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
      asm volatile("vmv.v.v v15, v14");
      sld_elem = *i_;
      asm volatile("vmacc.vx v2, %0, v14" ::"r"(t4));

      t6 = *f_;
      asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
      asm volatile("vmv.v.v v17, v16");
      asm volatile("vmacc.vx v2, %0, v16" ::"r"(t5));

      asm volatile("vmv.v.v v19, v18");
      f_ = f+1;
      t0 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v16" ::"r"(t6));
      asm volatile("vslide1down.vx v20, v4, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v2, %0, v18" ::"r"(t6));

      for (int32_t idx = 1; idx < F - 1; idx += 2) {
        // Fetch the other columns of the filter (except for the last one), and
        // start calculating their contributions on the two output rows (v0, v2) To
        // do so, at each iteration slide down the input rows by one
        t1 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
        asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;

        asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));
        t2 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
        asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;

        t3 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
        asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

        t4 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));
        asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
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
        asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
        asm volatile("vslide1down.vx v6, v22, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;

        asm volatile("vmacc.vx v0, %0, v6" ::"r"(t1));
        t2 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));
        asm volatile("vslide1down.vx v8, v24, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;

        t3 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v8" ::"r"(t2));
        asm volatile("vslide1down.vx v10, v26, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v8" ::"r"(t1));

        t4 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v10" ::"r"(t3));
        asm volatile("vslide1down.vx v12, v28, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v10" ::"r"(t2));

        t5 = *f_;
        f_ += lwf;
        asm volatile("vmacc.vx v0, %0, v12" ::"r"(t4));
        asm volatile("vslide1down.vx v14, v30, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
        asm volatile("vmacc.vx v2, %0, v12" ::"r"(t3));

        t6 = *f_;
        asm volatile("vmacc.vx v0, %0, v14" ::"r"(t5));
        asm volatile("vslide1down.vx v16, v29, %0" ::"r"(sld_elem));
        i_ += lwi;
        sld_elem = *i_;
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
      asm volatile("vslide1down.vx v22, v6, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t0));
      t1 = *f_;
      f_ += lwf;

      asm volatile("vslide1down.vx v24, v8, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v22" ::"r"(t1));
      t2 = *f_;
      f_ += lwf;
      asm volatile("vmv.v.v v4, v9");
      asm volatile("vmacc.vx v2, %0, v22" ::"r"(t0));

      asm volatile("vslide1down.vx v26, v10, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v24" ::"r"(t2));
      t3 = *f_;
      f_ += lwf;
      asm volatile("vmv.v.v v6, v11");
      asm volatile("vmacc.vx v2, %0, v24" ::"r"(t1));

      t4 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v28, v12, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v26" ::"r"(t3));

      t5 = *f_;
      f_ += lwf;
      asm volatile("vslide1down.vx v30, v14, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;
      asm volatile("vmacc.vx v0, %0, v28" ::"r"(t4));
      asm volatile("vmv.v.v v10, v15");

      t6 = *f_;
      f_ += lwf;
      asm volatile("vmacc.vx v0, %0, v30" ::"r"(t5));
      asm volatile("vslide1down.vx v29, v16, %0" ::"r"(sld_elem));
      i_ += lwi;
      sld_elem = *i_;

      asm volatile("vmacc.vx v0, %0, v20" ::"r"(t6));

      t0 = *f;
      t1 = *f + F;

      asm volatile("vse32.v  v0, (%0)" :: "r"(o__));
      o__ += lwo;

      asm volatile("vmv.v.v v8, v13");
      asm volatile("vmacc.vx v2, %0, v26" ::"r"(t2));
      asm volatile("vmv.v.v v12, v17");
      asm volatile("vmacc.vx v2, %0, v28" ::"r"(t3));
      asm volatile("vslide1down.vx v31, v18, %0" ::"r"(sld_elem));
      asm volatile("vmacc.vx v2, %0, v30" ::"r"(t4));


      asm volatile("vmv.v.v v14, v19");
      asm volatile("vmacc.vx v2, %0, v20" ::"r"(t5));

      asm volatile("vmacc.vx v2, %0, v22" ::"r"(t6));
      asm volatile("vse32.v  v2, (%0)" :: "r"(o__));

      i += block_size_o * (C + F - 1);
      o_ += block_size_o * C;

      asm volatile("vmv.v.i v0,  0");
      asm volatile("vmacc.vx v0, %0, v4" ::"r"(t0));

      asm volatile("vmv.v.i v2,  0");
      asm volatile("vmacc.vx v2, %0, v6" ::"r"(t0));
    }
  }
}

#undef R
#undef C
#undef F
