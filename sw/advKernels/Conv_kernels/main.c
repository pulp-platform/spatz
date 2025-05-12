/*
 * Copyright (C) 2021 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>

#define THRESHOLD       0.00010f

#include "benchmark/benchmark.c"

#include "data/data7.h"
#include "Golden/gold7.h"

typedef union float_hex {
  float f;
  uint32_t ui32;
} float_hex;

#define BOX_FLOAT_IN_FLOAT(VAR_NAME, VAL_32B)                                  \
  do {                                                                         \
    float_hex nan_boxed_val;                                                   \
    nan_boxed_val.ui32 = VAL_32B;                                              \
    VAR_NAME = nan_boxed_val.f;                                                \
  } while (0)

// unsigned int hwpe_cg_addr;
// unsigned int hwpe_sel_addr;
// unsigned int hwpe_base_addr;



float *In_Img;
float *Out_Img;
float *Filter_Kern;


float *temp;




void check_result(float *x, int r){    
    printf("check_result\n");
    float diff = 0.0f;
    int err = 0;

    for(int i = 0; i < r; i++){
      // printf("i = %d\n",i);  
        diff = fabs(x[i] - ref[i]);
        if(diff>THRESHOLD){
            err++;
            printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, ref[i], x[i], diff);
        }
    }

    if(err != 0)
        printf("TEST FAILED with %d errors!!\n", err);
    else
        printf("TEST PASSED!!\n");    
}

// V0 - V2  => kernel
// V3 - V5  => In_Img
// V6 - V8  => In_Img Slide down / mul res
// V9 - V11 => redsum
void Conv3 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  int col_idx = 16 - FILT_W + 1; // 16 = VLEN/SEW = 512/32
  float acc[FILT_W];
  for (int row=0; row < ROW; row++) {
    int col = 0;
    int t = 0;
    while(col * col_idx + 16 <= COL){
      asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v3, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v4, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      for(int i = 0; i < col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v6, v3, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v7, v4, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v8, v5, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v6, v6, v0");
        asm volatile("vfmul.vv v7, v7, v1");
        asm volatile("vfmul.vv v8, v8, v2");

        asm volatile("vfredsum.vs v9 , v6, v31");
        asm volatile("vfredsum.vs v10, v7, v31");
        asm volatile("vfredsum.vs v11, v8, v31");
        asm volatile("vfmv.f.s %0, v9 " : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v10" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v11" : "=f"(acc[2]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2];
      }
    col++;
    }
    if(col * col_idx < COL){
      asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(Input_cl - col * col_idx));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v3, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v4, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      for(int i = 0; i < COL - col * col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v6, v3, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v7, v4, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v8, v5, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v6, v6, v0");
        asm volatile("vfmul.vv v7, v7, v1");
        asm volatile("vfmul.vv v8, v8, v2");

        asm volatile("vfredsum.vs v9 , v6, v31");
        asm volatile("vfredsum.vs v10, v7, v31");
        asm volatile("vfredsum.vs v11, v8, v31");
        asm volatile("vfmv.f.s %0, v9 " : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v10" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v11" : "=f"(acc[2]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2];
      }
    }
  }
}

// V0 - V4  => kernel
// V5 - V9  => In_Img
// V10- V14 => In_Img Slide down / mul res
// V15- V19 => redsum
void Conv5 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  int col_idx = 16 - FILT_W + 1; // 16 = VLEN/SEW = 512/32
  float acc[FILT_W];
  for (int row=0; row < ROW; row++) {
    int col = 0;
    int t = 0;
    while(col * col_idx + 16 <= COL){
      asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v6, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v7, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col * col_idx));
      asm volatile("vle32.v v9, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col * col_idx));
      for(int i = 0; i < col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v10, v5, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v11, v6, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v12, v7, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v13, v8, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v14, v9, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v10, v10, v0");
        asm volatile("vfmul.vv v11, v11, v1");
        asm volatile("vfmul.vv v12, v12, v2");
        asm volatile("vfmul.vv v13, v13, v3");
        asm volatile("vfmul.vv v14, v14, v4");

        asm volatile("vfredsum.vs v15, v10, v31");
        asm volatile("vfredsum.vs v16, v11, v31");
        asm volatile("vfredsum.vs v17, v12, v31");
        asm volatile("vfredsum.vs v18, v13, v31");
        asm volatile("vfredsum.vs v19, v14, v31");
        asm volatile("vfmv.f.s %0, v15" : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v16" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v17" : "=f"(acc[2]));
        asm volatile("vfmv.f.s %0, v18" : "=f"(acc[3]));
        asm volatile("vfmv.f.s %0, v19" : "=f"(acc[4]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2] + acc[3] + acc[4];
      }
    col++;
    }
    if(col * col_idx < COL){
      asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(Input_cl - col * col_idx));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v6, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v7, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col * col_idx));
      asm volatile("vle32.v v9, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col * col_idx));
      for(int i = 0; i < COL - col * col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v10, v5, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v11, v6, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v12, v7, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v13, v8, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v14, v9, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v10, v10, v0");
        asm volatile("vfmul.vv v11, v11, v1");
        asm volatile("vfmul.vv v12, v12, v2");
        asm volatile("vfmul.vv v13, v13, v3");
        asm volatile("vfmul.vv v14, v14, v4");

        asm volatile("vfredsum.vs v15, v10, v31");
        asm volatile("vfredsum.vs v16, v11, v31");
        asm volatile("vfredsum.vs v17, v12, v31");
        asm volatile("vfredsum.vs v18, v13, v31");
        asm volatile("vfredsum.vs v19, v14, v31");
        asm volatile("vfmv.f.s %0, v15" : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v16" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v17" : "=f"(acc[2]));
        asm volatile("vfmv.f.s %0, v18" : "=f"(acc[3]));
        asm volatile("vfmv.f.s %0, v19" : "=f"(acc[4]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2] + acc[3] + acc[4];
      }
    }
  }
}

// V0 - V6  => kernel
// V7 - V13 => In_Img
// V14- V20 => In_Img Slide down / mul res
// V21- V27 => redsum
void Conv7 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  asm volatile("vle32.v v5, (%0)" :: "r"(Kernel+5*FILT_W));
  asm volatile("vle32.v v6, (%0)" :: "r"(Kernel+6*FILT_W));
  int col_idx = 16 - FILT_W + 1; // 16 = VLEN/SEW = 512/32
  float acc[FILT_W];
  for (int row=0; row < ROW; row++) {
    int col = 0;
    int t = 0;
    while(col * col_idx + 16 <= COL){
      asm volatile("vsetvli %0 , %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v7 , (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v8 , (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v9 , (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      asm volatile("vle32.v v10, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col * col_idx));
      asm volatile("vle32.v v11, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col * col_idx));
      asm volatile("vle32.v v12, (%0)" :: "r"(In_Img+(row+5)*Input_cl + col * col_idx));
      asm volatile("vle32.v v13, (%0)" :: "r"(In_Img+(row+6)*Input_cl + col * col_idx));
      for(int i = 0; i < col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v14, v7 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v15, v8 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v16, v9 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v17, v10, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v18, v11, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v19, v12, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v20, v13, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v14, v14, v0");
        asm volatile("vfmul.vv v15, v15, v1");
        asm volatile("vfmul.vv v16, v16, v2");
        asm volatile("vfmul.vv v17, v17, v3");
        asm volatile("vfmul.vv v18, v18, v4");
        asm volatile("vfmul.vv v19, v19, v5");
        asm volatile("vfmul.vv v20, v20, v6");

        asm volatile("vfredsum.vs v21, v14, v31");
        asm volatile("vfredsum.vs v22, v15, v31");
        asm volatile("vfredsum.vs v23, v16, v31");
        asm volatile("vfredsum.vs v24, v17, v31");
        asm volatile("vfredsum.vs v25, v18, v31");
        asm volatile("vfredsum.vs v26, v19, v31");
        asm volatile("vfredsum.vs v27, v20, v31");
        asm volatile("vfmv.f.s %0, v21" : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v22" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v23" : "=f"(acc[2]));
        asm volatile("vfmv.f.s %0, v24" : "=f"(acc[3]));
        asm volatile("vfmv.f.s %0, v25" : "=f"(acc[4]));
        asm volatile("vfmv.f.s %0, v26" : "=f"(acc[5]));
        asm volatile("vfmv.f.s %0, v27" : "=f"(acc[6]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2] + acc[3] + acc[4] + acc[5] + acc[6];
      }
    col++;
    }
    if(col * col_idx < COL){
      asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(Input_cl - col * col_idx));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v7 , (%0)" :: "r"(In_Img+(row+0)*Input_cl + col * col_idx));
      asm volatile("vle32.v v8 , (%0)" :: "r"(In_Img+(row+1)*Input_cl + col * col_idx));
      asm volatile("vle32.v v9 , (%0)" :: "r"(In_Img+(row+2)*Input_cl + col * col_idx));
      asm volatile("vle32.v v10, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col * col_idx));
      asm volatile("vle32.v v11, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col * col_idx));
      asm volatile("vle32.v v12, (%0)" :: "r"(In_Img+(row+5)*Input_cl + col * col_idx));
      asm volatile("vle32.v v13, (%0)" :: "r"(In_Img+(row+6)*Input_cl + col * col_idx));
      for(int i = 0; i < COL - col * col_idx; i++){
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vslidedown.vx v14, v7 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v15, v8 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v16, v9 , %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v17, v10, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v18, v11, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v19, v12, %[A]" ::[A] "r"(i));
        asm volatile("vslidedown.vx v20, v13, %[A]" ::[A] "r"(i));
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128

        asm volatile("vfmul.vv v14, v14, v0");
        asm volatile("vfmul.vv v15, v15, v1");
        asm volatile("vfmul.vv v16, v16, v2");
        asm volatile("vfmul.vv v17, v17, v3");
        asm volatile("vfmul.vv v18, v18, v4");
        asm volatile("vfmul.vv v19, v19, v5");
        asm volatile("vfmul.vv v20, v20, v6");

        asm volatile("vfredsum.vs v21, v14, v31");
        asm volatile("vfredsum.vs v22, v15, v31");
        asm volatile("vfredsum.vs v23, v16, v31");
        asm volatile("vfredsum.vs v24, v17, v31");
        asm volatile("vfredsum.vs v25, v18, v31");
        asm volatile("vfredsum.vs v26, v19, v31");
        asm volatile("vfredsum.vs v27, v20, v31");
        asm volatile("vfmv.f.s %0, v21" : "=f"(acc[0]));
        asm volatile("vfmv.f.s %0, v22" : "=f"(acc[1]));
        asm volatile("vfmv.f.s %0, v23" : "=f"(acc[2]));
        asm volatile("vfmv.f.s %0, v24" : "=f"(acc[3]));
        asm volatile("vfmv.f.s %0, v25" : "=f"(acc[4]));
        asm volatile("vfmv.f.s %0, v26" : "=f"(acc[5]));
        asm volatile("vfmv.f.s %0, v27" : "=f"(acc[6]));

        t = row*COL + col * col_idx + i;
        Out_Img[t] = acc[0] + acc[1] + acc[2] + acc[3] + acc[4] + acc[5] + acc[6];
      }
    }
  }
}


int main() {

    volatile int errors = 0;

    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;

    printf("start_addr: %x \n", snrt_cluster_memory().end);

    int reorder_enable = 1;

    // Core 0 programs DMA transfer of inputs
    if (cid == 0) {
        In_Img = (float   *)snrt_l1alloc(INP_COL*INP_COL * sizeof(float));
        snrt_dma_start_1d(In_Img, In_Img_dram, INP_COL*INP_COL * sizeof(float));

        Filter_Kern = (float   *)snrt_l1alloc(FILT_WIN*FILT_WIN * sizeof(float));
        snrt_dma_start_1d(Filter_Kern, Filter_Kern_dram, FILT_WIN*FILT_WIN * sizeof(float));
        
        Out_Img = (float   *)snrt_l1alloc(OUT_ROW*OUT_COL * sizeof(float));

        temp = (float   *)snrt_l1alloc(INP_COL*INP_COL * sizeof(float));
    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    printf("Conv Start\n");
    // Conv3(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv5(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    Conv7(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    printf("Conv Done\n");
    // End dump
    stop_kernel();

    // float check_vect[N_CLUSTERS*N_COORDS];
    // int c = 0;
    // if(cid == 0)
    // {    
    //     for (int i=0; i<N_CLUSTERS; i++) {
    //         for (int j=0; j<N_COORDS; j++) {
    //             check_vect[c] = clusters[i][j];
    //             c++;
    //         }   
    //     }
    //     check_result(check_vect, N_CLUSTERS*N_COORDS);
    // }

    check_result(Out_Img,OUT_ROW*OUT_COL);


    snrt_cluster_hw_barrier();
    return errors;

}
