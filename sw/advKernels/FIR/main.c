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

#include "data/data.h"
#include "Golden/gold.h"

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

float *input;
float *output;
float *Filter;

float *temp;

void check_result(float *x, int r){    
    float diff = 0.0f;
    int err = 0;

    for(int i = 0; i < r; i++){  
        diff = fabs(x[i] - check[i]);
        if(diff>THRESHOLD){
            err++;
            printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, check[i], x[i], diff);
        }
    }

    if(err != 0)
        printf("TEST FAILED with %d errors!!\n", err);
    else
        printf("TEST PASSED!!\n");
}

// FilterLength <= 128
// V0 - V7  FILTER REVERSE ORDER 
// V8 - V15 SIGNAL 
// V16- V23 SIGNAL SLIDED / MUL RES
// V24      REDSUT
void FIR (float *Signal, float *Filter, float *Output,int FilterLength, int InputLength, unsigned int vl){
  int i, j, z;
  float sum=0;

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(FilterLength));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Filter));
  int sig_idx = 0;
  while(sig_idx*128+128 <= InputLength){
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));//VLMAX = LMUL*VLEN/SEW =128
    asm volatile("vle32.v v8, (%0)" :: "r"(Signal + sig_idx*128));
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(FilterLength));//VLMAX = LMUL*VLEN/SEW =128
    for(int i = 0; i < 128; i++){
      int out_idx = sig_idx*128+i;
      asm volatile("vslidedown.vx v16, v8, %[A]" ::[A] "r"(i));
      asm volatile("vfmul.vv v16, v16, v0");
      asm volatile("vfredsum.vs v24, v16, v31");
      asm volatile("vfmv.f.s %0, v24" : "=f"(Output[out_idx]));

    }
    sig_idx++;
  }
  if(sig_idx*128 < InputLength){
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(InputLength - sig_idx*128));//VLMAX = LMUL*VLEN/SEW =128
    asm volatile("vle32.v v8, (%0)" :: "r"(Signal + sig_idx*128));
    
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(FilterLength));//VLMAX = LMUL*VLEN/SEW =128
    for(int i = 0; i < InputLength - sig_idx*128 - FilterLength; i++){
      int out_idx = sig_idx*128+i;
      asm volatile("vslidedown.vx v16, v8, %[A]" ::[A] "r"(i));
      asm volatile("vfmul.vv v16, v16, v0");
      asm volatile("vfredsum.vs v24, v16, v31");
      asm volatile("vfmv.f.s %0, v24" : "=f"(Output[out_idx]));
    }
  }
}

void FIR_v2 (float *Signal, float *Filter, float *Output,int FilterLength, int InputLength, unsigned int vl){

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(FilterLength));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Filter));
  for (int i = 0; i < InputLength-FilterLength; i++){
    asm volatile("vle32.v v8, (%0)" :: "r"(Signal + i));
    asm volatile("vfmul.vv v16, v8, v0");
    asm volatile("vfredsum.vs v24, v16, v31");
    asm volatile("vfmv.f.s %0, v24" : "=f"(Output[i]));
  }
}

void FIR_v3 (float *Signal, float *Filter, float *Output,int FilterLength, int InputLength, unsigned int vl){

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(FilterLength));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vle32.v v0, (%0)" :: "r"(Filter));
  asm volatile("vle32.v v8, (%0)" :: "r"(Signal));
  for (int i = 0; i < InputLength-FilterLength-1; i++){
    asm volatile("vfmul.vv v16, v8, v0");
    asm volatile("vle32.v v8, (%0)" :: "r"(Signal+i+1));
    asm volatile("vfredsum.vs v24, v16, v31");
    asm volatile("vfmv.f.s %0, v24" : "=f"(Output[i]));
  }
  asm volatile("vfmul.vv v16, v8, v0");
  asm volatile("vfredsum.vs v24, v16, v31");
  asm volatile("vfmv.f.s %0, v24" : "=f"(Output[InputLength-FilterLength-1]));
}

void FIR_v4 (float *Signal, float *Filter, float *Output,int FilterLength, int InputLength, unsigned int vl){
    int itr = 0;
    int num_elements = InputLength-FilterLength;
    int VLMAX = 128;
    
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
      for (int i = 0; i < FilterLength; i++){
        asm volatile("vle32.v v0, (%0)" :: "r"(Signal+itr*VLMAX+i));
        float f = Filter[FilterLength-i-1];
        // snrt_cluster_hw_barrier();
        if(i == 0){
          asm volatile("vfmul.vf  v8, v0, %0" ::"f"(f));
        }else{
          asm volatile("vfmacc.vf v8, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse32.v v8, (%0)" :: "r"(Output+itr*VLMAX));
      itr++;
    }
    if(itr*VLMAX < num_elements){
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      for (int i = 0; i < FilterLength; i++){

        asm volatile("vle32.v v0, (%0)" :: "r"(Signal+itr*VLMAX+i));  
        float f = Filter[FilterLength-i-1];
        // snrt_cluster_hw_barrier();
        if(i == 0){
          asm volatile("vfmul.vf  v8, v0, %0" ::"f"(f));
        }else{
          asm volatile("vfmacc.vf v8, %0, v0" ::"f"(f));
        }
      }
      asm volatile("vse32.v v8, (%0)" :: "r"(Output+itr*VLMAX));
    }
}




void FIR_v5 (float *Signal, float *Filter, float *Output,int FilterLength, int InputLength, unsigned int vl){
    int itr = 0;
    int num_elements = InputLength-FilterLength;
    int VLMAX = 128;
    float zero = 0;
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
      asm volatile("vfsub.vv v8, v8, v8");
      asm volatile("vfsub.vv v24, v24, v24");      
      for (int i = 0; i < FilterLength; i+=2){
        asm volatile("vle32.v v0, (%0)" :: "r"(Signal+itr*VLMAX+i));
        float temp = Signal[(itr+1)*VLMAX+i];
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        float f1 = Filter[FilterLength-i-1];
        float f2 = Filter[FilterLength-(i+1)-1];
        asm volatile("vfmacc.vf v8 , %0, v0 " ::"f"(f1));
        asm volatile("vfmacc.vf v24, %0, v16" ::"f"(f2));
      }
      asm volatile("vfadd.vv v8, v24, v8");

      asm volatile("vse32.v v8, (%0)" :: "r"(Output+itr*VLMAX));
      itr++;
    }
    if(itr*VLMAX < num_elements){
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      asm volatile("vfsub.vv v8, v8, v8");
      asm volatile("vfsub.vv v24, v24, v24");      
      for (int i = 0; i < FilterLength; i+=2){
        asm volatile("vle32.v v0, (%0)" :: "r"(Signal+itr*VLMAX+i));
        // float temp = Signal[itr*VLMAX+FilterLength + i];
        float temp = Signal[num_elements + i];
        // float temp = 0;
        asm volatile("vfslide1down.vf v16, v0, %[A]" ::[A] "f"(temp));
        float f1 = Filter[FilterLength-i-1];
        float f2 = Filter[FilterLength-(i+1)-1];
        asm volatile("vfmacc.vf v8 , %0, v0 " ::"f"(f1));
        asm volatile("vfmacc.vf v24, %0, v16" ::"f"(f2));
      }
      asm volatile("vfadd.vv v8, v24, v8");

      asm volatile("vse32.v v8, (%0)" :: "r"(Output+itr*VLMAX));
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
        input = (float   *)snrt_l1alloc(LENGTH * sizeof(float));
        snrt_dma_start_1d(input, UnitImpulse_dram, LENGTH * sizeof(float));

        output = (float   *)snrt_l1alloc((LENGTH-ORDER) * sizeof(float));

        Filter = (float   *)snrt_l1alloc(ORDER * sizeof(float));
        snrt_dma_start_1d(Filter, Filter_dram, ORDER * sizeof(float));


        temp = (float   *)snrt_l1alloc(LENGTH * sizeof(float));


        // input = (float   *)snrt_l1alloc(LENGTH * sizeof(float));
        // snrt_dma_start_1d(input, UnitImpulse_dram, LENGTH * sizeof(float));

        // Filter = (float   *)snrt_l1alloc(ORDER * sizeof(float));
        // snrt_dma_start_1d(Filter, Filter_dram, ORDER * sizeof(float));

        // output = (float   *)snrt_l1alloc(LENGTH-ORDER * sizeof(float));

        // temp = (float   *)snrt_l1alloc(LENGTH * sizeof(float));




    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();

    // FIR (input, Filter, output, ORDER, LENGTH, vl);
    // FIR_v2 (input, Filter, output, ORDER, LENGTH, vl);
    // FIR_v3 (input, Filter, output, ORDER, LENGTH, vl);
    // FIR_v4 (input, Filter, output, ORDER, LENGTH, vl);
    FIR_v5 (input, Filter, output, ORDER, LENGTH, vl);
    
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

    check_result(output,LENGTH-ORDER);

    snrt_cluster_hw_barrier();
    return errors;

}
