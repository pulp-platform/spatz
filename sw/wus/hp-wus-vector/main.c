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
#include "fft.h"
#include "fft.c"

#define N_DATA  512
#define DATA_SIZE 1
#define DATA_CH 8
// #define DEBUG 1

#define NTWI 2304






#if DATA_SIZE == 1
    typedef __fp16 data_t;
#else
    typedef uint32_t data_t;
#endif

#include "benchmark/benchmark.c"


#include "inc/archi_hwpe.h"
#include "inc/hal_hwpe.h"

#include "data/data.h"
#include "Golden/gold.h"

#include "data/fft_data.h"


typedef union float_hex {
  float f;
  uint32_t ui32;
} float_hex;


#define BOX_HALF_IN_FLOAT(VAR_NAME, VAL_16B)                                   \
  do {                                                                         \
    float_hex nan_boxed_val;                                                   \
    nan_boxed_val.ui32 = ((uint32_t)0xffff << 16) | VAL_16B;                   \
    VAR_NAME = nan_boxed_val.f;                                                \
  } while (0)



uint16_t  *dataIn;//Multi Channel Input Data
uint16_t  *gaussian_coef;//Gaussian Filter
uint16_t  *hilbert_coef;//Hilbert Transform
uint16_t  *tg_coef;//Time Gain Compensation
__fp16    *result;//Result
__fp16    *temp;//TEMP


//====================================
//  VECTOR FFT VARIABLES
__fp16 *buffer;
__fp16 *twiddle;

uint16_t *store_idx;
uint16_t *bitrev;
//====================================

unsigned int hwpe_cg_addr;
unsigned int hwpe_sel_addr;
unsigned int hwpe_base_addr;

int fp_check_16(__fp16 *val, const __fp16 *golden) {
    float diff = *val - *golden;
    if (diff < 0)
        diff = -diff;

    float eps = 0.15f * *golden;
    if (eps < 0)
        eps = -eps;

    return ((diff > eps) && (diff > 1e-4));
}

int check_data16(__fp16 *a,__fp16 *g,int start, int end){
    int errors = 0;
    for (int i = start; i < end; ++i){
        if (fp_check_16(&(((__fp16 *) a)[i]), &(((__fp16 *) g)[i-start]))) {
            errors ++;
            printf("error detected at index %d. Value = %x. Golden = %x \n", i, ((int16_t *) a)[i], ((int16_t *) g)[i-start]);
        }
    }
    return errors;
}

void FFT_vector(__fp16 *inp, __fp16 *buffer, __fp16 *twiddle, __fp16 *out, uint16_t *store_idx, uint16_t *bitrev, unsigned int n_samples){

    fft_sc(inp, buffer, twiddle, out, store_idx, bitrev, n_samples);
    
}

void iFFT_vector(__fp16 *inp, __fp16 *buffer, __fp16 *twiddle, __fp16 *out,uint16_t * store_idx,uint16_t * bitrev, int n_samples, unsigned int vl){
    const uint32_t scalar = 0x8000;

    for(int i = 0; i < n_samples/256;i++){
        // Load FFT result
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));//VLMAX = LMUL*VLEN/SEW =256
        asm volatile("vle16.v v0, (%0)" :: "r"((data_t *)inp + n_samples + i*256));
        // XOR (NEGATE THE COMPLEX PART)
        asm volatile("vxor.vx v0, v0, %[A]" ::[A] "r"(scalar));
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));//VLMAX = LMUL*VLEN/SEW =256
        asm volatile("vse16.v v0, (%0)" :: "r"((data_t *)inp + n_samples + i*256));
    }

    FFT_vector(inp, buffer, twiddle, out, store_idx, bitrev, n_samples);
    // //----------------------------------------------------------
    // // THE IFFT RESULT STORES IN INP [2*N_DATA:4*N_DATA]
    // //----------------------------------------------------------
    
    
    // NEGATE THE IMAG PART AND DIV BY N_DATA
    float fscalar_16;
    float fscalar_16_neg;
    BOX_HALF_IN_FLOAT(fscalar_16, 0X1800);//N_DATA =  512 => 1/N_DATA = 0x1800
    BOX_HALF_IN_FLOAT(fscalar_16_neg, 0X9800);//N_DATA = -512 => 1/N_DATA = 0x9800

    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));
    for(int i = 0; i < n_samples/256;i++){
        asm volatile("vle16.v v0, (%0)" :: "r"((data_t *)inp + 2*n_samples + i*256 ));//VLMAX = LMUL*VLEN/SEW =256
        asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(fscalar_16));// DIV (N_DATA)
        asm volatile("vse16.v v0, (%0)" :: "r"((data_t *)inp + 2*n_samples + i*256));
    }
    for(int i = 0; i < n_samples/256;i++){
        asm volatile("vle16.v v0, (%0)" :: "r"((data_t *)inp + 3*n_samples + i*256 ));//VLMAX = LMUL*VLEN/SEW =256
        asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(fscalar_16_neg));// DIV (-N_DATA)
        asm volatile("vse16.v v0, (%0)" :: "r"((data_t *)inp + 3*n_samples + i*256));
    }
}


void hp_wus_vector(int    cid   , uint16_t *dataIn , uint16_t *tg_coef  , __fp16    *result, uint16_t *hilbert_coef ,
                   __fp16 *buffer, __fp16    *twiddle, uint16_t *store_idx, uint16_t *bitrev, uint16_t *gaussian_coef, __fp16 *temp){

    if (cid == 0) {

        unsigned int vl;

//===================================================================================================
// TIME GAIN COMPENSATION

        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));//VLMAX =LMUL*VLEN/SEW = 256 or (-1)
        for(int i = 0; i < 2*N_DATA/256;i++){
            // Load DataIn
            asm volatile("vle16.v v0, (%0)" :: "r"(dataIn+ i*256));//Channel 0 
            // Load Time Gain coef.
            asm volatile("vle16.v v8, (%0)" :: "r"(tg_coef + i*256));
            // MUL
            asm volatile("vfmul.vv v16, v0, v8");
            // STORE THE RESULT
            asm volatile("vse16.v v16, (%0)" :: "r"(result + i*256));
        }
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("TIME GAIN COMPENSATION \n");
                check_data16((__fp16 *)result,(__fp16 *)TG_dataIn_GOLD,0,2*N_DATA);
            }
        #endif


//===================================================================================================
// FFT
        FFT_vector(result, buffer, twiddle, result+2*N_DATA, store_idx, bitrev, (unsigned int) N_DATA);
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("FIRST FFT\n");
                check_data16((__fp16 *)result,(__fp16 *)F_dataIn_GOLD,2*N_DATA,4*N_DATA);
            }
        #endif
//===================================================================================================
//  GAUSSIAN FILTER
        //----------------------------------------------------------        
        // VALUES COULD BE NEGATED BEFORE THE STORE
        //----------------------------------------------------------

        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));
        for(int i = 0; i < 2*N_DATA/256;i++){
            // Load FFT result
            asm volatile("vle16.v v0, (%0)" :: "r"(result + 2*N_DATA + i*256));
            asm volatile("vle16.v v8, (%0)" :: "r"(gaussian_coef + i*256));// Load Gaussian filter coef
            asm volatile("vfmul.vv v16, v0, v8");
            asm volatile("vse16.v v16, (%0)" :: "r"(result + i*256));
            asm volatile("vse16.v v16, (%0)" :: "r"(temp   + i*256));

        }
        //===========================================================================================
        // CHECK THE RESULT        
        #ifdef DEBUG
            if(cid == 0){
                printf("GAUSSIAN FILTER\n");
                check_data16((__fp16 *)result,(__fp16 *)GF_dataIn_GOLD,0,2*N_DATA);
            }
        #endif

// //===================================================================================================
// // GET A COPY OF GF DATA INTO TEMP

//         asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));
//         for(int i = 0; i < 2*N_DATA/256;i++){
//             asm volatile("vle16.v v0, (%0)" :: "r"(result + i*256));
//             asm volatile("vse16.v v0, (%0)" :: "r"(temp   + i*256));
//         }

//===================================================================================================
//IFFT

        iFFT_vector(result, buffer, twiddle, (result+2*N_DATA), store_idx, bitrev, N_DATA, vl);
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("FIRST IFFT\n");
                check_data16((__fp16 *)result,(__fp16 *)xg_GOLD,2*N_DATA,4*N_DATA);
            }
        #endif
//===================================================================================================
// NOW ON WORK ON TEMP
// HILBERT TRANSFORM

        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(256));
        for(int i = 0; i < 2*N_DATA/256;i++){
            asm volatile("vle16.v v0 , (%0)" :: "r"(temp + i*256));
            asm volatile("vle16.v v8 , (%0)" :: "r"(hilbert_coef + i*256));// Load HILBERT coef
            asm volatile("vfmul.vv v16, v0, v8");
            asm volatile("vse16.v v16, (%0)" :: "r"(temp + i*256));
        }
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("HILBERT TRANSFORM\n");            
                check_data16((__fp16 *)temp,(__fp16 *)HGF_GOLD,0,2*N_DATA);
            }
        #endif

//===================================================================================================
// SECOND INVERSE FFT INPUT DATA IN TEMP[0:2*N_DATA]

        iFFT_vector(temp, buffer, twiddle, ((__fp16 *)(temp+2*N_DATA)), store_idx, bitrev, N_DATA, vl);
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("SECOND IFFT\n");
                check_data16((__fp16 *)temp,(__fp16 *)xh_GOLD,2*N_DATA,4*N_DATA);
            }  
        #endif
//===================================================================================================
// ABS WITHOUT SQRT

//-------------------------------------------------
// ORDER OF REAL AND IMAG REVERESED IN FINAL RESULT
//-------------------------------------------------

// x(t)  = REAL PART IN RESULT[2*N_DATA:4*N_DATA]
// xh(t) = IMAG PART IN TEMP  [2*N_DATA:4*N_DATA]

        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" :   "=r"(vl) : "r"(256));
        for(int i = 0; i < N_DATA/256;i++){
            asm volatile("vle16.v v0, (%0)" :: "r"(result + 2*N_DATA + i*256));//Real x(t)
            asm volatile("vle16.v v8, (%0)" :: "r"(temp + 3*N_DATA + i*256));  // imag xh(t)
            asm volatile("vfsub.vv v0, v0, v8");
            asm volatile("vfmul.vv v0, v0, v0");                       //MUL (^2)
            asm volatile("vse16.v v0, (%0)" :: "r"(result + i*256));
        }
        for(int i = 0; i < N_DATA/256;i++){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" :   "=r"(vl) : "r"(256));
            asm volatile("vle16.v v0, (%0)" :: "r"(temp + 2*N_DATA + i*256));
            asm volatile("vfmul.vv v0, v0, v0");                       //MUL (^2)
            asm volatile("vse16.v v0, (%0)" :: "r"(result + N_DATA + i*256));
        }

        #ifdef DEBUG
            if(cid == 0){
                printf("CHECK z_temp2\n");            
                check_data16((__fp16 *)result,(__fp16 *)z_temp2_GOLD,0,2*N_DATA);
            }
        #endif
    }
}

int main() {
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    hwpe_sel_addr  = snrt_cluster_memory().end + SPATZ_CLUSTER_PERIPHERAL_HWPE_SELECT_REG_OFFSET;
    hwpe_cg_addr   = snrt_cluster_memory().end + SPATZ_CLUSTER_PERIPHERAL_FFT_HWPE_CG_ENABLE_REG_OFFSET;
    hwpe_base_addr = snrt_cluster_memory().end + SPATZ_CLUSTER_PERIPHERAL_PARAM_REG_WIDTH*1024;



    printf("start_addr: %x \n", hwpe_base_addr);

    volatile int errors = 0;
    int reorder_enable = 1;

    int offload_id;

    unsigned int timer = (unsigned int)-1;

    const uint32_t scalar = 0x80000000;// 1 << (1 or 2)*32 - 1;

    const unsigned int log2_nfft = 31 - __builtin_clz(N_DATA >> 1);

    // Core 0 programs DMA transfer of inputs
    if (cid == 0) {

        buffer = (__fp16 *)snrt_l1alloc(2 * N_DATA * sizeof(__fp16));
        snrt_dma_start_1d(buffer, buffer_dram, 2 * N_DATA * sizeof(__fp16));

        twiddle = (__fp16 *)snrt_l1alloc(2 * NTWI * sizeof(__fp16));
        snrt_dma_start_1d(twiddle, twiddle_dram, 2 * NTWI * sizeof(__fp16));

        store_idx =(uint16_t *)snrt_l1alloc(log2_nfft * (N_DATA / 2) * sizeof(uint16_t));
        snrt_dma_start_1d(store_idx, store_idx_dram, log2_nfft * (N_DATA / 2) * sizeof(uint16_t));

        bitrev = (uint16_t *)snrt_l1alloc(N_DATA/2 * sizeof(uint16_t));
        snrt_dma_start_1d(bitrev, bitrev_dram, N_DATA/2 * sizeof(uint16_t));

        dataIn = (uint16_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint16_t));
        snrt_dma_start_1d(dataIn, dataIn_dram, N_DATA*2 * sizeof(uint16_t));
        //load Time Gain coef into L1 -> real
        tg_coef = (uint16_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint16_t));
        snrt_dma_start_1d(tg_coef, TG_coef_dram, N_DATA*2 * sizeof(uint16_t));
        //load Gaussian coef into L1 -> real
        gaussian_coef = (uint16_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint16_t));
        snrt_dma_start_1d(gaussian_coef, gaussian_coef_dram, N_DATA*2 * sizeof(uint16_t));
        //load hilbert coef into L1 -> real
        hilbert_coef = (uint16_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint16_t));
        snrt_dma_start_1d(hilbert_coef, hilbert_coef_dram, N_DATA*2 * sizeof(uint16_t));
        //RESULT
        result = (__fp16 *)snrt_l1alloc(N_DATA*4 * sizeof(__fp16));
        temp = (__fp16 *)snrt_l1alloc(N_DATA*4 * sizeof(__fp16));
    snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();
    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();

    hp_wus_vector(cid   , dataIn , tg_coef  , result, hilbert_coef ,
                  buffer, twiddle, store_idx, bitrev, gaussian_coef, temp);

    
    // End dump
    stop_kernel();

    if(cid == 0){
        printf("CHECK RESULTS\n");            
        errors = check_data16((__fp16 *)result,(__fp16 *)z_temp2_GOLD,0,2*N_DATA);
    }
    snrt_cluster_hw_barrier();

    // End timer and check if new best runtime
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    // Display runtime
    if (cid == 0) {
        long unsigned int performance =
            1000 * 10 * N_DATA * 6 / 5 / timer;
        long unsigned int utilization = performance / (2 * num_cores * 8);

        printf("\n----- WUS_CORE on %d samples and %d channels -----\n", N_DATA, DATA_CH);
        printf("The execution took %u cycles.\n", timer);
        printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
            performance, utilization);
    }

    snrt_cluster_hw_barrier();
    return errors;

}
