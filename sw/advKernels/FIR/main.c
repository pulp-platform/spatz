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
        printf("input address = %x\t size = %x\n",input,LENGTH*sizeof(float));
        snrt_dma_start_1d(input, UnitImpulse_dram, LENGTH * sizeof(float));

        output = (float   *)snrt_l1alloc((LENGTH-ORDER) * sizeof(float));
        printf("output address = %x\t size = %x\n",output,(LENGTH-ORDER)*sizeof(float));

        Filter = (float   *)snrt_l1alloc(ORDER * sizeof(float));
        printf("Filter address = %x\t size = %x\n",Filter,(ORDER)*sizeof(float));
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
    printf("FIR Start\n");

    FIR (input, Filter, output, ORDER, LENGTH, vl);

    printf("FIR Done\n");
    // End dump
    stop_kernel();

    check_result(output,LENGTH-ORDER);

    snrt_cluster_hw_barrier();
    return errors;

}
