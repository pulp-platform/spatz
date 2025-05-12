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
#define DATA_SIZE 2
#define DATA_CH 1
// #define DEBUG 1
#define NTWI 2304



#if DATA_SIZE == 1
    typedef __fp16 data_t;
#else
    typedef uint32_t data_t;
#endif


#include "inc/archi_hwpe.h"
#include "inc/hal_hwpe.h"
#include "benchmark/benchmark.c"

#include "data/data.h"
#include "golden/gold.h"

#include "data/fft_data.h"

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

uint32_t *dataIn[DATA_CH];//Multi Channel Input Data
uint32_t *gaussian_coef;  //Gaussian Filter
uint32_t *hilbert_coef;   //Hilbert Transform
uint32_t *tg_coef;        //Time Gain Compensation
float    *result;         //Result
float    *temp;           //TEMP

//====================================
//  VECTOR FFT VARIABLES
float    *buffer;
float    *twiddle;
uint16_t *store_idx;
uint16_t *bitrev;
//====================================

unsigned int hwpe_cg_addr;
unsigned int hwpe_sel_addr;
unsigned int hwpe_base_addr;

int fp_check_32(float val, const float golden) {
    float diff = val - golden;
    if (diff < 0)
        diff = -diff;

    float eps = 0.01f * golden;
    if (eps < 0)
        eps = -eps;

    return ((diff > eps) && (diff > 1e-5));
}

int check_data32(float *a,float *g,int start, int end){
    int errors = 0;
    for (int i = start; i < end; ++i){
        if (fp_check_32((((float *) a)[i]), (((float *) g)[i-start]))) {
            errors ++;
            printf("error detected at index %d. Value = %x. Golden = %x \n", i, ((uint32_t *) a)[i], ((uint32_t *) g)[i-start]);
        }
    }
    return errors;
}

void FFT_vector(float *inp, float *buffer, float *twiddle, float *out, uint16_t *store_idx, uint16_t *bitrev, unsigned int n_samples){

    fft_sc(inp, buffer, twiddle, out, store_idx, bitrev, n_samples);
    
}

void iFFT_vector(float *inp, float *buffer, float *twiddle, float *out,uint16_t * store_idx,uint16_t * bitrev, int n_samples, unsigned int vl){
    const uint32_t scalar = 0x80000000;

    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));//VLMAX = LMUL*VLEN/SEW =128
    for(int i = 0; i < n_samples/128;i++){
        // Load FFT result
        asm volatile("vle32.v v0, (%0)" :: "r"((data_t *)inp + n_samples + i*128));
        // XOR (NEGATE THE COMPLEX PART)
        asm volatile("vxor.vx v0, v0, %[A]" ::[A] "r"(scalar));
        asm volatile("vse32.v v0, (%0)" :: "r"((data_t *)inp + n_samples + i*128));
    }

    FFT_vector(inp, buffer, twiddle, out, store_idx, bitrev, n_samples);
    //----------------------------------------------------------
    // THE IFFT RESULT STORES IN INP [2*N_DATA:4*N_DATA]
    //----------------------------------------------------------
    
    
    // NEGATE THE IMAG PART AND DIV BY N_DATA
    float fscalar_32;
    float fscalar_32_neg;
    BOX_FLOAT_IN_FLOAT(fscalar_32, 0X3B000000);//N_DATA =  512 => 1/N_DATA
    BOX_FLOAT_IN_FLOAT(fscalar_32_neg, 0XBB000000);//N_DATA = -512 => 1/N_DATA

    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));
    for(int i = 0; i < n_samples/128;i++){
        asm volatile("vle32.v v0, (%0)" :: "r"((data_t *)inp + 2*n_samples + i*128 ));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(fscalar_32));// DIV (N_DATA)
        asm volatile("vse32.v v0, (%0)" :: "r"((data_t *)inp + 2*n_samples + i*128));
    }
    for(int i = 0; i < n_samples/128;i++){
        asm volatile("vle32.v v0, (%0)" :: "r"((data_t *)inp + 3*n_samples + i*128 ));//VLMAX = LMUL*VLEN/SEW =128
        asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(fscalar_32_neg));// DIV (-N_DATA)
        asm volatile("vse32.v v0, (%0)" :: "r"((data_t *)inp + 3*n_samples + i*128));
    }
}

void sp_wus_vector(int    cid   , uint32_t *dataIn , uint32_t *tg_coef  , float    *result, uint32_t *hilbert_coef ,
                   float *buffer, float    *twiddle, uint16_t *store_idx, uint16_t *bitrev, uint32_t *gaussian_coef, float *temp){

        if (cid == 0) {

        unsigned int vl;

 //===================================================================================================
 // TIME GAIN COMPENSATION

        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));//VLMAX =LMUL*VLEN/SEW = 128 or (-1)
        for(int i = 0; i < 2*N_DATA/128;i++){
            // Load DataIn
            asm volatile("vle32.v v0, (%0)" :: "r"(dataIn+ i*128));//Channel 0 
            // Load Time Gain coef.
            asm volatile("vle32.v v8, (%0)" :: "r"(tg_coef + i*128));
            // MUL
            asm volatile("vfmul.vv v16, v0, v8");
            // STORE THE RESULT
            asm volatile("vse32.v v16, (%0)" :: "r"(result + i*128));
        }
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("TIME GAIN COMPENSATION \n");
                check_data32((float *)result_,(float *)TG_dataIn_GOLD,0,2*N_DATA);
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
                check_data32((float *)result,(float *)F_dataIn_GOLD,2*N_DATA,4*N_DATA);
            }
        #endif
 //===================================================================================================
 //  GAUSSIAN FILTER ==> could be more optimize (the real and imag of gaussian coef are the same)
        //----------------------------------------------------------        
        // VALUES COULD BE NEGATED BEFORE THE STORE
        //----------------------------------------------------------

        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));
        for(int i = 0; i < 2*N_DATA/128;i++){
            // Load FFT result
            asm volatile("vle32.v v0, (%0)" :: "r"(result + 2*N_DATA + i*128));
            asm volatile("vle32.v v8, (%0)" :: "r"(gaussian_coef + i*128));// Load Gaussian filter coef
            asm volatile("vfmul.vv v16, v0, v8");
            asm volatile("vse32.v v16, (%0)" :: "r"(result + i*128));
            asm volatile("vse32.v v16, (%0)" :: "r"(temp   + i*128));// MAKE A COPY

        }
        //===========================================================================================
        // CHECK THE RESULT        
        #ifdef DEBUG
            if(cid == 0){
                printf("GAUSSIAN FILTER\n");            
                check_data32((float *)result,(float *)GF_dataIn_GOLD,0,2*N_DATA);
            }
        #endif

//  //===================================================================================================
//  // GET A COPY OF GF DATA INTO TEMP

//         // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));
//         // for(int i = 0; i < 2*N_DATA/128;i++){
//         //     asm volatile("vle32.v v16, (%0)" :: "r"(result + i*128));
//         //     asm volatile("vse32.v v16 , (%0)" :: "r"(temp   + i*128));
//         // }

 //===================================================================================================
 //IFFT

        iFFT_vector(result, buffer, twiddle, (result+2*N_DATA), store_idx, bitrev, N_DATA, vl);
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("FIRST IFFT\n");
                check_data32((float *)result,(float *)xg_GOLD,2*N_DATA,4*N_DATA);
            }
        #endif
 //===================================================================================================
 // NOW ON WORK ON TEMP
 // HILBERT TRANSFORM

        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));
        for(int i = 0; i < 2*N_DATA/128;i++){
            asm volatile("vle32.v v0 , (%0)" :: "r"(temp + i*128));
            asm volatile("vle32.v v8 , (%0)" :: "r"(hilbert_coef + i*128));// Load HILBERT coef
            asm volatile("vfmul.vv v16, v0, v8");
            asm volatile("vse32.v v16, (%0)" :: "r"(temp + i*128));
        }
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("HILBERT TRANSFORM\n");
                check_data32((float *)temp,(float *)HGF_GOLD,0,2*N_DATA);
            }
        #endif

 //===================================================================================================
 // SECOND INVERSE FFT INPUT DATA IN TEMP[0:2*N_DATA]

        iFFT_vector(temp, buffer, twiddle, ((float *)(temp+2*N_DATA)), store_idx, bitrev, N_DATA, vl);
        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("SECOND IFFT\n");            
                check_data32((float *)temp,(float *)xh_GOLD,2*N_DATA,4*N_DATA);
            }  
        #endif
 //===================================================================================================
 // ABS WITHOUT SQRT

 //-------------------------------------------------
 // ORDER OF REAL AND IMAG REVERESED IN FINAL RESULT
 //-------------------------------------------------

 // x(t)  = REAL PART IN RESULT[2*N_DATA:4*N_DATA]
 // xh(t) = IMAG PART IN TEMP  [2*N_DATA:4*N_DATA]

        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" :   "=r"(vl) : "r"(128));
        for(int i = 0; i < N_DATA/128;i++){
            asm volatile("vle32.v v0, (%0)" :: "r"(result + 2*N_DATA + i*128));//Real x (t)
            asm volatile("vle32.v v8, (%0)" :: "r"(temp   + 3*N_DATA + i*128));//imag xh(t)
            asm volatile("vfsub.vv v0, v0, v8");
            // SQUARE
            asm volatile("vfmul.vv v0, v0, v0");//MUL (^2)
            asm volatile("vse32.v v0, (%0)" :: "r"(result + i*128));
        }
        // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" :   "=r"(vl) : "r"(128));
        for(int i = 0; i < N_DATA/128;i++){
            asm volatile("vle32.v v0, (%0)" :: "r"(temp   + 2*N_DATA + i*128));
            // SQUARE
            asm volatile("vfmul.vv v0, v0, v0");//MUL (^2)
            asm volatile("vse32.v v0, (%0)" :: "r"(result +   N_DATA + i*128));
        }        

        //===========================================================================================
        // CHECK THE RESULT
        #ifdef DEBUG
            if(cid == 0){
                printf("CHECK z_temp2\n");
                check_data32((float *)result,(float *)z_temp2_GOLD,0,2*N_DATA);
            }
        #endif
    }
}

int main() {

    volatile int errors = 0;
    
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;

    const unsigned int log2_nfft = 31 - __builtin_clz(N_DATA >> 1);

    if (cid == 0) {

        buffer = (float *)snrt_l1alloc(2 * N_DATA * sizeof(float));
        snrt_dma_start_1d(buffer, buffer_dram, 2 * N_DATA * sizeof(float));

        twiddle = (float *)snrt_l1alloc(2 * NTWI * sizeof(float));
        snrt_dma_start_1d(twiddle, twiddle_dram, 2 * NTWI * sizeof(float));

        store_idx =(uint16_t *)snrt_l1alloc(log2_nfft * (N_DATA / 2) * sizeof(uint16_t));
        snrt_dma_start_1d(store_idx, store_idx_dram, log2_nfft * (N_DATA / 2) * sizeof(uint16_t));

        bitrev = (uint16_t *)snrt_l1alloc(N_DATA/2 * sizeof(uint16_t));
        snrt_dma_start_1d(bitrev, bitrev_dram, N_DATA/2 * sizeof(uint16_t));
        for (int i = 0; i < DATA_CH; i++){
            dataIn[i] = (uint32_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint32_t));
            snrt_dma_start_1d(dataIn[i], dataIn_dram[i], N_DATA*2 * sizeof(uint32_t));
        }
        tg_coef = (uint32_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint32_t));
        snrt_dma_start_1d(tg_coef, TG_coef_dram, N_DATA*2 * sizeof(uint32_t));        
    
        gaussian_coef = (uint32_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint32_t));
        snrt_dma_start_1d(gaussian_coef, gaussian_coef_dram, N_DATA*2 * sizeof(uint32_t));        

        hilbert_coef = (uint32_t *)snrt_l1alloc(N_DATA*2 * sizeof(uint32_t));
        snrt_dma_start_1d(hilbert_coef, hilbert_coef_dram, N_DATA*2 * sizeof(uint32_t));        
        //RESULT
        result = (float *)snrt_l1alloc(N_DATA*4 * sizeof(float));            

        temp = (float *)snrt_l1alloc(N_DATA*4 * sizeof(float));
    snrt_dma_wait_all();
    }


    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    
    sp_wus_vector(cid   , dataIn[0] , tg_coef  , result, hilbert_coef ,
                  buffer, twiddle, store_idx, bitrev, gaussian_coef, temp);
    
    // End dump
    stop_kernel();

    if(cid == 0){
        printf("CHECK RESULTS\n");
        errors = check_data32((float *)result,(float *)z_temp2_GOLD,0,2*N_DATA);
    }

    // End timer and check if new best runtime
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    // Display runtime
    if (cid == 0) {
        long unsigned int performance =
            1000 * 10 * N_DATA * log2_nfft * 6 / 5 / timer;
        long unsigned int utilization = performance / (2 * num_cores * 8);

        printf("\n----- WUS_VEC on %d samples and %d channels -----\n", N_DATA, DATA_CH);
        printf("The execution took %u cycles.\n", timer);
        printf("The performance is %ld OP/1000cycle (%ld%%o utilization).\n",
            performance, utilization);
    }

    snrt_cluster_hw_barrier();

    return errors;

}
