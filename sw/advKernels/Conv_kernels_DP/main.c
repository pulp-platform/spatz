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

// #include "data/data3_mini.h"
// #include "Golden/gold3_mini.h"

// #include "data/data3.h"
// #include "Golden/gold3.h"
//----------------------------
// #include "data/data5.h"
// #include "Golden/gold5.h"
// //----------------------------
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



double *In_Img;
double *Out_Img;
double *Filter_Kern;


double *tempArr;




void check_result(double *x, int r){    
    printf("check_result\n");
    double diff = 0.0f;
    int err = 0;

    for(int i = 0; i < r; i++){
      // printf("i = %d\n",i);  
        // diff = fabs(x[i] - ref[i]);
        diff = fabs(x[i] - ref[i]);
        if(diff>THRESHOLD){
            err++;
            printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, ref[i], x[i], diff);
            // printf("Error index %d: result = %f (@ %x), golden = %f\n", i,
            //       *((unsigned int *)&x[i]), (unsigned int)(x + i),
            //       *((unsigned int *)&ref[i]));            
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
        
        asm volatile("vfadd.vv v6, v7, v6");
        asm volatile("vfadd.vv v6, v8, v6");
        asm volatile("vfredsum.vs v9 , v6, v31");
        asm volatile("vfmv.f.s %0, v9" : "=f"(Out_Img[row*COL + col * col_idx + i]));
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

        asm volatile("vfadd.vv v6, v7, v6");
        asm volatile("vfadd.vv v6, v8, v6");
        asm volatile("vfredsum.vs v9 , v6, v31");
        asm volatile("vfmv.f.s %0, v9" : "=f"(Out_Img[row*COL + col * col_idx + i]));
      }
    }
  }
}


// V0 - V2  => kernel
// V3 - V5  => In_Img
// V6 - V8  => In_Img Slide down / mul res
// V9 - V11 => redsum
void Conv3_v2 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  for (int row=0; row < ROW; row++) {
    for (int col=0; col < COL; col++) {
      asm volatile("vle32.v v3, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col));
      asm volatile("vle32.v v4, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col));
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col));
      // snrt_cluster_hw_barrier();      
      asm volatile("vfmul.vv v3, v3, v0");
      asm volatile("vfmul.vv v4, v4, v1");
      asm volatile("vfmul.vv v5, v5, v2");

      asm volatile("vfadd.vv v3, v4, v3");
      asm volatile("vfadd.vv v3, v5, v3");
            
      asm volatile("vfredsum.vs v6 , v3, v31");
      asm volatile("vfmv.f.s %0, v6" : "=f"(Out_Img[row*COL + col]));
    }
  }
}

void Conv3_v3 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  for (int row=0; row < ROW; row++) {
    asm volatile("vle32.v v3, (%0)" :: "r"(In_Img+(row+0)*Input_cl));
    asm volatile("vle32.v v4, (%0)" :: "r"(In_Img+(row+1)*Input_cl));
    asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+2)*Input_cl));    
    for (int col=0; col < COL; col++) {
      // snrt_cluster_hw_barrier();      
      asm volatile("vfmul.vv v3, v3, v0");
      asm volatile("vfmul.vv v4, v4, v1");
      asm volatile("vfmul.vv v5, v5, v2");

      asm volatile("vfadd.vv v3, v4, v3");
      asm volatile("vfadd.vv v6, v5, v3");
            
      asm volatile("vfredsum.vs v7 , v6, v31");
      
      asm volatile("vle32.v v3, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col+1));
      asm volatile("vle32.v v4, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col+1));
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col+1));

      asm volatile("vfmv.f.s %0, v7" : "=f"(Out_Img[row*COL + col]));
    }
  }
}

void Conv3_v4 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 128;
  float f0,f1,f2;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    asm volatile("vle32.v  v0, (%0)" :: "r"(In_Img+(row)*Input_cl));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        f0 = Kernel[i*FILT_W+0];
        f1 = Kernel[i*FILT_W+1];
        f2 = Kernel[i*FILT_W+2];
        asm volatile("vslide1down.vx v8, v0, %[A]" ::[A] "r"(0));
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        asm volatile("vslide1down.vx v0, v8, %[A]" ::[A] "r"(0));
        asm volatile("vfmacc.vf  v16, %0, v8" ::"f"(f1));
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+i+1)*Input_cl+itr*VLMAX));
      }
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX-FILT_W+1));
      asm volatile("vse32.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        f0 = Kernel[i*FILT_W+0];
        f1 = Kernel[i*FILT_W+1];
        f2 = Kernel[i*FILT_W+2];
        asm volatile("vslide1down.vx v8, v0, %[A]" ::[A] "r"(0));
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        asm volatile("vslide1down.vx v0, v8, %[A]" ::[A] "r"(0));
        asm volatile("vfmacc.vf  v16, %0, v8" ::"f"(f1));
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+i+1)*Input_cl+itr*VLMAX));
      }
      asm volatile("vse32.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv3_v5 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 128;
  float f;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        for (int j = 0; j < FILT_W; j++){
          f = Kernel[i*FILT_W+j];          
          asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+i)*Input_cl+j+itr*VLMAX));
          // snrt_cluster_hw_barrier();
          asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse32.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv3_v6 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 128;
  float f;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        for (int j = 0; j < FILT_W; j++){
          f = Kernel[i*FILT_W+j];          
          asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+i)*Input_cl+j+itr*VLMAX));
          // snrt_cluster_hw_barrier();
          asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse32.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv3_v7 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  // float f;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v28, v28, v28");
      asm volatile("vfsub.vv v24, v24, v24");

      float f0 = Kernel[0];
      float f1 = Kernel[1];
      float temp = In_Img[(row)*Input_cl + num_elements + 1];//1
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp));//1
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
      asm volatile("vfmacc.vf  v24, %0, v4" ::"f"(f1));

      float f2 = Kernel[2];
      float f3 = Kernel[3];
      temp = In_Img[(row)*Input_cl + num_elements + 2];//2
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));//3
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp));//2
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
      asm volatile("vfmacc.vf  v24, %0, v8" ::"f"(f2));

      float f4 = Kernel[4];
      float f6 = Kernel[6];
      temp = In_Img[(row+1)*Input_cl + num_elements + 1];//4
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));//6
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp));//4
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f6));
      asm volatile("vfmacc.vf  v24, %0, v4" ::"f"(f4));

      float f5 = Kernel[5];
      float f7 = Kernel[7];
      float f8 = Kernel[8];
      temp = In_Img[(row+1)*Input_cl + num_elements + 2];
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));//7
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp));//5
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f7));
      asm volatile("vfmacc.vf  v24, %0, v8" ::"f"(f5));
      temp = In_Img[(row+2)*Input_cl + num_elements + 2];      
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp));//8
      asm volatile("vfadd.vv v28, v28, v24");
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f8));

      asm volatile("vse32.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv3_v8 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  // float f;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v28, v28, v28");

      float f0 = Kernel[0];
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
      float temp1 = In_Img[(row)*Input_cl + num_elements + 1];//1
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp1));//1
      float f1 = Kernel[1];
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f1));
      float temp2 = In_Img[(row)*Input_cl + num_elements + 2];//2
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp2));//2
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));//3
      float f2 = Kernel[2];
      float f3 = Kernel[3];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f2));
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

      float temp4 = In_Img[(row+1)*Input_cl + num_elements + 1];//4
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp4));//4
      float f4 = Kernel[4];
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f4));
      float temp5 = In_Img[(row+1)*Input_cl + num_elements + 2];//5
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp5));//5
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));//6
      float f5 = Kernel[5];
      float f6 = Kernel[6];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f5));
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

      float temp7 = In_Img[(row+2)*Input_cl + num_elements + 1];//7
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp7));//7
      float f7 = Kernel[7];
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f7));
      float temp8 = In_Img[(row+2)*Input_cl + num_elements + 2];//8
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp8));//8
      float f8 = Kernel[8];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f8));

      asm volatile("vse32.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv3_v9 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  // float f;
  for (int row = 0; row < 1; row++){
    asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v28, v28, v28");

      float f0 = Kernel[0];
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      float temp1 = In_Img[(row)*Input_cl + num_elements + 1];//1
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp1));//1
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
      float f1 = Kernel[1];
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f1));
      float temp2 = In_Img[(row)*Input_cl + num_elements + 2];//2
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp2));//2
      float f2 = Kernel[2];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f2));


      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));//3
      float temp4 = In_Img[(row+1)*Input_cl + num_elements + 1];//4
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp4));//4
      float f3 = Kernel[3];
      float f4 = Kernel[4];
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f4));
      float temp5 = In_Img[(row+1)*Input_cl + num_elements + 2];//5
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp5));//5
      float f5 = Kernel[5];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f5));

      
      asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));//6
      float temp7 = In_Img[(row+2)*Input_cl + num_elements + 1];//7
      asm volatile("vfslide1down.vf v4, v0, %[A]" ::[A] "f"(temp7));//7
      float f6 = Kernel[6];
      float f7 = Kernel[7];
      asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));
      asm volatile("vfmacc.vf  v28, %0, v4" ::"f"(f7));
      float temp8 = In_Img[(row+2)*Input_cl + num_elements + 2];//8
      asm volatile("vfslide1down.vf v8, v4, %[A]" ::[A] "f"(temp8));//8
      float f8 = Kernel[8];
      asm volatile("vfmacc.vf  v28, %0, v8" ::"f"(f8));

      asm volatile("vse32.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));

    }
  }
}

void Conv3_v10 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  double f0 = Kernel[0];
  double f1 = Kernel[3];
  double f2 = Kernel[6];
  double temp;
  for (int row = 0; row < Input_row; row+=3){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
    //   itr++;
    // }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      // asm volatile("vfsub.vv v28, v28, v28");
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX-FILT_W+1)));
        ///////////

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX-FILT_W+1)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      }
    }
  }
}

void Conv3_v11 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  
  double f0;
  double f1;
  double f2;

  double temp;
  for (int row = 0; row < Input_row; row+=3){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
    //   itr++;
    // }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      // asm volatile("vfsub.vv v28, v28, v28");
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));


        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX-FILT_W+1)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX-FILT_W+1)));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      }
    }
  }
}

void Conv3_v12 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 64;
  
  double f0;
  double f1;
  double f2;

  double temp;
  for (int row = 0; row < Input_row; row+=3){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
    //   itr++;
    // }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      // asm volatile("vfsub.vv v28, v28, v28");
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));


        // asm volatile("vle64.v v20, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

        // asm volatile("vmv.v.v v0, v20");      
        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX-FILT_W+1)));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        // asm volatile("vle64.v v20, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        // asm volatile("vmv.v.v v0, v20");      
        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX-FILT_W+1)));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      }
    }
  }
}

void Conv_1Block (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl,
                  int row, int itr, int VLMAX, int num_elements){
  double f0;
  double f1;
  double f2;

  double temp;
  
  if(row == 0){
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
    temp = In_Img[(row)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
    
    temp = In_Img[(row)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
    ///////////
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
    temp = In_Img[(row+1)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
    f1 = Kernel[3];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

    temp = In_Img[(row+1)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
    f1 = Kernel[4];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
    f1 = Kernel[5];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
    ///////////
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
    temp = In_Img[(row+2)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
    f1 = Kernel[3];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
    f2 = Kernel[6];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

    temp = In_Img[(row+2)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
    f1 = Kernel[4];
    asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
    f2 = Kernel[7];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

    f2 = Kernel[8];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
    f1 = Kernel[5];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


    asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));

  }else{
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
    temp = In_Img[(row)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
    f1 = Kernel[3];
    asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
    f2 = Kernel[6];
    asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

    temp = In_Img[(row)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
    f1 = Kernel[4];
    asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
    f2 = Kernel[7];
    asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));


    // asm volatile("vle64.v v20, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
    f2 = Kernel[8];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
    f1 = Kernel[5];
    asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

    // asm volatile("vmv.v.v v0, v20");      
    asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX-FILT_W+1)));

    ///////////
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
    temp = In_Img[(row+1)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
    f1 = Kernel[3];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
    f2 = Kernel[6];
    asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

    temp = In_Img[(row+1)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
    f1 = Kernel[4];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
    f2 = Kernel[7];
    asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

    // asm volatile("vle64.v v20, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
    f2 = Kernel[8];
    asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
    f1 = Kernel[5];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

    // asm volatile("vmv.v.v v0, v20");      
    asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX-FILT_W+1)));

    ///////////
    asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
    temp = In_Img[(row+2)*Input_cl + num_elements + 0];
    asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

    f0 = Kernel[0];
    asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
    f1 = Kernel[3];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
    f2 = Kernel[6];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

    temp = In_Img[(row+2)*Input_cl + num_elements + 1];
    asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
    f0 = Kernel[1];
    asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
    f1 = Kernel[4];
    asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
    f2 = Kernel[7];
    asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

    f2 = Kernel[8];
    asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
    f0 = Kernel[2];
    asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
    f1 = Kernel[5];
    asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));


    asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
  }
}

void Conv3_v13 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 64;
  
  double f0;
  double f1;
  double f2;

  double temp;
  for (int row = 0; row < Input_row; row+=3){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
    //   itr++;
    // }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      Conv_1Block (In_Img, Out_Img, Kernel, ROW, COL, Stride, Input_cl, Input_row, FILT_W, vl, row, itr, VLMAX, num_elements);
      
    }
  }
}

void Conv3_v14 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 32;
  
  double f0;
  double f1;
  double f2;

  double temp;

  // int row;
  // float rem = Input_cl%FILT_W;
  asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
  while(itr*VLMAX + VLMAX <= num_elements){
    for (int row = 0; row < Input_row; row+=3){
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
    itr++;
  }
  if(itr*VLMAX < num_elements){
    for (int row = 0; row < Input_row; row+=3){
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
  }
}

void Conv3_v15 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 32;
  
  double f0;
  double f1;
  double f2;

  double temp;

  int row;
  float rem = Input_cl%FILT_W;
  asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
  while(itr*VLMAX + VLMAX <= num_elements){
    for (row = 0; row+3 < Input_row; row+=3){
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
    
    if(rem == 1){
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[6];
      asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
    }else{
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f1 = Kernel[3];
      asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
      f2 = Kernel[6];
      asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

      f1 = Kernel[4];
      asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f1 = Kernel[5];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

      ///////////
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[6];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

      asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
    }
    itr++;
  }
  if(itr*VLMAX < num_elements){
    for (row = 0; row+3 < Input_row; row+=3){
      asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));

        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf  v12, v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));
        f2 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));

        f2 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
    if(rem == 1){
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[6];
      asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
    }else{
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f1 = Kernel[3];
      asm volatile("vfmacc.vf v4 , %0, v0" ::"f"(f1));
      f2 = Kernel[6];
      asm volatile("vfmacc.vf v8 , %0, v0" ::"f"(f2));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));

      f1 = Kernel[4];
      asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f1 = Kernel[5];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

      ///////////
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[6];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

      temp = In_Img[(row+1)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
      f2 = Kernel[7];
      asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));        

      f2 = Kernel[8];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));

      asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
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


// V0 - V4  => kernel
// V5 - V9  => In_Img
// V10- V14 => In_Img Slide down / mul res
// V15- V19 => redsum
void Conv5_v2 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  for (int row=0; row < ROW; row++) {
    for (int col=0; col < COL; col++) {
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col));
      asm volatile("vle32.v v6, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col));
      asm volatile("vle32.v v7, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col));
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col));
      asm volatile("vle32.v v9, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col));


      asm volatile("vfmul.vv v5, v5, v0");
      asm volatile("vfmul.vv v6, v6, v1");
      asm volatile("vfmul.vv v7, v7, v2");
      asm volatile("vfmul.vv v8, v8, v3");
      asm volatile("vfmul.vv v9, v9, v4");

      asm volatile("vfadd.vv v5, v6, v5");
      asm volatile("vfadd.vv v7, v8, v7");
      asm volatile("vfadd.vv v5, v5, v9");
      asm volatile("vfadd.vv v5, v5, v7");
            
      asm volatile("vfredsum.vs v10 , v5, v31");
      asm volatile("vfmv.f.s %0, v10" : "=f"(Out_Img[row*COL + col]));
    }
  }
}

void Conv5_v3 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  for (int row=0; row < ROW; row++) {
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+0)*Input_cl));
      asm volatile("vle32.v v6, (%0)" :: "r"(In_Img+(row+1)*Input_cl));
      asm volatile("vle32.v v7, (%0)" :: "r"(In_Img+(row+2)*Input_cl));
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+3)*Input_cl));
      asm volatile("vle32.v v9, (%0)" :: "r"(In_Img+(row+4)*Input_cl));
    for (int col=0; col < COL; col++) {



      asm volatile("vfmul.vv v5, v5, v0");
      asm volatile("vfmul.vv v6, v6, v1");
      asm volatile("vfmul.vv v7, v7, v2");
      asm volatile("vfmul.vv v8, v8, v3");
      asm volatile("vfmul.vv v9, v9, v4");

      asm volatile("vfadd.vv v5, v6, v5");
      asm volatile("vfadd.vv v7, v8, v7");
      asm volatile("vfadd.vv v5, v5, v9");
      asm volatile("vfadd.vv v10, v5, v7");
                  
      asm volatile("vfredsum.vs v11 , v10, v31");
      asm volatile("vle32.v v5, (%0)" :: "r"(In_Img+(row+0)*Input_cl + col+1));
      asm volatile("vle32.v v6, (%0)" :: "r"(In_Img+(row+1)*Input_cl + col+1));
      asm volatile("vle32.v v7, (%0)" :: "r"(In_Img+(row+2)*Input_cl + col+1));
      asm volatile("vle32.v v8, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col+1));
      asm volatile("vle32.v v9, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col+1));

      asm volatile("vfmv.f.s %0, v11" : "=f"(Out_Img[row*COL + col]));
    }
  }
}


void Conv5_v4 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 128;
  float f;
  for (int row = 0; row < ROW; row++){
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        for (int j = 0; j < FILT_W; j++){
          f = Kernel[i*FILT_W+j];          
          asm volatile("vle32.v v0, (%0)" :: "r"(In_Img+(row+i)*Input_cl+j+itr*VLMAX));
          // snrt_cluster_hw_barrier();
          asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse32.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv5_v12 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 32;
  
  double f0;
  double f1;
  double f2;
  double f3;
  double f4;

  double temp;
  int row;
  float rem = Input_cl%FILT_W;

  asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
  while(itr*VLMAX + VLMAX <= num_elements){
    for (row = 0; row+5 < Input_row; row+=5){
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v20, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        /////////// 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

        /////////// 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
        
        /////////// 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

        /////////// 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));

        /////////// 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
    if(rem == 1){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
    }else if(rem == 2){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
   
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));      

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
    }else if(rem == 3){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
   
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));      
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
      /////////// 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));    

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

    }else{//4
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f1 = Kernel[5];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));
      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f1 = Kernel[6];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f1 = Kernel[7];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f1 = Kernel[8];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f1 = Kernel[9];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
      /////////// 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));


      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));    
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

      /////////// 3
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
      snrt_cluster_hw_barrier();//SOMETHING WRONG HERE
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
    }
    itr++;
  }
  if(itr*VLMAX < num_elements){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    for (row = 0; row+5 < Input_row; row+=5){
      if(row == 0){
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v20, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        
        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));

        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        ///////////
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));

      }else{
        /////////// 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

        temp = In_Img[(row)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

        /////////// 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

        temp = In_Img[(row+1)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
        
        /////////// 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+2)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

        /////////// 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

        temp = In_Img[(row+3)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));

        /////////// 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[15];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[16];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[17];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));
        f4 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f1));        
        f2 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f2));
        f3 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f3));
        f4 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v24" ::"f"(f4));

        f4 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f3));

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
      }
    }
    if(rem == 1){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
    }else if(rem == 2){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
   
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));      

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
    }else if(rem == 3){
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
   
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));      
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
      /////////// 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+2)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));    

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

    }else{//4
      /////////// 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));
      temp = In_Img[(row)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f1 = Kernel[5];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));
      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f1 = Kernel[6];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));
      f1 = Kernel[7];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));

      temp = In_Img[(row)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));
      f1 = Kernel[8];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f1));        
      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v16, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f4));
      f1 = Kernel[9];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));

      /////////// 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+1)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[10];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f2 = Kernel[11];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f2 = Kernel[12];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));

      temp = In_Img[(row+1)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f2 = Kernel[13];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f2));
      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v12, %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
      f2 = Kernel[14];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));        
      
      /////////// 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+2)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));


      f3 = Kernel[15];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[16];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f3 = Kernel[17];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+2)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f3 = Kernel[18];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f3));
      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v8 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));    
      f3 = Kernel[19];
      asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(f3));

      asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));

      /////////// 3
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
      temp = In_Img[(row+3)*Input_cl + num_elements + 0];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[20];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + num_elements + 1];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[21];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + num_elements + 2];
      asm volatile("vfslide1down.vf v24, v0, %[A]" ::[A] "f"(temp));

      f4 = Kernel[22];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      temp = In_Img[(row+3)*Input_cl + num_elements + 3];
      asm volatile("vfslide1down.vf v0, v24, %[A]" ::[A] "f"(temp));

      f4 = Kernel[23];
      asm volatile("vfmacc.vf  v4 , %0, v24" ::"f"(f4));

      f4 = Kernel[24];
      asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));

      asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
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


// V0 - V6  => kernel
// V7 - V13 => In_Img
// V14- V20 => In_Img Slide down / mul res
// V21- V27 => redsum
void Conv7_v2 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  asm volatile("vle32.v v5, (%0)" :: "r"(Kernel+5*FILT_W));
  asm volatile("vle32.v v6, (%0)" :: "r"(Kernel+6*FILT_W));
  for (int row=0; row < ROW; row++) {
    for (int col=0; col < COL; col++) {
      asm volatile("vsetvli %0 , %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v7 , (%0)" :: "r"(In_Img+(row+0)*Input_cl + col));
      asm volatile("vle32.v v8 , (%0)" :: "r"(In_Img+(row+1)*Input_cl + col));
      asm volatile("vle32.v v9 , (%0)" :: "r"(In_Img+(row+2)*Input_cl + col));
      asm volatile("vle32.v v10, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col));
      asm volatile("vle32.v v11, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col));
      asm volatile("vle32.v v12, (%0)" :: "r"(In_Img+(row+5)*Input_cl + col));
      asm volatile("vle32.v v13, (%0)" :: "r"(In_Img+(row+6)*Input_cl + col));


      asm volatile("vfmul.vv v7 , v7 , v0");
      asm volatile("vfmul.vv v8 , v8 , v1");
      asm volatile("vfmul.vv v9 , v9 , v2");
      asm volatile("vfmul.vv v10, v10, v3");
      asm volatile("vfmul.vv v11, v11, v4");
      asm volatile("vfmul.vv v12, v12, v5");
      asm volatile("vfmul.vv v13, v13, v6");

      asm volatile("vfadd.vv v7 , v8 , v7 ");
      asm volatile("vfadd.vv v9 , v10, v9 ");
      asm volatile("vfadd.vv v11, v12, v11");
      asm volatile("vfadd.vv v7 , v13, v7 ");
      asm volatile("vfadd.vv v9 , v11, v9 ");
      asm volatile("vfadd.vv v7 , v7 , v9 ");
            
      asm volatile("vfredsum.vs v14 , v7, v31");
      asm volatile("vfmv.f.s %0, v14" : "=f"(Out_Img[row*COL + col]));
    }
  }
}


void Conv7_v3 (float *In_Img, float *Out_Img, float  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){
  asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(FILT_W));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Kernel+0*FILT_W));
  asm volatile("vle32.v v1, (%0)" :: "r"(Kernel+1*FILT_W));
  asm volatile("vle32.v v2, (%0)" :: "r"(Kernel+2*FILT_W));
  asm volatile("vle32.v v3, (%0)" :: "r"(Kernel+3*FILT_W));
  asm volatile("vle32.v v4, (%0)" :: "r"(Kernel+4*FILT_W));
  asm volatile("vle32.v v5, (%0)" :: "r"(Kernel+5*FILT_W));
  asm volatile("vle32.v v6, (%0)" :: "r"(Kernel+6*FILT_W));
  for (int row=0; row < ROW; row++) {
    asm volatile("vle32.v v7 , (%0)" :: "r"(In_Img+(row+0)*Input_cl));
    asm volatile("vle32.v v8 , (%0)" :: "r"(In_Img+(row+1)*Input_cl));
    asm volatile("vle32.v v9 , (%0)" :: "r"(In_Img+(row+2)*Input_cl));
    asm volatile("vle32.v v10, (%0)" :: "r"(In_Img+(row+3)*Input_cl));
    asm volatile("vle32.v v11, (%0)" :: "r"(In_Img+(row+4)*Input_cl));
    asm volatile("vle32.v v12, (%0)" :: "r"(In_Img+(row+5)*Input_cl));
    asm volatile("vle32.v v13, (%0)" :: "r"(In_Img+(row+6)*Input_cl));
    for (int col=0; col < COL; col++) {
      asm volatile("vfmul.vv v7 , v7 , v0");
      asm volatile("vfmul.vv v8 , v8 , v1");
      asm volatile("vfmul.vv v9 , v9 , v2");
      asm volatile("vfmul.vv v10, v10, v3");
      asm volatile("vfmul.vv v11, v11, v4");
      asm volatile("vfmul.vv v12, v12, v5");
      asm volatile("vfmul.vv v13, v13, v6");

      asm volatile("vfadd.vv v7 , v8 , v7 ");
      asm volatile("vfadd.vv v9 , v10, v9 ");
      asm volatile("vfadd.vv v11, v12, v11");
      asm volatile("vfadd.vv v7 , v13, v7 ");
      asm volatile("vfadd.vv v9 , v11, v9 ");
      asm volatile("vfadd.vv v14 , v7 , v9 ");
            
      asm volatile("vfredsum.vs v15 , v14, v31");

      asm volatile("vle32.v v7 , (%0)" :: "r"(In_Img+(row+0)*Input_cl + col + 1));
      asm volatile("vle32.v v8 , (%0)" :: "r"(In_Img+(row+1)*Input_cl + col + 1));
      asm volatile("vle32.v v9 , (%0)" :: "r"(In_Img+(row+2)*Input_cl + col + 1));
      asm volatile("vle32.v v10, (%0)" :: "r"(In_Img+(row+3)*Input_cl + col + 1));
      asm volatile("vle32.v v11, (%0)" :: "r"(In_Img+(row+4)*Input_cl + col + 1));
      asm volatile("vle32.v v12, (%0)" :: "r"(In_Img+(row+5)*Input_cl + col + 1));
      asm volatile("vle32.v v13, (%0)" :: "r"(In_Img+(row+6)*Input_cl + col + 1));

      asm volatile("vfmv.f.s %0, v15" : "=f"(Out_Img[row*COL + col]));
    }
  }
}


void Conv7_v4 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int FILT_W, unsigned int vl){

  int itr = 0;
  int num_elements = Input_cl;
  int VLMAX = 64;
  double f;
  for (int row = 0; row < ROW; row++){
    itr = 0;
    asm volatile("vsetvli  %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){//AS WE WROTE VLMAX-2 element at the end is this line true?
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        for (int j = 0; j < FILT_W; j++){
          f = Kernel[i*FILT_W+j];
          asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+i)*Input_cl+j+itr*VLMAX));
          asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
      itr++;
    }
    if(itr*VLMAX < num_elements){//THIS ONE WORKS
      asm volatile("vsetvli  %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v16, v16, v16");
      for (int i = 0; i < FILT_W; i++){
        for (int j = 0; j < FILT_W; j++){
          f = Kernel[i*FILT_W+j];
          asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+i)*Input_cl+j+itr*VLMAX));
          asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX-FILT_W+1)));
    }
  }
}

void Conv7_v12 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 16;
  
  double f0;
  double f1;
  double f2;
  double f3;
  double f4;
  double f5;
  double f6;

  double temp;
  int row;
  float rem = Input_cl%FILT_W;

  asm volatile("vsetvli  %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(VLMAX));
  while(itr*VLMAX + VLMAX <= num_elements){
    for (row = 0; row < Input_row; row+=7){
      if(row == 0){
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v14, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v10, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v6 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));


        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));        

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        #pragma endregion        

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v2 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v14, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);
      }else{
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v14, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v10, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v10, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v6, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v6 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v2, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v2 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + itr*VLMAX +  VLMAX + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v14, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

      }
    }
    itr++;
  }
  if(itr*VLMAX < num_elements){
    asm volatile("vsetvli  %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    for (row = 0; row < Input_row; row+=7){
      if(row == 0){

        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        temp = In_Img[(row)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v14, v0, %0" ::"f"(f0));
        
        temp = In_Img[(row)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));

        temp = In_Img[(row)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));

        temp = In_Img[(row+1)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v10, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));

        temp = In_Img[(row+2)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));

        temp = In_Img[(row+3)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        #pragma endregion

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v6 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));

        temp = In_Img[(row+4)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));


        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        #pragma endregion
        
        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);
        
        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+5)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));        

        temp = In_Img[(row+5)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));

        temp = In_Img[(row+5)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        

        temp = In_Img[(row+5)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));

        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        #pragma endregion        

        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+6)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v2 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v14, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion
        // asm volatile("vse64.v v14, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

      }else{
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+0)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v14, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));

        temp = In_Img[(row+0)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+1)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));

        temp = In_Img[(row+1)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v10, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+2)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v10, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+2)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+3)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+3)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v6, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+4)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v6 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+4)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+5)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));

        temp = In_Img[(row+5)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8, %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v2, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        temp = In_Img[(row+6)*Input_cl + num_elements + 0];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v2 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 1];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 2];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 3];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 4];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));

        temp = In_Img[(row+6)*Input_cl + num_elements + 5];
        asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v2 , %0, v16" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v16" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v6 , %0, v16" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v16" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v10, %0, v16" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12, %0, v16" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v14, %0, v16" ::"f"(f6));

        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v14, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v2 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v6 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v10, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v14, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

      }
    }
  }
}

void Conv7_v13 (double *In_Img, double *Out_Img, double  *Kernel, int ROW, int COL, int Stride, int Input_cl, int Input_row, int FILT_W, unsigned int vl){

  int itr = 0;
  // int num_elements = Input_cl;
  int num_elements = COL;
  int VLMAX = 32;
  
  double f0;
  double f1;
  double f2;
  double f3;
  double f4;
  double f5;
  double f6;

  double temp;
  int row;
  float rem = Input_cl%FILT_W;

  asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
  while(itr*VLMAX + VLMAX <= num_elements){
    for (row = 0; row + 7< Input_row; row+=7){
      if(row == 0){
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v28, v0, %0" ::"f"(f0));
        
        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+1));//0
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+2));//0
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+3));//0
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 4];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+4));//0
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 5];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+5));//0
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+6));//0
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v24, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 4];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 5];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));  
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));  
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));  
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));  
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));  
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));  
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        #pragma endregion        

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);
      }else{
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v28, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v24, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion
      }
    }

    if(rem == 1){
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));


      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

    }else if(rem == 2){
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
      #pragma endregion

    }else{//rem == 4
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f3 = Kernel[21];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[28];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f3 = Kernel[22];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[29];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f3 = Kernel[23];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[30];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f3 = Kernel[24];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[31];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f3 = Kernel[25];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[32];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f3 = Kernel[26];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[33];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
      f3 = Kernel[27];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[34];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));

      f4 = Kernel[28];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
      f4 = Kernel[29];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
      f4 = Kernel[30];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
      f4 = Kernel[31];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
      f4 = Kernel[32];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
      f4 = Kernel[33];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));
      f4 = Kernel[34];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 3
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
      #pragma endregion
    }
    itr++;
  }
  if(itr*VLMAX < num_elements){
    asm volatile("vsetvli  %0, %1, e64, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    for (row = 0; row + 7 < Input_row; row+=7){
      if(row == 0){
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX));//0
        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf  v28, v0, %0" ::"f"(f0));
        
        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 1];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+1));//0
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 2];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+2));//0
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 3];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+3));//0
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 4];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+4));//0
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        // temp = In_Img[(row)*Input_cl + itr*VLMAX +  VLMAX + 5];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+5));//0
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row)*Input_cl+itr*VLMAX+6));//0
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v24, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 1];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 2];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 3];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 4];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        // temp = In_Img[(row+1)*Input_cl + itr*VLMAX +  VLMAX + 5];
        // asm volatile("vfslide1down.vf v0, v16, %[A]" ::[A] "f"(temp));
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+2)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));  
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));  
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));  
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));  
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));  
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));  
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+3)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        #pragma endregion        

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

        // asm volatile("vse64.v v28, (%0)" :: "r"(tempArr));
        // printf("temp[0] = %f\n",tempArr[0]);

      }else{
        #pragma region 0
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
        // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
        // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v28, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 1
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));

        f0 = Kernel[0];
        asm volatile("vfmul.vf   v24, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 2
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v20, v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 3
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v16 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 4
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v12 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+4)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v8, (%0)" :: "r"(Out_Img+(row-2)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 5
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v8 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v16, %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+5)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v4, (%0)" :: "r"(Out_Img+(row-1)*COL+itr*(VLMAX)));
        #pragma endregion

        #pragma region 6
        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX));
        f0 = Kernel[0];
        asm volatile("vfmul.vf   v4 , v0, %0" ::"f"(f0));
        f1 = Kernel[7];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[14];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[21];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[28];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[35];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[42];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+1));
        f0 = Kernel[1];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[8];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[15];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[22];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[29];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[36];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[43];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+2));
        f0 = Kernel[2];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[9];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[16];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[23];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[30];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[37];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[44];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+3));
        f0 = Kernel[3];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[10];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[17];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[24];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[31];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[38];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[45];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+4));
        f0 = Kernel[4];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[11];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[18];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[25];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[32];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[39];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        
        f6 = Kernel[46];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+5));
        f0 = Kernel[5];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[12];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[19];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));
        f3 = Kernel[26];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[33];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[40];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));
        f6 = Kernel[47];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));

        asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+6)*Input_cl+itr*VLMAX+6));
        f6 = Kernel[48];
        asm volatile("vfmacc.vf  v28, %0, v0" ::"f"(f6));
        f0 = Kernel[6];
        asm volatile("vfmacc.vf  v4 , %0, v0" ::"f"(f0));
        f1 = Kernel[13];
        asm volatile("vfmacc.vf  v8 , %0, v0" ::"f"(f1));        
        f2 = Kernel[20];
        asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f2));        
        f3 = Kernel[27];
        asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f3));
        f4 = Kernel[34];
        asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f4));
        f5 = Kernel[41];
        asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f5));        

        asm volatile("vse64.v v28, (%0)" :: "r"(Out_Img+(row)*COL+itr*(VLMAX)));
        #pragma endregion

      }
    }

    if(rem == 1){
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));


      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

    }else if(rem == 2){
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
      #pragma endregion

    }else{//rem == 4
      #pragma region 0
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX));
      // temp = In_Img[(row+0)*Input_cl + itr*VLMAX +  VLMAX + 0];
      // asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
      f3 = Kernel[21];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[28];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+1));
      f3 = Kernel[22];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[29];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+2));
      f3 = Kernel[23];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[30];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+3));
      f3 = Kernel[24];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[31];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+4));
      f3 = Kernel[25];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[32];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+5));
      f3 = Kernel[26];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[33];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+0)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v24, %0, v0" ::"f"(f6));
      f3 = Kernel[27];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f3));
      f4 = Kernel[34];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f4));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v24, (%0)" :: "r"(Out_Img+(row-6)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 1
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX));

      f4 = Kernel[28];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+1));
      f4 = Kernel[29];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+2));
      f4 = Kernel[30];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+3));
      f4 = Kernel[31];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+4));
      f4 = Kernel[32];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+5));
      f4 = Kernel[33];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+1)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v20, %0, v0" ::"f"(f6));
      f4 = Kernel[34];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f4));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v20, (%0)" :: "r"(Out_Img+(row-5)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 2
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX));
      f5 = Kernel[35];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+1));
      f5 = Kernel[36];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+2));
      f5 = Kernel[37];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+3));
      f5 = Kernel[38];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+4));
      f5 = Kernel[39];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+5));
      f5 = Kernel[40];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+2)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v16 , %0, v0" ::"f"(f6));
      f5 = Kernel[41];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f5));        

      asm volatile("vse64.v v16, (%0)" :: "r"(Out_Img+(row-4)*COL+itr*(VLMAX)));
      #pragma endregion

      #pragma region 3
      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX));
      f6 = Kernel[42];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+1));
      f6 = Kernel[43];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+2));
      f6 = Kernel[44];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+3));
      f6 = Kernel[45];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+4));
      f6 = Kernel[46];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+5));
      f6 = Kernel[47];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vle64.v v0, (%0)" :: "r"(In_Img+(row+3)*Input_cl+itr*VLMAX+6));
      f6 = Kernel[48];
      asm volatile("vfmacc.vf  v12 , %0, v0" ::"f"(f6));

      asm volatile("vse64.v v12, (%0)" :: "r"(Out_Img+(row-3)*COL+itr*(VLMAX)));
      #pragma endregion
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
        In_Img = (double   *)snrt_l1alloc(INP_COL*INP_COL * sizeof(double));
        snrt_dma_start_1d(In_Img, In_Img_dram, INP_COL*INP_COL * sizeof(double));

        Filter_Kern = (double   *)snrt_l1alloc(FILT_WIN*FILT_WIN * sizeof(double));
        snrt_dma_start_1d(Filter_Kern, Filter_Kern_dram, FILT_WIN*FILT_WIN * sizeof(double));
        
        Out_Img = (double   *)snrt_l1alloc(OUT_ROW*OUT_COL * sizeof(double));

        tempArr = (double   *)snrt_l1alloc(INP_COL*INP_COL * sizeof(double));
    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    // Conv3(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v2(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v3(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v4(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v5(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v7(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v8(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v9(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv3_v10(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv3_v11(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv3_v12(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv3_v13(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv3_v14(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv3_v15(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE, INP_COL, INP_COL, FILT_WIN, vl);
    // Conv5(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv5_v2(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv5_v3(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv5_v4(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv5_v12(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL,INP_COL, FILT_WIN,vl);
    // Conv7(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv7_v2(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv7_v3(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv7_v4(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL, FILT_WIN,vl);
    // Conv7_v12(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL,INP_COL, FILT_WIN,vl);
    Conv7_v13(In_Img, Out_Img, Filter_Kern, OUT_ROW, OUT_COL, STRIDE,INP_COL,INP_COL, FILT_WIN,vl);

    // End dump
    stop_kernel();

    // End timer and check if new best runtime
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    snrt_cluster_hw_barrier();
    // Display runtime
    if (cid == 0) {
        printf("The execution took %u cycles.\n", timer);
    }

    check_result(Out_Img,OUT_ROW*OUT_COL);

    snrt_cluster_hw_barrier();
    return errors;

}
