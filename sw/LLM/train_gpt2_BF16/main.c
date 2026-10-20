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

#include "/scratch2/amirkia/spatz-github/spatz/sw/spatzBenchmarks/hp-fmatmul/data/layer.h"


#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
//=====================================
#include "hp-fmatmul.c"
//=====================================
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

//=====================================
// // defines: fopenCheck, freadCheck, fcloseCheck, fseekCheck, mallocCheck
// #include "llmc/utils.h"
// // defines: tokenizer_init, tokenizer_decode, tokenizer_free
// #include "llmc/tokenizer.h"
// // defines: dataloader_init, dataloader_reset, dataloader_next_batch, dataloader_free
// #include "llmc/dataloader.h"
//=====================================

#define THRESHOLD       0.00010f
#define VLMAX 256
// typedef union float_hex {
//     float f;
//     uint32_t ui32;
//   } float_hex;
// // NaN-Box a 16-bit IEEE 754 half float in a 32-bit float
// #define BOX_HALF_IN_FLOAT(VAR_NAME, VAL_16B)                                   \
//   do {                                                                         \
//     float_hex nan_boxed_val;                                                   \
//     nan_boxed_val.ui32 = ((uint32_t)0xffff << 16) | VAL_16B;                   \
//     VAR_NAME = nan_boxed_val.f;                                                \
//   } while (0)

typedef union {
    float f32;
    __fp16 f16;
} fp_bits_t;


void check_result(__fp16 *x,__fp16 *ref, int r){    
    float diff = 0.0f;
    int err = 0;

    for(int i = 0; i < r; i++){
        // printf("i = %d\n",i);
        diff = fabs((float)(x[i] - ref[i]));
        // diff = fabs((float)(x[i] - ((__fp16 *)ref)[i]));
        if(diff>THRESHOLD && (diff > (fabs((float)ref[i])/100))){
            err++;
            printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, (float)ref[i], (float)x[i], diff);
            // printf("Error at index %d:\t expected %f\t real %f\t error %f\n", i, (float)(((__fp16 *)ref)[i]), (float)x[i], diff);
        }
    }

    if(err != 0)
        printf("TEST FAILED with %d errors!!\n", err);
    else
        printf("TEST PASSED!!\n");
}

union {
    __fp16 f;
    uint16_t i;
} result;
void fast_exp_BF16(__fp16* inp, __fp16* out, int memload, int vnum, int lmul, int size, unsigned int vl){//size <= vlmax
    fp_bits_t coef;
    fp_bits_t bias;

    coef.f16 = 184.66496523f;
    bias.f16 = 16249.0f;

    if(memload){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle16.v v24, (%0)" :: "r"(inp));
        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));
        // asm volatile("vse16.v v24, (%0)" :: "r"(out));
        // for (int i = 0; i < size; i++){
        //     result.f = out[i];
        //     result.i = (uint16_t)result.f;
        //     out[i] = result.f;
        // }
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vse16.v v24, (%0)" :: "r"(out));

    }else {
        if(vnum == 0){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
            asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(bias.f32));
            asm volatile("vfcvt.rtz.xu.f.v v0, v0");
            // asm volatile("vse32.v v0, (%0)" :: "r"(out));
            // for (int i = 0; i < size; i++){
            //     result.f = out[i];
            //     result.i = (uint32_t)result.f;
            //     out[i] = result.f;
            // }
            // asm volatile("vle32.v v0, (%0)" :: "r"(out));
        }
    }
}

// SIZE LESS THAN VLMAX/2 TO ONLY WORKS ON V24 to V31
// IN THIS CASE LMUL=4 
// However effect 2xsize of out
void fast_cosh_v1(float* inp, float* out, float* temp, int size, unsigned int vl){
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(size));
    asm volatile("vle32.v v28, (%0)" :: "r"(inp));
    
    asm volatile("vfmul.vf v24, v28, %[A]" ::[A] "f"(-1.0f));

    asm volatile("vfmul.vf v28, v28, %[A]" ::[A] "f"(12102203.0f));
    asm volatile("vfadd.vf v28, v28, %[A]" ::[A] "f"(1064866805.0f));

    asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
    asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));

    asm volatile("vse32.v v28, (%0)" :: "r"(temp));
    asm volatile("vse32.v v24, (%0)" :: "r"(temp+size));
    for (int i = 0; i < size*2; i++){
        result.f = temp[i];
        result.i = (uint32_t)result.f;
        temp[i] = result.f;
    }

    asm volatile("vle32.v v24, (%0)" :: "r"(temp));
    asm volatile("vle32.v v28, (%0)" :: "r"(temp+size));
    asm volatile("vfadd.vv v24, v24, v28");
    asm volatile("vfdiv.vf    v24, v24, %[A]" ::[A] "f"(2.0f));
    asm volatile("vse32.v v24, (%0)" :: "r"(out));
}

void fast_cosh_v2(__fp16* inp, __fp16* out, __fp16* temp, int memload ,int vnum,int size, unsigned int vl){
    fp_bits_t coef;
    fp_bits_t bias;
    fp_bits_t temp_1;
    fp_bits_t temp2;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;
    temp_1.f16 = -1.0f;
    temp2.f16 = 2.0f;

    if(memload){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle16.v v24, (%0)" :: "r"(inp));
        
        asm volatile("vfmul.vf v16, v24, %[A]" ::[A] "f"(temp_1.f32));

        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

        // asm volatile("vse32.v v24, (%0)" :: "r"(temp));
        // asm volatile("vse32.v v16, (%0)" :: "r"(temp+size));
        // for (int i = 0; i < size*2; i++){
        //     result.f = temp[i];
        //     result.i = (uint32_t)result.f;
        //     temp[i] = result.f;
        // }
        // asm volatile("vle32.v v16, (%0)" :: "r"(temp));
        // asm volatile("vle32.v v24, (%0)" :: "r"(temp+size));
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");

        asm volatile("vfadd.vv v16, v16, v24");
        asm volatile("vfdiv.vf    v16, v16, %[A]" ::[A] "f"(temp2.f32));
        asm volatile("vse16.v v16, (%0)" :: "r"(out));
    } else if(vnum == 8){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        // asm volatile("vle32.v v8, (%0)" :: "r"(inp));
        
        asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(temp_1.f32));

        asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(bias.f32));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

        // asm volatile("vse32.v v8, (%0)" :: "r"(temp));
        // asm volatile("vse32.v v16, (%0)" :: "r"(temp+size));
        // for (int i = 0; i < size*2; i++){
        //     result.f = temp[i];
        //     result.i = (uint32_t)result.f;
        //     temp[i] = result.f;
        // }
        // asm volatile("vle32.v v16, (%0)" :: "r"(temp));
        // asm volatile("vle32.v v8, (%0)" :: "r"(temp+size));
        asm volatile("vfcvt.rtz.xu.f.v v16, v16");
        asm volatile("vfcvt.rtz.xu.f.v v8 , v8");


        asm volatile("vfadd.vv v16, v16, v8");
        asm volatile("vfdiv.vf v8, v16, %[A]" ::[A] "f"(temp2.f32));
        // asm volatile("vse32.v v16, (%0)" :: "r"(out));
    }
}

// SIZE LESS THAN VLMAX/2 TO ONLY WORKS ON V24 to V31
// IN THIS CASE LMUL=4 
// in this case there is no need for temp and we can go only with out.
void fast_tanh_v1(float* inp, float* out, float* temp, int memload ,int vnum,int size, unsigned int vl){
    if(memload){
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle32.v v28, (%0)" :: "r"(inp));
        asm volatile("vfmul.vf v28, v28, %[A]" ::[A] "f"(2.0f));

        asm volatile("vfmul.vf v28, v28, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v28, v28, %[A]" ::[A] "f"(1064866805.0f));

        // asm volatile("vse32.v v28, (%0)" :: "r"(temp));
        // for (int i = 0; i < size; i++){
        //     result.f = temp[i];
        //     result.i = (uint32_t)result.f;
        //     temp[i] = result.f;
        // }
        // asm volatile("vle32.v v28, (%0)" :: "r"(temp));

        asm volatile("vfcvt.rtz.xu.f.v v28, v28");

        asm volatile("vfadd.vf v24, v28, %[A]" ::[A] "f"(1.0f));
        asm volatile("vfsub.vf v28, v28, %[A]" ::[A] "f"(1.0f));

        asm volatile("vfdiv.vv v28, v28, v24");
        asm volatile("vse32.v v28, (%0)" :: "r"(out));
    } else {
        if(vnum == 0){

        }else if(vnum == 8){
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vle32.v v12, (%0)" :: "r"(inp));
            asm volatile("vfmul.vf v12, v8, %[A]" ::[A] "f"(2.0f));
    
            asm volatile("vfmul.vf v12, v12, %[A]" ::[A] "f"(12102203.0f));
            asm volatile("vfadd.vf v12, v12, %[A]" ::[A] "f"(1064866805.0f));
    
            // asm volatile("vse32.v v12, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v12, (%0)" :: "r"(temp));

            asm volatile("vfcvt.rtz.xu.f.v v12, v12");
            
            asm volatile("vfadd.vf v8 , v12, %[A]" ::[A] "f"(1.0f));
            asm volatile("vfsub.vf v12, v12, %[A]" ::[A] "f"(1.0f));
    
            asm volatile("vfdiv.vv v8, v12, v8");
            // asm volatile("vse32.v v28, (%0)" :: "r"(out));            
        }else if(vnum == 16){
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vle32.v v12, (%0)" :: "r"(inp));
            asm volatile("vfmul.vf v20, v16, %[A]" ::[A] "f"(2.0f));
    
            asm volatile("vfmul.vf v20, v20, %[A]" ::[A] "f"(12102203.0f));
            asm volatile("vfadd.vf v20, v20, %[A]" ::[A] "f"(1064866805.0f));
    
            // asm volatile("vse32.v v20, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v20, (%0)" :: "r"(temp));

            asm volatile("vfcvt.rtz.xu.f.v v20, v20");
            

            asm volatile("vfadd.vf v16 , v20, %[A]" ::[A] "f"(1.0f));
            asm volatile("vfsub.vf v20, v20, %[A]" ::[A] "f"(1.0f));
    
            asm volatile("vfdiv.vv v16, v20, v16");   
        }
    }
}

void fast_tanh_v2(__fp16* inp, __fp16* out, __fp16* temp, int memload ,int vnum,int size, unsigned int vl){
    fp_bits_t coef;
    fp_bits_t bias;
    fp_bits_t temp1;
    fp_bits_t temp_1;
    fp_bits_t temp2;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;
    temp_1.f16 = -1.0f;
    temp1.f16 = 1.0f;
    temp2.f16 = 2.0f;
    if(memload){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle16.v v24, (%0)" :: "r"(inp));
        asm volatile("vfmul.vf v16, v24, %[A]" ::[A] "f"(temp2.f32));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

        // asm volatile("vse32.v v28, (%0)" :: "r"(temp));
        // for (int i = 0; i < size; i++){
        //     result.f = temp[i];
        //     result.i = (uint32_t)result.f;
        //     temp[i] = result.f;
        // }
        // asm volatile("vle32.v v28, (%0)" :: "r"(temp));

        asm volatile("vfcvt.rtz.xu.f.v v16, v16");

        asm volatile("vfadd.vf v24, v16, %[A]" ::[A] "f"(temp1.f32));
        asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(temp1.f32));
        asm volatile("vfdiv.vv v24, v16, v24");
        asm volatile("vse16.v v24, (%0)" :: "r"(out));
        

    } else {
        if(vnum == 0){

        }else if(vnum == 8){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
            asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(temp2.f32));
    
            asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));
    
            // asm volatile("vse32.v v16, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v16, (%0)" :: "r"(temp));
            asm volatile("vfcvt.rtz.xu.f.v v16, v16");

            
            asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(temp1.f32));
            asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(temp1.f32));
    
            asm volatile("vfdiv.vv v8, v16, v8");
            // asm volatile("vse32.v v28, (%0)" :: "r"(out));            
        }else if(vnum == 16){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vle32.v v12, (%0)" :: "r"(inp));
            asm volatile("vfmul.vf v24, v16, %[A]" ::[A] "f"(temp2.f32));
    
            asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));
    
            // asm volatile("vse32.v v24, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v24, (%0)" :: "r"(temp));
            asm volatile("vfcvt.rtz.xu.f.v v24, v24");

    
            asm volatile("vfadd.vf v16, v24, %[A]" ::[A] "f"(temp1.f32));
            asm volatile("vfsub.vf v24, v24, %[A]" ::[A] "f"(temp1.f32));
    
            asm volatile("vfdiv.vv v16, v24, v16");   
        }
    }
}

void fast_tanh_v3(__fp16* inp, __fp16* out, __fp16* temp, int memload ,int vnum,int size, unsigned int vl){
    fp_bits_t coef;
    fp_bits_t bias;
    fp_bits_t temp1;
    fp_bits_t temp_1;
    fp_bits_t temp2;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;
    temp_1.f16 = -1.0f;
    temp1.f16 = 1.0f;
    temp2.f16 = 2.0f;
    if(memload){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle16.v v24, (%0)" :: "r"(inp));
        asm volatile("vfmul.vf v16, v24, %[A]" ::[A] "f"(temp_1.f32));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

        asm volatile("vfcvt.rtz.xu.f.v v16, v16");

        //*************/
        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(temp_1.f32));
        //*************/

        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));

        asm volatile("vfcvt.rtz.xu.f.v v24, v24");

        // asm volatile("vfadd.vv v8 , v16, v24");
        // asm volatile("vfsub.vv v16, v24, v16");
        // asm volatile("vfdiv.vv v24, v16, v8");
        // asm volatile("vse16.v v24, (%0)" :: "r"(out));

        asm volatile("vfadd.vv v16, v24, v16");
        asm volatile("vse16.v v16, (%0)" :: "r"(out));

    } else {
        if(vnum == 0){

        }else if(vnum == 8){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
            asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(temp2.f32));
    
            asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));
    
            // asm volatile("vse32.v v16, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v16, (%0)" :: "r"(temp));
            asm volatile("vfcvt.rtz.xu.f.v v16, v16");

            
            asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(temp1.f32));
            asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(temp1.f32));
    
            asm volatile("vfdiv.vv v8, v16, v8");
            // asm volatile("vse32.v v28, (%0)" :: "r"(out));            
        }else if(vnum == 16){
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vle32.v v12, (%0)" :: "r"(inp));
            asm volatile("vfmul.vf v24, v16, %[A]" ::[A] "f"(temp2.f32));
    
            asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));
    
            // asm volatile("vse32.v v24, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v24, (%0)" :: "r"(temp));
            asm volatile("vfcvt.rtz.xu.f.v v24, v24");

    
            asm volatile("vfadd.vf v16, v24, %[A]" ::[A] "f"(temp1.f32));
            asm volatile("vfsub.vf v24, v24, %[A]" ::[A] "f"(temp1.f32));
    
            asm volatile("vfdiv.vv v16, v24, v16");   
        }
    }
}


int verify_matrix(__fp16 *matrix, const __fp16 *checksum,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    float sum = 0;
    for (unsigned int j = 0; j < num_columns; ++j) {
      sum += (float)matrix[i * num_columns + j];
    }

    float diff = sum - (float)checksum[i];
    if (diff < 0)
      diff = -diff;

    float eps;
    if ((num_rows == 128) && (num_columns == 128)) {
      if (i == 75) {
        diff = 0; // skip check on this element (catastrophic cancellation in
                  // the last addition producing a large relative error)
        eps = 1;
      } else {
        eps = 0.15f * (float)checksum[i];
      }
    } else {
      eps = 0.05f * (float)checksum[i];
    }
    if (eps < 0)
      eps = -eps;

    if (diff > eps) {
      return i == 0 ? -1 : (int)i;
    }
  }
  return 0;
}



// ----------------------------------------------------------------------------
// all the individual layers' forward and backward passes
// B = batch_size, T = sequence_length, C = channels, V = vocab_size

void encoder_forward(__fp16* out,
    int* inp, __fp16* wte, __fp16* wpe,
    int B, int T, int C,
    unsigned int vl) {
    // out is (B,T,C). At each position (b,t), a C-dimensional vector summarizing token & position
    // inp is (B,T) of integers, holding the token ids at each (b,t) position
    // wte is (V,C) of token embeddings, short for "weight token embeddings"
    // wpe is (maxT,C) of position embeddings, short for "weight positional embedding"
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the output position in out[b,t,:]
            __fp16* out_bt = out + b * T * C + t * C;
            // get the index of the token at inp[b, t]
            int ix = inp[b * T + t];
            // seek to the position in wte corresponding to the token
            __fp16* wte_ix = wte + ix * C;
            // seek to the position in wpe corresponding to the position
            __fp16* wpe_t = wpe + t * C;
            // add the two vectors and store the result in out[b,t,:]

            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl     -= vl;
                asm volatile("vle16.v v0 , (%0)" :: "r"(wte_ix));
                wte_ix  += vl;
                asm volatile("vle16.v v8 , (%0)" :: "r"(wpe_t ));
                wpe_t   += vl;
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse16.v v16, (%0)" :: "r"(out_bt));
                out_bt  += vl;

                // wte_ix  += vl;
                // wpe_t   += vl;
                // out_bt  += vl;
                // avl     -= vl;
            } while (avl > 0);
        
        }
    }
}

void encoder_backward(__fp16* dwte, __fp16* dwpe,
                      __fp16* dout, int* inp,
                      int B, int T, int C,
                      unsigned int vl) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            __fp16* dout_bt = dout + b * T * C + t * C;
            int ix = inp[b * T + t];
            __fp16* dwte_ix = dwte + ix * C;
            __fp16* dwpe_t = dwpe + t * C;

            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl     -= vl;                

                asm volatile("vle16.v v0 , (%0)" :: "r"(dwte_ix));
                // dwte_ix  += vl;

                asm volatile("vle16.v v16, (%0)" :: "r"(dout_bt));
                dout_bt  += vl;

                asm volatile("vfadd.vv v0, v0, v16");

                asm volatile("vle16.v v8 , (%0)" :: "r"(dwpe_t ));
                // dwpe_t   += vl;

                asm volatile("vse16.v v0 , (%0)" :: "r"(dwte_ix));
                dwte_ix  += vl;

                asm volatile("vfadd.vv v8, v8, v16");

                asm volatile("vse16.v v8 , (%0)" :: "r"(dwpe_t ));
                dwpe_t   += vl;

            } while (avl > 0);
        }        
    }
}


void layernorm_forward(__fp16* out, __fp16* mean, __fp16* rstd,
                       __fp16* inp, __fp16* weight, __fp16* bias, __fp16* temp,
                       fp_bits_t* m, fp_bits_t* v, fp_bits_t* s,
                       int B, int T, int C,
                       unsigned int vl) {
    // reference: https://pytorch.org/docs/stable/generated/torch.nn.LayerNorm.html
    // both inp and out are (B,T,C) of the activations
    // mean and rstd are (B,T) buffers, to be used later in backward pass
    // at each position (b,t) of the input, the C-dimensional vector
    // of activations gets normalized, then scaled and shifted
    float eps = 1e-5f;
    // fp_bits_t m;
    // fp_bits_t v;
    // __fp16 m,v;
    float t0;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the input position inp[b,t,:]
            __fp16* x = inp + b * T * C + t * C;
            // calculate the mean

            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;

                asm volatile("vle16.v v0 , (%0)" :: "r"(x));
                x += vl;
                
                asm volatile("vfredsum.vs v8 , v0 , v31");
                asm volatile("vfmv.f.s %0 , v8" : "=f"(m->f32));//!!!!
                asm volatile("vfmv.s.f v31, %0" : "=f"(m->f32));//!!!!
            } while (avl > 0);
            
            m->f16 = m->f16/C;
            // cache the mean for the backward pass later
            mean[b * T + t] = m->f16;//I MOVE THIS HERE OTHERWISE DOES NOT WORK

            // calculate the variance (without any bias correction)
            v->f16 = 0.0f;
            
            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            avl = C;

            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;

                asm volatile("vle16.v v0 , (%0)" :: "r"(x));
                x += vl;

                asm volatile("vfsub.vf v8, v0, %[A]" ::[A] "f"(m->f32));
                asm volatile("vfmul.vv v8, v8, v8");
                asm volatile("vfredsum.vs v16 , v8 , v31");
                asm volatile("vfmv.f.s %0 , v16" : "=f"(v->f32));//!!!!
                asm volatile("vfmv.s.f v31, %0 " : "=f"(v->f32));//!!!!

            } while (avl > 0);
            



            v->f16 = v->f16/C;
            s->f16 = 1.0f / sqrtf(v->f16 + eps);//I CHANGED THIS LINE
            rstd[b * T + t] = s->f16;
            // cache the rstd for the backward pass later
            // seek to the output position in out[b,t,:]
            __fp16* out_bt = out + b * T * C + t * C;
            x = inp + b * T * C + t * C;
            __fp16* weight_ptr = weight;
            __fp16* bias_ptr = bias;
            avl = C;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v v0  , (%0)" :: "r"(x      ));
                x += vl;
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(m->f32));
                
                asm volatile("vle16.v v8  , (%0)" :: "r"(weight_ptr ));
                weight_ptr += vl;
                
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(s->f32));
                // asm volatile("vse16.v v0 , (%0)" :: "r"(out_bt));
                asm volatile("vle16.v v16 , (%0)" :: "r"(bias_ptr   ));
                bias_ptr += vl;

                asm volatile("vfmul.vv v8 , v0, v8");

                asm volatile("vfadd.vv v16, v8, v16");

                asm volatile("vse16.v v16 , (%0)" :: "r"(out_bt));
                out_bt += vl;                
            } while (avl > 0);
            
        }
    }
}


void layernorm_backward(__fp16* dinp, __fp16* dweight, __fp16* dbias,
                        __fp16* dout, __fp16* inp, __fp16* weight, __fp16* mean, __fp16* rstd,
                        fp_bits_t* dnorm_mean, fp_bits_t* dnorm_norm_mean,
                        fp_bits_t* mean_bt, fp_bits_t* rstd_bt,
                        int B, int T, int C,
                        unsigned int vl) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            __fp16* dout_bt = dout + b * T * C + t * C;
            __fp16* inp_bt  = inp  + b * T * C + t * C;
            __fp16* dinp_bt = dinp + b * T * C + t * C;
            mean_bt->f16 = mean[b * T + t];
            rstd_bt->f16 = rstd[b * T + t];

            // first: two reduce operations
            dnorm_mean->f32 = 0.0f;
            dnorm_norm_mean->f32 = 0.0f;

            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            asm volatile("vfsub.vv v30,v30,v30");

            __fp16* weight_ptr = weight;

            int avl = C;

            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v v0  , (%0)" :: "r"(inp_bt));
                inp_bt += vl;
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(mean_bt->f32));

                asm volatile("vle16.v v8  , (%0)" :: "r"(weight_ptr));
                weight_ptr += vl;

                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(rstd_bt->f32));

                asm volatile("vle16.v v16 , (%0)" :: "r"(dout_bt));
                dout_bt += vl;

                asm volatile("vfmul.vv v8 , v8, v16");
                
                asm volatile("vfredsum.vs v24 , v8  , v31");
                asm volatile("vfmv.f.s %0 , v24" : "=f"(dnorm_mean->f32));
                // asm volatile("vfmv.s.f v31, %0"  : "=f"(dnorm_mean->f32));

                asm volatile("vfmul.vv    v16 , v8  , v0");
                asm volatile("vfredsum.vs v25 , v16 , v30");
                asm volatile("vfmv.f.s %0 , v25" : "=f"(dnorm_norm_mean->f32));
                // asm volatile("vfmv.s.f v30, %0"  : "=f"(dnorm_norm_mean->f32));
 
                asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfmv.v.f v31, %[A]" ::[A] "f"(dnorm_mean->f32));
                asm volatile("vfmv.v.f v30, %[A]" ::[A] "f"(dnorm_norm_mean->f32));
                

            } while (avl > 0);
            

            dnorm_mean->f16 = dnorm_mean->f16 / C;
            dnorm_norm_mean->f16 = dnorm_norm_mean->f16 / C;

            weight_ptr = weight;
            __fp16 *dweight_ptr = dweight;
            __fp16 *dbias_ptr = dbias;
            dout_bt = dout + b * T * C + t * C;
            inp_bt  = inp  + b * T * C + t * C;

            avl = C;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v   v0 , (%0)" :: "r"(inp_bt));
                inp_bt += vl;
                asm volatile("vfsub.vf  v0 , v0, %[A]" ::[A] "f"(mean_bt->f32));
                
                asm volatile("vle16.v   v8 , (%0)" :: "r"(dout_bt));
                dout_bt += vl;

                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(rstd_bt->f32));

                asm volatile("vle16.v   v16, (%0)" :: "r"(dbias_ptr));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse16.v   v16, (%0)" :: "r"(dbias_ptr));
                dbias_ptr += vl;

                asm volatile("vle16.v   v16, (%0)" :: "r"(dweight_ptr));
                asm volatile("vfmacc.vv v16, v8, v0");
                asm volatile("vse16.v   v16, (%0)" :: "r"(dweight_ptr));
                dweight_ptr += vl;

                asm volatile("vle16.v   v16, (%0)" :: "r"(weight_ptr));
                asm volatile("vfmul.vv  v8 , v8, v16");
                weight_ptr += vl;
                //---------------------------------------------------------------
                asm volatile("vle16.v   v16, (%0)" :: "r"(dinp_bt));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(dnorm_norm_mean->f32));
                asm volatile("vfsub.vf  v8 , v8, %[A]" ::[A] "f"(dnorm_mean->f32));
                asm volatile("vfsub.vv  v8 , v8, v0");
                asm volatile("vfmul.vf  v8 , v8, %[A]" ::[A] "f"(rstd_bt->f32));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse16.v   v16, (%0)" :: "r"(dinp_bt));
                dinp_bt += vl;
            } while (avl > 0);
        }
    }
}

void matmul_forward_naive(__fp16* out,
                        const __fp16* inp, const __fp16* weight, __fp16* weightT, const __fp16* bias,
                        int B, int T, int C, int OC,
                        unsigned int vl) {

    // THIS FUNCTION IN THE LLM.C MULTIPLY INP BY TRANSPOSED OF WEIGHT.
    // SO FIRST TRANSPOSED THE WEIGHT IN WEIGHTT AND THEN USE SPATZ MATHMUL.
    // for (int i = 0; i < OC; i++){
    //     for (int j = 0; j < C; j++){
    //         weightT[j*OC+i] = weight[i*C+j];
    //     }
    // }

    matmul_8xVL(out, inp, weight, 0, B*T, C, OC, 0, OC);

    // IF THE MATMUL SUPPORTS GEMM WE SHOULD FIRST INITIAL OUT WITH BIAS AND THEN USE MATHMUL.
    // NOW JUST MATMUL AND THEN ADDING BIAS
    // for (int b = 0; b < B; b++) {
    //     for (int t = 0; t < T; t++) {
    //         int bt = b * T + t;
    //         for (int o = 0; o < OC; o++) {
    //             __fp16 val = (bias != NULL) ? bias[o] : 0.0f;
    //             out[bt * OC + o] += val;
    //         }
    //     }
    // }

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            __fp16* out_bt = out + (b * T + t)*OC;
            __fp16* bias_ptr = bias;
            int avl = OC;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v v0 , (%0)" :: "r"(out_bt));
                asm volatile("vle16.v v8 , (%0)" :: "r"(bias_ptr));
                bias_ptr += vl;
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse16.v v16, (%0)" :: "r"(out_bt));
                out_bt += vl;
            } while (avl > 0);
        }
    }     
}


void matmul_forward_naive_res(__fp16* out, const __fp16* residual,
                        const __fp16* inp, const __fp16* weight, __fp16* weightT, const __fp16* bias,
                        int B, int T, int C, int OC,
                        unsigned int vl) {
    // Fuses matmul_forward + residual_forward, i.e. the sequence
    //     matmul_forward(tmp, inp, weight, bias, B,T,C,OC);
    //     residual_forward(out, residual, tmp, B*T*C);
    // train_gpt2.c chains for both skip connections in a block (attn-proj and mlp-proj).
    // Since the matmul's raw output is only ever used to be summed with `residual`, `out`
    // does double duty: matmul_4xVL writes the raw product into it, then this function adds
    // bias and `residual` in place -- no separate "l_attproj"/"l_fcproj"-style scratch buffer
    // is needed. Requires OC == C (the residual connection only makes sense when the matmul's
    // output width matches the residual's width).

    matmul_4xVL(out, inp, weight, 0, B*T, C, OC, 0, OC);

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            __fp16* out_bt = out + (b * T + t)*OC;
            const __fp16* res_bt = residual + (b * T + t)*OC;

            int avl = OC;
            do{
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v v0 , (%0)" :: "r"(out_bt));
                asm volatile("vle16.v v8 , (%0)" :: "r"(bias  ));
                bias += vl;
                asm volatile("vfadd.vv v0 , v0, v8");
                asm volatile("vle16.v v16, (%0)" :: "r"(res_bt));
                res_bt += vl;
                asm volatile("vfadd.vv v0 , v0, v16");
                asm volatile("vse16.v v0 , (%0)" :: "r"(out_bt));
                out_bt += vl;
            } while (avl > 0);
            
        }
    }
}



void matmul_backward(__fp16* dinp, __fp16* dweight, __fp16* dbias,
                    const __fp16* dout, __fp16* doutT, const __fp16* inp, const __fp16* weight,
                    fp_bits_t* temp,
                    int B, int T, int C, int OC,
                    unsigned int vl) {
    // most of the running time is spent here and in matmul_forward
    // this backward could be done in a single "round" of loops
    // but that doesn't afford an efficient parallelization strategy

    // IF THE MATMUL SUPPORTS GEMM WE SHOULD FIRST INITIAL OUT WITH BIAS AND THEN USE MATHMUL.
    // NOW JUST MATMUL AND THEN ADDING BIAS
    matmul_8xVL(dinp, dout, weight, 0, B*T, OC, C, 0, C);

    for (int i = 0; i < B*T; i++){
        __fp16* src = dout + i * OC;
        __fp16* dst = doutT + i;

        int avl = OC;
        do{
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
            avl -= vl;
            asm volatile("vle16.v v0  , (%0)" :: "r"(src));
            src += vl;
            asm volatile("vsse16.v v0, (%0), %1" ::"r"(dst), "r"(B*T * sizeof(__fp16)));            
            dst += vl*B*T;

        } while (avl > 0);
    }


    matmul_8xVL(dweight, doutT, inp, 0, OC, B*T, C, 0, C);

    int itr = 0;
    int num_elements = OC;
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle16.v v8  , (%0)" :: "r"(dbias+itr*VLMAX));
        for (int i = 0; i < B*T; i++) {
                float *doutTaddress = dout + i*OC;
                asm volatile("vle16.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
                asm volatile("vfadd.vv    v8 , v8, v0");
        }
        asm volatile("vse16.v     v8 , (%0)" :: "r"(dbias+itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle16.v v8  , (%0)" :: "r"(dbias+itr*VLMAX));
        for (int i = 0; i < B*T; i++) {
                float *doutTaddress = dout + i*OC;
                asm volatile("vle16.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
                asm volatile("vfadd.vv    v8 , v8, v0");
        }
        asm volatile("vse16.v     v8 , (%0)" :: "r"(dbias+itr*VLMAX));
    }

    // int avl = OC;
    // do{
    //     asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
    //     avl -= vl;
    //     asm volatile("vle16.v v8  , (%0)" :: "r"(dbias));
    //     for (int i = 0; i < B*T; i++) {
    //             float *doutTaddress = dout + i*OC;
    //             asm volatile("vle16.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
    //             asm volatile("vfadd.vv    v8 , v8, v0");
    //     }
    //     asm volatile("vse16.v     v8 , (%0)" :: "r"(dbias));
    //     dbias += vl;
    // } while (avl > 0);
}

void matmul_backward_res(__fp16* dinp, __fp16* dinp_t, __fp16* dresidual, __fp16* dweight, __fp16* dweight_t, __fp16* dbias,
                    const __fp16* dout, __fp16* doutT, const __fp16* inp, const __fp16* weight,
                    int B, int T, int C, int OC,
                    unsigned int vl) {
    // Fuses residual_backward + matmul_backward, i.e. the sequence
    //     residual_backward(dresidual, tmp, dout, B*T*C);
    //     matmul_backward(dinp, dweight, dbias, tmp, inp, weight, B,T,C,OC);
    // train_gpt2.c chains this way for both skip connections in a block (dout here is the
    // gradient w.r.t. the residual-merged output, e.g. dl_residual3 or dl_residual2). Since
    // out = residual + matmul_out is a plain sum, d(matmul_out) == d(out) == `dout` --
    // no separate dl_attproj/dl_fcproj gradient buffer is needed, `dout` is used directly as
    // matmul_backward's own upstream gradient below. Requires OC == C.

    // gradient into the shortcut/residual branch: dresidual += dout


    int avl = B*T*OC;
    do{
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle16.v v0, (%0)" :: "r"(dout      ));
        dout += vl;
        
        asm volatile("vle16.v v8, (%0)" :: "r"(dresidual));
        asm volatile("vfadd.vv v8, v0, v8");

        asm volatile("vse16.v v8, (%0)" :: "r"(dresidual));
        dresidual += vl;

    } while (avl > 0);
    



    // gradient into the matmul branch -- identical to matmul_backward, using `dout` directly
    matmul_8xVL(dinp, dout, weight, 0, B*T, OC, C, 0, C);

    for (int i = 0; i < B*T; i++){
        float* src = dout + i * OC;
        float* dst = doutT + i;

        avl = OC;
        do{
            asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
            avl -= vl;
            asm volatile("vle16.v v0  , (%0)" :: "r"(src));
            src += vl;
            asm volatile("vsse16.v v0, (%0), %1" ::"r"(dst), "r"(B*T * sizeof(float)));
            dst += vl*B*T;
        } while (avl > 0);
        


    }


    matmul_8xVL(dweight, doutT, inp, 0, OC, B*T, C, 0, C);
    // matmul_4xVL(dweight_t, doutT, inp, 0, OC, B*T, C, 0, C);

    int itr = 0;
    int num_elements = OC;
    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v v8  , (%0)" :: "r"(dbias+itr*VLMAX));
        for (int i = 0; i < B*T; i++) {
                float *doutTaddress = dout + i*OC;
                asm volatile("vle32.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
                asm volatile("vfadd.vv    v8 , v8, v0");
        }
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dbias+itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle32.v v8  , (%0)" :: "r"(dbias+itr*VLMAX));
        for (int i = 0; i < B*T; i++) {
                float *doutTaddress = dout + i*OC;
                asm volatile("vle32.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
                asm volatile("vfadd.vv    v8 , v8, v0");
        }
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dbias+itr*VLMAX));
    }
}



void attention_forward(__fp16* out, __fp16* preatt, __fp16* att,
                       __fp16* inp,
                       __fp16* temp,
                       fp_bits_t* scale, fp_bits_t* val, fp_bits_t* expsum, fp_bits_t* expsum_inv, fp_bits_t* att_btht2, fp_bits_t* maxval,
                       int B, int T, int C, int NH,
                       unsigned int vl) {
    // input is (B, T, 3C) holding the query, key, value (Q, K, V) vectors
    // preatt, att are (B, NH, T, T). NH = number of heads, T = sequence length
    // that holds the pre-attention and post-attention scores (used in backward)
    // output is (B, T, C)
    // attention is the only layer that mixes information across time
    // every other operation is applied at every (b,t) position independently
    // (and of course, no layer mixes information across batch)
    int C3 = C*3;
    int hs = C / NH; // head size
    // scale->f16 = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    scale->f16 = 0.353553;

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                __fp16* query_t = inp + b * T * C3 + t * C3 + h * hs;
                __fp16* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                __fp16* att_bth = att + b*NH*T*T + h*T*T + t*T;

                // pass 1: calculate query dot key and maxval
                maxval->f16 = -10000.0f; // TODO something better
                for (int t2 = 0; t2 <= t; t2++) {
                    __fp16* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key

                    // (query_t) dot (key_t2)
                    val->f16 = 0.0f;                    
                    //query_t V0 - V7
                    //key_t2  V8 - V15
                    //mul     v8 - v15
                    //val     v16
                    int itr = 0;
                    int num_elements = hs;

                    asm volatile("vsetvli  %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
                    asm volatile("vfsub.vv v31,v31,v31");
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(query_t+itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(key_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v16 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v16" : "=f"(val->f32));
                        asm volatile("vfmv.s.f    v31, %0"  : "=f"(val->f32));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(query_t+itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(key_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v16 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v16" : "=f"(val->f32));
                    }

                    val->f16 *= scale->f16;
                    if (val->f16 > maxval->f16) {
                        maxval->f16 = val->f16;
                    }

                    preatt_bth[t2] = val->f16;                    
                }
                // pass 2: calculate the exp and keep track of sum
                // maxval is being calculated and subtracted only for numerical stability

                expsum->f32 = 0.0f;
                // preatt_bth V0 - V7
                // expsum     V8
                int itr = 0;
                int num_elements = t+1;

                asm volatile("vsetvli  %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfsub.vv v31,v31,v31");
                while(itr*VLMAX + VLMAX <= num_elements){
                    asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    asm volatile("vle16.v     v0 , (%0)" :: "r"(preatt_bth+itr*VLMAX));
                    asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval->f32));
                    //----------------------  BUILT IN EXP
                    //THERE IS NO NEED TO ADD itr*VLMAX
                    // asm volatile("vse32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));
                    // snrt_cluster_hw_barrier();
                    // for (int i = 0; i < VLMAX; i++){
                    //     temp[itr*VLMAX + i] = expf(temp[itr*VLMAX + i]);
                    // }     
                    // asm volatile("vle32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));
            
                    //----------------------  FAST EXP
                    fast_exp(temp, temp, 0, 0, 8, VLMAX, vl);

                    asm volatile("vfredsum.vs v8 , v0  , v31");
                    asm volatile("vfmv.f.s    %0 , v8" : "=f"(expsum->f32));
                    asm volatile("vfmv.s.f    v31, %0"  : "=f"(expsum->f32));
                    asm volatile("vse16.v     v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    itr++;
                }
                if(itr*VLMAX < num_elements){
                    asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle16.v     v0 , (%0)" :: "r"(preatt_bth+itr*VLMAX));
                    asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval->f32));

                    //----------------------  BUILT IN EXP
                    // asm volatile("vse32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));
                    // snrt_cluster_hw_barrier();
                    // for (int i = 0; i < num_elements - itr*VLMAX; i++){//THIS MUST BE CHECK NOT TO ITERATE OVER VLMAX
                    //     temp[itr*VLMAX + i] = expf(temp[itr*VLMAX + i]);
                    // }
                    // asm volatile("vle32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));

                    //----------------------  FAST EXP
                    fast_exp(temp, temp, 0, 0, 8, (num_elements - itr*VLMAX), vl);

                    asm volatile("vfredsum.vs v8 , v0  , v31");
                    asm volatile("vfmv.f.s    %0 , v8" : "=f"(expsum->f32));
                    asm volatile("vse16.v     v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                }

                expsum_inv->f32 = expsum->f16 == 0.0f ? 0.0f : (1.0f / ((float)expsum->f16));
                snrt_cluster_hw_barrier();//ISSUE
                expsum_inv->f16 = (__fp16 )expsum_inv->f32;
                // pass 3: normalize to get the softmax
                itr = 0;
                num_elements = t+1;                
                asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while(itr*VLMAX + VLMAX <= num_elements){
                    asm volatile("vle16.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(expsum_inv->f32));
                    asm volatile("vse16.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    itr++;
                }
                if(itr*VLMAX < num_elements){
                    asm volatile("vsetvli   %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle16.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(expsum_inv->f32));
                    asm volatile("vse16.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                }
                for (int t2 = t+1; t2 < T; t2++) {
                    // causal attention mask. not strictly necessary to set to zero here
                    // only doing this explicitly for debugging and checking to PyTorch
                    att_bth[t2] = 0.0f;
                }                

                // pass 4: accumulate weighted values into the output of attention
                itr = 0;
                num_elements = hs;
                __fp16* out_bth = out + b * T * C + t * C + h * hs;
                asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                asm volatile("vfsub.vv v0, v0, v0");//ISSUE
                while(itr*VLMAX + VLMAX <= num_elements){
                    asm volatile("vse16.v v0 , (%0)" :: "r"(out_bth+itr*VLMAX));
                    itr++;
                }
                if(itr*VLMAX < num_elements){
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vse16.v v0 , (%0)" :: "r"(out_bth+itr*VLMAX));
                }

                for (int t2 = 0; t2 <= t; t2++) {
                    __fp16* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    att_btht2->f16 = att_bth[t2];
                    itr = 0;
                    num_elements = hs;
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle16.v    v0 , (%0)" :: "r"(value_t2+itr*VLMAX));
                        asm volatile("vle16.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(att_btht2->f32));
                        asm volatile("vse16.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle16.v    v0 , (%0)" :: "r"(value_t2+itr*VLMAX));
                        asm volatile("vle16.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(att_btht2->f32));
                        asm volatile("vse16.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                    }
                }
            }
        }
    }
}


void attention_forward_opt(__fp16* out, __fp16* preatt, __fp16* att,
                       __fp16* inp,
                       __fp16* temp,
                       __fp16* mask,
                       __fp16* max_arr,
                       __fp16* temp2,
                       fp_bits_t* scale,
                       int B, int T, int C, int NH,
                       unsigned int vl) {
    // input is (B, T, 3C) holding the query, key, value (Q, K, V) vectors
    // preatt, att are (B, NH, T, T). NH = number of heads, T = sequence length
    // that holds the pre-attention and post-attention scores (used in backward)
    // output is (B, T, C)
    // attention is the only layer that mixes information across time
    // every other operation is applied at every (b,t) position independently
    // (and of course, no layer mixes information across batch)
    int C3 = C*3;
    int hs = C / NH; // head size
    scale->f16 = 0.353553;

    fp_bits_t coef;
    fp_bits_t bias;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;

    fp_bits_t one;
    one.f16 = 1.0f;


    int avl;
    int stride = B*T * sizeof(__fp16);
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vle32.v v4  , (%0)" :: "r"(mask));


    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {
            for (int i = 0; i < T; i++){
                __fp16* src = inp + b * T * C3 + h * hs + i * C3;
                __fp16* dst = temp + i;
                avl = hs;

                // while (avl > 0){
                    asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vle16.v v0  , (%0)" :: "r"(src));
                    asm volatile("vsse16.v v0, (%0), %1" ::"r"(dst), "r"(stride));
                    // src += vl;
                    // dst += vl*T;
                    // avl -= vl;
                // }
            }


            for (int t = 0; t < T; t+=4) {// rows (columns) of Q*K output (TxT)
                __fp16* key_t2  = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key
                __fp16* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T + t;
                __fp16* query_t = temp + t;


                // float key0 = key_t2[0*C3];
                // float key1 = key_t2[1*C3];
                // float key2 = key_t2[2*C3];
                // float key3 = key_t2[3*C3];

                float key0;
                float key1;
                float key2;
                float key3;

                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key0) : [a] "r"(key_t2[0*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key1) : [a] "r"(key_t2[1*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key2) : [a] "r"(key_t2[2*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key3) : [a] "r"(key_t2[3*C3]));


                avl = T-t;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v0  , (%0)" :: "r"(query_t));

                for (int i = 0; i < hs; i++) { // rows of Q and elements of K
                    query_t += T;
                    // do{
                        if(i == 0){
                            asm volatile("vfmul.vf  v16, v0, %0" ::"f"(key0));
                            // key0 = key_t2[i+0*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key0) : [a] "r"(key_t2[i+0*C3+1]));

                            asm volatile("vfmul.vf  v20, v0, %0" ::"f"(key1));
                            // key1 = key_t2[i+1*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key1) : [a] "r"(key_t2[i+1*C3+1]));

                            asm volatile("vfmul.vf  v24, v0, %0" ::"f"(key2));
                            // key2 = key_t2[i+2*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key2) : [a] "r"(key_t2[i+2*C3+1]));
                            
                            asm volatile("vfmul.vf  v28, v0, %0" ::"f"(key3));
                            asm volatile("vle16.v v0  , (%0)" :: "r"(query_t));
                            // key3 = key_t2[i+3*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key3) : [a] "r"(key_t2[i+3*C3+1]));

                        }else{
                            asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(key0));
                            // key0 = key_t2[i+0*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key0) : [a] "r"(key_t2[i+0*C3+1]));

                            asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(key1));
                            // key1 = key_t2[i+1*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key1) : [a] "r"(key_t2[i+1*C3+1]));

                            asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(key2));
                            // key2 = key_t2[i+2*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key2) : [a] "r"(key_t2[i+2*C3+1]));

                            asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(key3));
                            asm volatile("vle16.v v0  , (%0)" :: "r"(query_t));
                            // key3 = key_t2[i+3*C3+1];
                            asm volatile("flh %[t], 0(%[a])" : [t] "=f"(key3) : [a] "r"(key_t2[i+3*C3+1]));

                        }

                    //     query_t += vl;
                    //     avl -= vl;
                    // }while (avl > 0);
                }
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale->f32));
                asm volatile("vfmul.vf  v20 , v20, %[A]" ::[A] "f"(scale->f32));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale->f32));
                asm volatile("vfmul.vf  v28 , v28, %[A]" ::[A] "f"(scale->f32));

                asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth+0), "r"(stridesize_patt));
                // asm volatile("vsse32.v v20, (%0), %1" ::"r"(preatt_bth+1), "r"(stridesize_patt));
                // asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth+2), "r"(stridesize_patt));
                // asm volatile("vsse32.v v28, (%0), %1" ::"r"(preatt_bth+3), "r"(stridesize_patt));
                asm volatile("vse16.v v16, (%0)" :: "r"(preatt_bth+0*T));
                asm volatile("vse16.v v20, (%0)" :: "r"(preatt_bth+1*T));
                asm volatile("vse16.v v24, (%0)" :: "r"(preatt_bth+2*T));
                asm volatile("vse16.v v28, (%0)" :: "r"(preatt_bth+3*T));
            }// LOOP OVER T

            avl = T;
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vfsub.vv  v0 , v0, v0");
            for (int t = 0; t < T; t++) {
                asm volatile("vse16.v v0, (%0)" :: "r"(temp+t*T));
            }

            // MAXVAL & EXP
            avl = T;
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
            __fp16* preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle16.v v0  , (%0)" :: "r"(preatt_bth));
            for (int t = 0; t < T; t++) {
                preatt_bth += T;
                asm volatile("vle16.v v4  , (%0)" :: "r"(preatt_bth));
                asm volatile("vfmax.vv v0, v0, v4");
            }

            // float* att_bth    = att    + b*NH*T*T + h*T*T;
            __fp16* att_bth    = temp;
            preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle16.v v8  , (%0)" :: "r"(preatt_bth));
            preatt_bth += T+1;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                asm volatile("vle16.v v12  , (%0)" :: "r"(preatt_bth));
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v8 , v8, v0");
                
                //------------ FAST_EXP
                asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(coef.f32));
                asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(bias.f32));
                asm volatile("vfcvt.rtz.xu.f.v v8, v8");
                //------------


                // asm volatile("vsse32.v v8, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse16.v v8, (%0)" :: "r"(att_bth));
                att_bth += T+1; 
                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v0 , v4, %[A]" ::[A] "r"(0));
                asm volatile("vle16.v v8  , (%0)" :: "r"(preatt_bth));
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v12 , v12, v4");
                //------------ FAST_EXP
                asm volatile("vfmul.vf v12, v12, %[A]" ::[A] "f"(coef.f32));
                asm volatile("vfadd.vf v12, v12, %[A]" ::[A] "f"(bias.f32));
                asm volatile("vfcvt.rtz.xu.f.v v12, v12");
                //------------
                // asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse16.v v12, (%0)" :: "r"(att_bth));
                att_bth += T+1; // I think it is stored transposed !!!!!!!
            }

            // pass 3: normalize to get the softmax
            // att_bth    = att    + b*NH*T*T + h*T*T;
            att_bth    = temp;
            avl = T;//CHECK BETWEEN T OR HS
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(one.f32));
            asm volatile("vle16.v v0  , (%0)" :: "r"(att_bth));
            att_bth += T;
            for (int t = 1; t < T; t++) {
                asm volatile("vle16.v v4  , (%0)" :: "r"(att_bth));
                att_bth += T;
                asm volatile("vfadd.vv v0, v0, v4");
            }

            asm volatile("vfdiv.vv v0,v8,v0");// 1/expsum

            
            __fp16 * att_bth_correct_form = att    + b*NH*T*T + h*T*T;
            att_bth    = temp;
            // att_bth    = att    + b*NH*T*T + h*T*T;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v0, v4");
                asm volatile("vslide1down.vx v8 , v0, %[A]" ::[A] "r"(0));
                // asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                asm volatile("vsse16.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                att_bth_correct_form += T+1;
                att_bth += T+1;

                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v8, v4");
                asm volatile("vslide1down.vx v0 , v8, %[A]" ::[A] "r"(0));
                // asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                asm volatile("vsse16.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                att_bth_correct_form += T+1;
                att_bth += T+1;

            }

            // pass 4: accumulate weighted values into the output of attention
            for (int t = 0; t < T; t++) {//
                avl = hs;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // att_bth    = att    + b*NH*T*T + h*T*T + t;
                att_bth    = att    + b*NH*T*T + h*T*T + t*T;
                __fp16* value_t2 = inp + b * T * C3 + h * hs + C*2; // +C*2 because it's value
                __fp16* out_bth = out + b * T * C + t * C + h * hs;
                asm volatile("vle16.v    v0 , (%0)" :: "r"(value_t2));
                // float att_btht2 = att_bth[0];
                float att_btht2;
                asm volatile("flw %[t], 0(%[a])" : [t] "=f"(att_btht2) : [a] "r"(att_bth[0]));

                value_t2 += C3;

                for (int t2 = 0; t2 <= t; t2++){
                    if(t2 == 0){
                        asm volatile("vfmul.vf   v4 ,v0 ,%[A]" ::[A] "f"(att_btht2));                            
                    }else{
                        asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(att_btht2));
                    }
                    // att_btht2 = att_bth[T*(t2+1)];
                    att_btht2 = att_bth[t2+1];
                    asm volatile("vle16.v    v0 , (%0)" :: "r"(value_t2));
                    value_t2 += C3;
                }
                asm volatile("vse16.v    v4 , (%0)" :: "r"(out_bth));
            }
        }
    }
}

void attention_backward(__fp16* dinp, __fp16* dpreatt, __fp16* datt,
                        __fp16* dout, __fp16* inp, __fp16* att,
                        fp_bits_t* scale, fp_bits_t* temp, fp_bits_t* bth_tmp, fp_bits_t* dbth_tmp,
                        int B, int T, int C, int NH,
                        unsigned int vl) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    // float scale = 1.f / sqrtf(hs);//I CHANGE THIS PART (I DID NOT CHECK THIS)
    scale->f16 = 0.353553;
    // float scale = 1.f / hs;

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                __fp16* att_bth = att + b*NH*T*T + h*T*T + t*T;
                __fp16* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
                __fp16* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                __fp16* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                __fp16* query_t = inp + b * T * C3 + t * C3 + h * hs;

                // backward pass 4, through the value accumulation
                __fp16* dout_bth = dout + b * T * C + t * C + h * hs;
                for (int t2 = 0; t2 <= t; t2++) {
                    __fp16* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    __fp16* dvalue_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C*2;
                    //dout_bth  V0 - V7
                    //value_t2  V8 - V15
                    //dvalue_t2 V16- V24
                    //mul       V8 - V15
                    //datt_bth  V24
                    //att_bth   V8 - V15
                    int itr = 0;
                    int num_elements = hs;
                    temp->f16 = 0;
                    bth_tmp->f16 = att_bth[t2];
                    asm volatile("vsetvli  %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
                    asm volatile("vfsub.vv v31,v31,v31");
                    // asm volatile("vle32.v v31  , (%0)" :: "r"(datt_bth[t2]));//THERE IS SOME ISSUE WITH THIS
                    asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(dout_bth  +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(value_t2  +itr*VLMAX));
                        asm volatile("vle16.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v24 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v24" : "=f"(temp->f32));
                        asm volatile("vfmv.s.f    v31, %0"  : "=f"(temp->f32));
                        // asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(att_bth[t2]));
                        asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(bth_tmp->f32));
                        asm volatile("vse16.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(dout_bth  +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(value_t2  +itr*VLMAX));
                        asm volatile("vle16.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v24 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v24" : "=f"(temp->f32));
                        // asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(att_bth[t2]));
                        asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(bth_tmp->f32));
                        asm volatile("vse16.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                    }
                    datt_bth[t2] = temp->f16 + datt_bth[t2];
                }

                // backward pass 2 & 3, the softmax
                // note that softmax (like e.g. tanh) doesn't need the input (preatt) to backward
                for (int t2 = 0; t2 <= t; t2++) {
                    dpreatt_bth[t2] += datt_bth[t2] * att_bth[t2];
                    int itr = 0;
                    int num_elements = t+1;
                    bth_tmp->f16  = att_bth[t2] ;
                    dbth_tmp->f16 = datt_bth[t2];
                    //att_bth      V0 - V7
                    //dpreatt_bth  V8 - V15
                    //loc          V16- V24
                    asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(att_bth      +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dpreatt_bth  +itr*VLMAX));
                        // asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( att_bth[t2]));
                        // asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(datt_bth[t2]));
                        asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( bth_tmp->f32));
                        asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(dbth_tmp->f32));
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dpreatt_bth +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(att_bth      +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dpreatt_bth  +itr*VLMAX));
                        // asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( att_bth[t2]));
                        // asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(datt_bth[t2]));
                        asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( bth_tmp->f32));
                        asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(dbth_tmp->f32));                        
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dpreatt_bth +itr*VLMAX));
                    }
                }

                // backward pass 1, the query @ key matmul
                for (int t2 = 0; t2 <= t; t2++) {
                    __fp16* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    __fp16* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    int itr = 0;
                    int num_elements = hs;
                    temp->f16 = dpreatt_bth[t2] * scale->f16;
                    //key_t2      V0 - V7
                    //dquery_t    V8 - V15
                    //query_t     V0 - V7
                    //dkey_t2     V8 - V15
                    asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(key_t2   +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp->f32));
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));

                        asm volatile("vle16.v     v0 , (%0)" :: "r"(query_t  +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp->f32));
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle16.v     v0 , (%0)" :: "r"(key_t2   +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp->f32));
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));

                        asm volatile("vle16.v     v0 , (%0)" :: "r"(query_t  +itr*VLMAX));
                        asm volatile("vle16.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp->f32));
                        asm volatile("vse16.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                    }
                }
            }
        }
    }
}


void attention_backward_rev3(__fp16* dinp, __fp16* dpreatt, __fp16* datt,
                              __fp16* dout, __fp16* inp, __fp16* att,
                              __fp16* temp, __fp16* dscratch, __fp16* mask,
                              fp_bits_t* scale,
                              int B, int T, int C, int NH,
                              unsigned int vl) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.f / sqrtf(hs);
    scale->f16 = 0.353553;

    int avl;
    int stride_T = T * sizeof(__fp16);        // transpose stride for dout -> temp
    int stridesize_patt = T * 4;              // transpose-back stride for dscratch -> datt


    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(-1));
            asm volatile("vle16.v v4  , (%0)" :: "r"(mask));

            for (int i = 0; i < T; i++) {
                __fp16* src = dout + b * T * C + h * hs + i * C;
                __fp16* dst = temp + i;
                avl = hs;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v0  , (%0)" :: "r"(src));
                asm volatile("vsse16.v v0, (%0), %1" ::"r"(dst), "r"(stride_T));
            }

            for (int i = 0; i < T; i++) {
                __fp16* src = datt +  b*NH*T*T + h*T*T + i*T;
                __fp16* dst = dscratch + i;
                avl = T;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v0  , (%0)" :: "r"(src));
                asm volatile("vsse16.v v0, (%0), %1" ::"r"(dst), "r"(stride_T));
            }



            for (int t = 0; t < T; t += 4) {
                __fp16* value_t2   = inp + b * T * C3 + t * C3 + h * hs + C*2; // +C*2 -> value
                __fp16* dout_bth_T = temp + t;
                __fp16* datt_bth   = datt + b*NH*T*T + h*T*T + t*T + t;
                __fp16* datt_bth_T = dscratch + t * T + t;


                float v0;
                float v1;
                float v2;
                float v3;
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v0) : [a] "r"(value_t2[0*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v1) : [a] "r"(value_t2[1*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v2) : [a] "r"(value_t2[2*C3]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v3) : [a] "r"(value_t2[3*C3]));

                avl = T - t;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v v0  , (%0)" :: "r"(dout_bth_T));

                asm volatile("vle16.v v16 , (%0)" :: "r"(datt_bth_T+0*T));
                asm volatile("vle16.v v20 , (%0)" :: "r"(datt_bth_T+1*T));
                asm volatile("vle16.v v24 , (%0)" :: "r"(datt_bth_T+2*T));
                asm volatile("vle16.v v28 , (%0)" :: "r"(datt_bth_T+3*T));


                for (int i = 0; i < hs; i++) {
                    dout_bth_T += T;
                    asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(v0));
                    // v0 = value_t2[i+0*C3+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v0) : [a] "r"(value_t2[i+0*C3+1]));

                    asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(v1));
                    // v1 = value_t2[i+1*C3+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v1) : [a] "r"(value_t2[i+1*C3+1]));
    
                    asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(v2));
                    // v2 = value_t2[i+2*C3+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v2) : [a] "r"(value_t2[i+2*C3+1]));

                    asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(v3));
                    asm volatile("vle16.v v0  , (%0)" :: "r"(dout_bth_T));
                    // v3 = value_t2[i+3*C3+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(v3) : [a] "r"(value_t2[i+3*C3+1]));

                }

                asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // asm volatile("vse32.v v16, (%0)" :: "r"(datt_bth+0*T));// FOR NOW TRANSPOSED TO WHAT SUPPOSED TO BE AND I DRAW IN THE FIGURES
                // // moshkel ine ke man avale kar balaye ghotr ro sefr mikonam vali mogheye store mikham zire ghotr sefr bashan
                // asm volatile("vse32.v v20, (%0)" :: "r"(datt_bth+1*T));
                // asm volatile("vse32.v v24, (%0)" :: "r"(datt_bth+2*T));
                // asm volatile("vse32.v v28, (%0)" :: "r"(datt_bth+3*T));

                asm volatile("vsse16.v v16, (%0), %1" ::"r"(datt_bth+0), "r"(stridesize_patt));
                asm volatile("vsse16.v v20, (%0), %1" ::"r"(datt_bth+1), "r"(stridesize_patt));
                asm volatile("vsse16.v v24, (%0), %1" ::"r"(datt_bth+2), "r"(stridesize_patt));
                asm volatile("vsse16.v v28, (%0), %1" ::"r"(datt_bth+3), "r"(stridesize_patt));

            }
            // ---------------------------------------------------------------
            // Step 2.5: 
            // ---------------------------------------------------------------
            for (int t = 0; t < T; t += 4) {// ASSUME UPPER DIAGONAL ELEMENTS OF ATT ARE 0
                __fp16* att_bth = att + b*NH*T*T + h*T*T + t*T + t;// move on diagonal elements
                __fp16* dout_bth = dout + b * T * C + t * C + h * hs;
                __fp16* dvalue_t2 = dinp + b * T * C3 + t * C3 + h * hs + C*2;

                avl = hs;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                float a0;
                float a1;
                float a2;
                float a3;

                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a0) : [a] "r"(att_bth[0]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a1) : [a] "r"(att_bth[1]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a2) : [a] "r"(att_bth[2]));
                asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a3) : [a] "r"(att_bth[3]));


                asm volatile("vle16.v v0  , (%0)" :: "r"(dvalue_t2+0*C3));
                asm volatile("vle16.v v4  , (%0)" :: "r"(dvalue_t2+1*C3));
                asm volatile("vle16.v v8  , (%0)" :: "r"(dvalue_t2+2*C3));
                asm volatile("vle16.v v12 , (%0)" :: "r"(dvalue_t2+3*C3));

                asm volatile("vle16.v v16  , (%0)" :: "r"(dout_bth));
                dout_bth += hs;

                for (int i = 0; i < T-t; i+=2) {
                    asm volatile("vfmacc.vf  v0 , %[A], v16 " ::[A] "f"(a0));
                    // a0 = att_bth[(i+1)*T+0];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a0) : [a] "r"(att_bth[(i+1)*T+0]));

                    asm volatile("vfmacc.vf  v4 , %[A], v16 " ::[A] "f"(a1));
                    // a1 = att_bth[(i+1)*T+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a1) : [a] "r"(att_bth[(i+1)*T+1]));

                    asm volatile("vfmacc.vf  v8 , %[A], v16 " ::[A] "f"(a2));
                    // a2 = att_bth[(i+1)*T+2];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a2) : [a] "r"(att_bth[(i+1)*T+2]));

                    asm volatile("vfmacc.vf  v12, %[A], v16 " ::[A] "f"(a3));
                    // a3 = att_bth[(i+1)*T+3];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a3) : [a] "r"(att_bth[(i+1)*T+3]));
                    
                    asm volatile("vle16.v v20  , (%0)" :: "r"(dout_bth));
                    dout_bth += hs;

                    asm volatile("vfmacc.vf  v0 , %[A], v20 " ::[A] "f"(a0));
                    // a0 = att_bth[(i+2)*T+0];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a0) : [a] "r"(att_bth[(i+2)*T+0]));

                    asm volatile("vfmacc.vf  v4 , %[A], v20 " ::[A] "f"(a1));
                    // a1 = att_bth[(i+2)*T+1];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a1) : [a] "r"(att_bth[(i+2)*T+1]));

                    asm volatile("vfmacc.vf  v8 , %[A], v20 " ::[A] "f"(a2));
                    a2 = att_bth[(i+2)*T+2];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a2) : [a] "r"(att_bth[(i+2)*T+2]));

                    asm volatile("vfmacc.vf  v12, %[A], v20 " ::[A] "f"(a3));
                    a3 = att_bth[(i+2)*T+3];
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(a3) : [a] "r"(att_bth[(i+2)*T+3]));

                    asm volatile("vle16.v v16  , (%0)" :: "r"(dout_bth));
                    dout_bth += hs;
                }

                asm volatile("vse16.v v0 , (%0)" :: "r"(dvalue_t2+0*C3));
                asm volatile("vse16.v v4 , (%0)" :: "r"(dvalue_t2+1*C3));
                asm volatile("vse16.v v8 , (%0)" :: "r"(dvalue_t2+2*C3));
                asm volatile("vse16.v v12, (%0)" :: "r"(dvalue_t2+3*C3));

            }
            // ---------------------------------------------------------------
            // Step 3: backward pass 2 & 3, the softmax
            // note that softmax (like e.g. tanh) doesn't need the input (preatt) to backward
            // ---------------------------------------------------------------

            for (int t = 0; t < T; t ++) {
                __fp16* att_bth = att + b*NH*T*T + h*T*T + t*T;
                __fp16* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
                __fp16* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;

                avl = t+1;
                asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle16.v     v0 , (%0)" :: "r"(att_bth));
                asm volatile("vle16.v     v4 , (%0)" :: "r"(datt_bth));
                asm volatile("vle16.v     v8 , (%0)" :: "r"(dpreatt_bth));

                asm volatile("vfmul.vv    v12 , v0 , v4");
                asm volatile("vfadd.vv    v8  , v12, v8");
                asm volatile("vse16.v     v12 , (%0)" :: "r"(temp));

                float temp_t2;

                for (int t2 = 0; t2 <= t; t2++) {
                    asm volatile("flh %[t], 0(%[a])" : [t] "=f"(temp_t2) : [a] "r"(temp[t2]));
                    asm volatile("vfnmsac.vf  v8 , %[A], v0" ::[A] "f"(temp_t2));
                }
                asm volatile("vse16.v     v8 , (%0)" :: "r"(dpreatt_bth));
            }

            for (int t = 0; t < T; t ++) {
                int avl = hs;
                asm volatile("vsetvli  %0, %1, e16, m4, ta, ma" : "=r"(vl) : "r"(avl));
                __fp16* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                __fp16* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                __fp16* query_t = inp + b * T * C3 + t * C3 + h * hs;

                asm volatile("vle16.v     v8 , (%0)" :: "r"(dquery_t));
                asm volatile("vle16.v     v16, (%0)" :: "r"(query_t));

                for (int t2 = 0; t2 <= t; t2++) {
                    __fp16* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    __fp16* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key

                    fp_bits_t temp;

                    temp.f16 = dpreatt_bth[t2] * scale->f16;

                    asm volatile("vle16.v     v0 , (%0)" :: "r"(key_t2));
                    asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp.f32));

                    asm volatile("vle16.v     v12, (%0)" :: "r"(dkey_t2));
                    asm volatile("vfmacc.vf   v12, %[A], v16" ::[A] "f"(temp.f32));
                    asm volatile("vse16.v     v12, (%0)" :: "r"(dkey_t2));

                }
                    asm volatile("vse16.v     v8 , (%0)" :: "r"(dquery_t));
            }
        }
    }
}


// #define GELU_SCALING_FACTOR 0.797884561f
void gelu_forward(__fp16* out, __fp16* inp, int N, unsigned int vl) {
    // // (approximate) GeLU elementwise non-linearity in the MLP block of Transformer

    int itr = 0;
    int num_elements = N;
    fp_bits_t const_tmp;
    fp_bits_t tmp1;
    fp_bits_t tmp05;
    fp_bits_t sf;

    const_tmp.f16 = 0.044715f;
    tmp1.f16 = 1.0f;
    tmp05.f16 = 0.5f;
    sf.f16 = 0.797884561f;

    int avl = N;

    fp_bits_t coef;
    fp_bits_t bias;
    fp_bits_t temp1;
    fp_bits_t temp_1;
    fp_bits_t temp2;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;
    temp_1.f16 = -1.0f;
    temp1.f16 = 1.0f;
    temp2.f16 = 2.0f;

    do{
        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle16.v     v0 , (%0)" :: "r"(inp));
        inp += vl;
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(const_tmp.f32));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(sf.f32));
        //----------------------  BUILT IN TANH V2
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out));

        // for (int i = 0; i < num_elements - itr*VLMAX; i++){
        //     out[itr*VLMAX + i] = tanhf(out[itr*VLMAX + i]);
        // }
        // asm volatile("vle32.v     v8 , (%0)" :: "r"(out));

        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        // asm volatile("vfmul.vv    v8 , v0, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out));
        // out += vl;        
        //----------------------  FAST TANH
        // snrt_cluster_hw_barrier();
        // asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));

        // fast_tanh_v2(temp, temp, temp, 0, 8, (VLMAX), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL  
         
        // asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(temp2.f32));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

        asm volatile("vfcvt.rtz.xu.f.v v16, v16");


        asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(temp1.f32));
        asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(temp1.f32));

        asm volatile("vfdiv.vv v8, v16, v8");
        //----------------------

        asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(tmp1.f32));
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(tmp05.f32));
        asm volatile("vse16.v     v8 , (%0)" :: "r"(out));
        out += vl;
    } while (avl > 0);
    


}

void gelu_backward(__fp16* dinp, __fp16* inp, __fp16* dout,
                   __fp16* temp, __fp16* temp1, __fp16* temp2,
                   int N, unsigned int vl) {

    fp_bits_t const_tmp;
    fp_bits_t tmp3xcnst;
    fp_bits_t tmp1;
    fp_bits_t tmp05;
    fp_bits_t sf;

    const_tmp.f16 = 0.044715f;
    tmp3xcnst.f16 = 0.134145f;
    tmp1.f16 = 1.0f;
    tmp05.f16 = 0.5f;
    sf.f16 = 0.797884561f;

    fp_bits_t coef;
    fp_bits_t bias;
    fp_bits_t const1;
    fp_bits_t const_1;
    fp_bits_t const2;

    coef.f16 = 1486.0f;
    bias.f16 = 15360.0f;
    const_1.f16 = -1.0f;
    const1.f16 = 1.0f;
    const2.f16 = 2.0f;



    int avl = N;
    do{
        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle16.v     v0 , (%0)" :: "r"(inp));
        inp += vl;
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(const_tmp.f32));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(sf.f32));//tanh_arg

        //----------------------  FAST TANH/COSH V2
        asm volatile("vse16.v     v8 , (%0)" :: "r"(temp));
        asm volatile("vle16.v     v16, (%0)" :: "r"(temp));
        snrt_cluster_hw_barrier();

        // fast_tanh_v2(temp1, temp1, temp1, 0, 16, (num_elements - itr*VLMAX), vl);
            asm volatile("vfmul.vf v24, v16, %[A]" ::[A] "f"(const2.f32));
    
            asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(bias.f32));
    
            asm volatile("vfcvt.rtz.xu.f.v v24, v24");
    
            asm volatile("vfadd.vf v16, v24, %[A]" ::[A] "f"(const1.f32));
            asm volatile("vfsub.vf v24, v24, %[A]" ::[A] "f"(const1.f32));
    
            asm volatile("vfdiv.vv v16, v24, v16");   

        asm volatile("vse16.v     v16 , (%0)" :: "r"(temp));

        // fast_cosh_v2(temp2, temp2, temp2, 0, 8, (num_elements - itr*VLMAX), vl);
            asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(const_1.f32));

            asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(bias.f32));

            asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(coef.f32));
            asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(bias.f32));

            asm volatile("vfcvt.rtz.xu.f.v v16, v16");
            asm volatile("vfcvt.rtz.xu.f.v v8 , v8");


            asm volatile("vfadd.vv v16, v16, v8");
            asm volatile("vfmul.vf    v8, v16, %[A]" ::[A] "f"(0.5));

            //LOAD THE TANH BACK
        asm volatile("vle16.v     v16, (%0)" :: "r"(temp));//tanh_out

        //---------------------------------------------------------  BYPASS NONLINEAR FUNCTIONS
        
        // asm volatile("vse16.v     v8 , (%0)" :: "r"(temp+itr*VLMAX));
        // asm volatile("vle16.v     v16, (%0)" :: "r"(temp+itr*VLMAX));

        //---------------------------------------------------------
        asm volatile("vfmul.vv    v8, v8, v8");//sech_out
        asm volatile("vfmul.vv    v24 , v0, v0");//x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(tmp3xcnst.f32));//3.0f * 0.044715f * x * x
        asm volatile("vfadd.vf    v24 , v24, %[A] " ::[A] "f"(tmp1.f32));//1.0f + 3.0f * 0.044715f * x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(sf.f32));
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(tmp05.f32));
        asm volatile("vfmul.vv    v24 , v24, v0");//x * 0.5f *  GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)
        asm volatile("vfdiv.vv    v0  , v24, v8");//x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)

        asm volatile("vfadd.vf    v16, v16, %[A] " ::[A] "f"(tmp1.f32));//(1.0f + tanh_out)
        asm volatile("vfmul.vf    v16, v16, %[A] " ::[A] "f"(tmp05.f32));//0.5f * (1.0f + tanh_out)
      
        asm volatile("vfadd.vv    v0 , v16, v0");
        asm volatile("vle16.v     v8 , (%0)" :: "r"(dout));
        dout += vl;
        asm volatile("vle16.v     v16, (%0)" :: "r"(dinp));

        asm volatile("vfmacc.vv    v8 , v16, v0");
        //---------------------------------------------------------
        asm volatile("vse16.v     v8 , (%0)" :: "r"(dinp));      
        dinp += vl;
    } while (avl > 0);
}

void residual_forward(__fp16* out, __fp16* inp1, __fp16* inp2, int N, unsigned int vl) {

    int avl = N;

    do{
        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle16.v     v0 , (%0)" :: "r"(inp1));
        inp1 += vl;
        asm volatile("vle16.v     v8 , (%0)" :: "r"(inp2));
        inp2 += vl;
        asm volatile("vfadd.vv    v8 , v0, v8");
        asm volatile("vse16.v     v8 , (%0)" :: "r"(out));
        out += vl;
    } while (avl > 0);
    
}

void residual_backward(__fp16* dinp1, __fp16* dinp2, __fp16* dout, int N, unsigned int vl) {

    int avl = N;
    do{
        asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle16.v     v0 , (%0)" :: "r"(dout));
        dout += vl;
        asm volatile("vle16.v     v8 , (%0)" :: "r"(dinp1));
        asm volatile("vfadd.vv    v8 , v0, v8 ");
        asm volatile("vle16.v     v16, (%0)" :: "r"(dinp2));
        asm volatile("vfadd.vv    v16, v0, v16");
        asm volatile("vse16.v     v8 , (%0)" :: "r"(dinp1));
        dinp1 += vl;
        asm volatile("vse16.v     v16, (%0)" :: "r"(dinp2));
        dinp2 += vl;
    } while (avl > 0);
    


}

void softmax_forward(__fp16* probs, __fp16* logits, __fp16* temp, fp_bits_t* sum, fp_bits_t* maxval, int B, int T, int V, int Vp, unsigned int vl) {
    // output: probs are (B,T,Vp) of the probabilities (sums to 1.0 in each b,t position)
    // input: logits is (B,T,Vp) of the unnormalized log probabilities
    // Vp is the padded vocab size (for efficiency), V is the "real" vocab size
    // example: Vp is 50304 and V is 50257
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // probs <- softmax(logits)
            __fp16* logits_bt = logits + b * T * Vp + t * Vp;
            __fp16* probs_bt = probs + b * T * Vp + t * Vp;

            maxval->f16 = -10000.0f; // TODO something better

            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            int avl = V;
            do{
                asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v     v0 , (%0)" :: "r"(logits_bt));
                logits_bt += vl;
                asm volatile("vfredmax.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(maxval->f32));
                asm volatile("vfmv.s.f v31, %0"  : "=f"(maxval->f32));
            } while (avl > 0);
            


            // snrt_cluster_hw_barrier();
            // snrt_cluster_hw_barrier();//ISSUE

            sum->f16 = 0.0f;

            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            avl = V;
            logits_bt = logits + b * T * Vp + t * Vp;
            fp_bits_t coef;
            fp_bits_t bias;

            coef.f16 = 1486.0f;
            bias.f16 = 15360.0f;

            do{
                asm volatile("vsetvli  %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v     v0 , (%0)" :: "r"(logits_bt));
                logits_bt += vl;
                asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval->f32));
                //----------------------  FAST EXP
                // fast_exp(temp, probs_bt+itr*VLMAX, 0, 0, 8, VLMAX, vl);
                asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(coef.f32));
                asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(bias.f32));
                asm volatile("vfcvt.rtz.xu.f.v v0, v0");                
                //----------------------
                asm volatile("vse16.v     v0 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
                asm volatile("vfredsum.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(sum->f32));
                asm volatile("vfmv.s.f v31, %0"  : "=f"(sum->f32));
            } while (avl > 0);

            probs_bt = probs + b * T * Vp + t * Vp;
            avl = V;
            do{
                asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v     v0 , (%0)" :: "r"(probs_bt));
                asm volatile("vfdiv.vf    v8, v0, %[A]" ::[A] "f"(sum->f32));
                asm volatile("vse16.v     v8 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
            } while (avl > 0);
            

            // for extra super safety we may wish to include this too,
            // forcing the probabilities here to be zero, but it shouldn't matter
            for (int i = V; i < Vp; i++) {
                probs_bt[i] = 0;
            }
        }
    }
}

void crossentropy_forward(__fp16* losses,
                          __fp16* probs, int* targets,
                          int B, int T, int Vp) {
    // output: losses is (B,T) of the individual losses at each position
    // input: probs are (B,T,Vp) of the probabilities
    // input: targets is (B,T) of integers giving the correct index in logits
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // loss = -log(probs[target])
            __fp16* probs_bt = probs + b * T * Vp + t * Vp;
            int ix = targets[b * T + t];
            losses[b * T + t] = (__fp16 )(-logf((float)probs_bt[ix]));
        }
    }
}

void crossentropy_softmax_backward(__fp16* dlogits,
                           __fp16* dlosses, __fp16* probs, int* targets,
                           int B, int T, int V, int Vp, 
                           unsigned int vl) {
    fp_bits_t dloss;
    // backwards through both softmax and crossentropy
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            __fp16* dlogits_bt = dlogits + b * T * Vp + t * Vp;
            __fp16* probs_bt = probs + b * T * Vp + t * Vp;
            dloss.f16 = dlosses[b * T + t];
            int ix = targets[b * T + t];
            // note we only loop to V, leaving the padded dimensions
            // of dlogits untouched, so gradient there stays at zero
            if(ix <= V){
                dlogits_bt[ix] -= dloss.f16;
            }

            int avl = V;

            do{
                asm volatile("vsetvli     %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle16.v     v0 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
                asm volatile("vle16.v     v8 , (%0)" :: "r"(dlogits_bt));
                asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(dloss.f32));
                asm volatile("vse16.v     v8 , (%0)" :: "r"(dlogits_bt));
                dlogits_bt += vl;
            } while (avl > 0);
        }
    }
}



int main() {

    volatile int errors = 0;
    
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid       = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;
    int B = B_Size;
    int C = C_Size;
    int T = T_Size;    

    
    int  * inp_int  ;
    __fp16* inp      ;
    __fp16* dinp     ;
    __fp16* dinp_t   ;//TEMP
    __fp16* mean     ;
    __fp16* wte      ;
    __fp16* wpe      ;
    __fp16* dwte     ;
    __fp16* dwpe     ;
    __fp16* rstd     ;
    __fp16* weight   ;
    __fp16* weightT  ;//TRANSPOSED
    __fp16* dweight  ;
    __fp16* dweight_t;//TEMP
    __fp16* bias     ;
    __fp16* dbias    ;
    __fp16* out      ;
    __fp16* dout     ;
    __fp16* doutT    ;//TRANSPOSED
    __fp16* preatt   ;
    __fp16* dpreatt  ;
    __fp16* att      ;
    __fp16* datt     ;
    __fp16* losses   ;
    __fp16* probs    ;
    int   * targets  ;
    __fp16* dlosses  ;
    __fp16* dlogits  ;
    __fp16* temp     ;
    __fp16* temp1    ;
    __fp16* temp2    ;




    __fp16 *a;
    __fp16 *b;
    __fp16 *c;


    uint32_t v;
    asm volatile ("csrr %0, 0x800" : "=r"(v));

    asm volatile ("csrw 0x800, %0" :: "r"(v | 3));


    int NH = 2;

    if (cid == 0) {

        // temp        = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
        // temp1       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
        // temp2       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
        snrt_dma_wait_all();
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    unsigned int vl;
    // Start timer

    timer = benchmark_get_cycle();
    // Start dump

    //--------------------------------------------------------- encoder_forward
    // if (cid == 0) {
    //     inp_int     = (int  *)snrt_l1alloc(B * T *     sizeof(int  ));
    //     wte         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     wpe         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     out         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp_int, inp_dram  , B * T *     sizeof(int  ));
    //     snrt_dma_start_1d(wte    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(wpe    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // encoder_forward(out, inp_int, wte, wpe, B, T, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out  , outG   , B * T * C);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- encoder_backward
    // if (cid == 0) {
    //     inp_int      = (int  *)snrt_l1alloc(B * T *     sizeof(int  ));
    //     dwte         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     dwpe         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     dout         = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp_int , inp_dram  , B * T *     sizeof(int  ));
    //     snrt_dma_start_1d(dwte    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dwpe    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dout    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // encoder_backward(dwte, dwpe, dout, inp_int, B, T, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dwte  , dwteG   , B * T * C);
    //     printf("CHECK RESULTS2\n");
    //     check_result(dwpe  , dwpeG   , B * T * C);        
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- layernorm_forward
    // fp_bits_t * m;
    // fp_bits_t * v;
    // fp_bits_t * s;
    // if (cid == 0) {
    //     m       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     v       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     s       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     // temp      = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     inp       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     weight    = (__fp16*)snrt_l1alloc(1 * 1 * C * sizeof(__fp16));
    //     bias      = (__fp16*)snrt_l1alloc(1 * 1 * C * sizeof(__fp16));
    //     mean      = (__fp16*)snrt_l1alloc(B * T * 1 * sizeof(__fp16));
    //     rstd      = (__fp16*)snrt_l1alloc(B * T * 1 * sizeof(__fp16));
    //     out       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * 1 * C * sizeof(__fp16));
    //     snrt_dma_start_1d(bias   , data1_dram, 1 * 1 * C * sizeof(__fp16));
    //     snrt_dma_start_1d(mean   , data1_dram, B * T * 1 * sizeof(__fp16));
    //     snrt_dma_start_1d(rstd   , data1_dram, B * T * 1 * sizeof(__fp16));
    //     // snrt_dma_start_1d(temp   , rstdG     , B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // layernorm_forward(out, mean, rstd, inp, weight, bias, temp, m, v, s, B, T, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(mean  , meanG   , B * T * 1);
    //     printf("CHECK RESULTS2\n");
    //     check_result(rstd  , rstdG   , B * T * 1);
    //     printf("CHECK RESULTS3\n");
    //     check_result(out   , outG    , B * T * C);        
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- layernorm_backward
    // fp_bits_t * dnorm_mean;
    // fp_bits_t * dnorm_norm_mean;
    // fp_bits_t * mean_bt;
    // fp_bits_t * rstd_bt;
    // if (cid == 0) {
    //     dnorm_mean       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     dnorm_norm_mean  = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     mean_bt          = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     rstd_bt          = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     inp    = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     dinp   = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     mean   = (__fp16*)snrt_l1alloc(B * T * 1 * sizeof(__fp16));
    //     rstd   = (__fp16*)snrt_l1alloc(B * T * 1 * sizeof(__fp16));
    //     dweight= (__fp16*)snrt_l1alloc(1 * 1 * C * sizeof(__fp16));
    //     weight = (__fp16*)snrt_l1alloc(1 * 1 * C * sizeof(__fp16));
    //     dbias  = (__fp16*)snrt_l1alloc(1 * 1 * C * sizeof(__fp16));
    //     dout   = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dinp   , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dweight, data1_dram, 1 * 1 * C * sizeof(__fp16));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * 1 * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dbias  , data1_dram, 1 * 1 * C * sizeof(__fp16));
    //     snrt_dma_start_1d(mean   , data1_dram, B * T * 1 * sizeof(__fp16));
    //     snrt_dma_start_1d(rstd   , data1_dram, B * T * 1 * sizeof(__fp16));
    //     snrt_dma_start_1d(dout   , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // layernorm_backward(dinp, dweight, dbias, dout, inp, weight, mean, rstd, dnorm_mean, dnorm_norm_mean, mean_bt, rstd_bt, B, T, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dbias    , dbiasG   , 1 * 1 * C);
    //     printf("CHECK RESULTS2\n");
    //     check_result(dweight  , dweightG , 1 * 1 * C);
    //     printf("CHECK RESULTS3\n");
    //     check_result(dinp     , dinpG    , B * T * C);        
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- matmul_forward_naive
    int OC = C;
    if (cid == 0) {
        inp     = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
        weight  = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
        // weightT = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
        // bias    = (__fp16*)snrt_l1alloc(1 * 1 * OC* sizeof(__fp16));
        out     = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    }
    snrt_cluster_hw_barrier();
    if (cid == 0) {
        snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
        snrt_dma_start_1d(weight , data1_dram, 1 * OC* C * sizeof(__fp16));
        // snrt_dma_start_1d(bias   , data1_dram, 1 * 1 * OC* sizeof(__fp16));
        snrt_dma_start_1d(out    , data1_dram, B * T * C * sizeof(__fp16));
        snrt_dma_wait_all();
    }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // matmul_forward_naive(out, inp, weight, weightT, bias, B, T, C, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out    , outG   , B * T * C);
    // }
    //#########################################################


    const gemm_layer gemm_l = {.M = 128,
                           .N = 128,
                           .K = 128,
                           .TA = 0,
                           .TB = 0,
                           .ALPHA = 0,
                           .dtype = FP16,
                           .expand = 0};

    unsigned int m_start, m_end;
    unsigned int p_start, p_end;
    unsigned int kernel_size;

    // if (cid == 0) {
    //     a = (__fp16 *)snrt_l1alloc(gemm_l.M * gemm_l.K * sizeof(__fp16));
    //     b = (__fp16 *)snrt_l1alloc(gemm_l.K * gemm_l.N * sizeof(__fp16));
    //     c = (__fp16 *)snrt_l1alloc(gemm_l.M * gemm_l.N * sizeof(__fp16));
    // }

    p_start = 0;
    p_end = gemm_l.N;
    m_start = (gemm_l.M / num_cores) * cid;
    m_end = (gemm_l.M / num_cores) * (cid + 1);

    // // Wait for all cores to finish
    // snrt_cluster_hw_barrier();

    // // Initialize matrices
    // if (cid == 0) {
    //     snrt_dma_start_1d(a, data1_dram, gemm_l.M * gemm_l.K * sizeof(__fp16));
    //     snrt_dma_start_1d(b, data1_dram, gemm_l.K * gemm_l.N * sizeof(__fp16));
    //     snrt_dma_start_1d(c, data1_dram, gemm_l.M * gemm_l.N * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }

    volatile uint32_t v0 = 0;
    volatile uint32_t v128 = 128;

    timer = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
        start_kernel();
    
    // matmul_8xVL(c, a, b, m_start, m_end, gemm_l.K, gemm_l.N, p_start, p_end);
    // matmul_8xVL(out, inp, weight, m_start, m_end, gemm_l.K, gemm_l.N, p_start, p_end);
    matmul_8xVL(out, inp, weight, 0, 128, 128, 128, v0, 128);
    // matmul_8xVL(out, inp, weight, 0, B*T, C, C, 0, C);
    // matmul_forward_naive(out, inp, weight, weightT, bias, B, T, C, C, vl);
    // matmul_forward_naive(out, inp, weight, weightT, bias, 1, gemm_l.K, gemm_l.N, gemm_l.K, vl);

    // matmul_8xVL(out, inp, weight, 0, B*T, C, OC, 0, OC);


    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();







    //---------------------------------------------------------

    //--------------------------------------------------------- matmul_backward
    // fp_bits_t * fp_tmp;
    // int OC = C;
    // if (cid == 0) {
    //     fp_tmp       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     inp       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     dinp      = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     // dinp_t    = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     dweight   = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
    //     // dweight_t = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
    //     weight    = (__fp16*)snrt_l1alloc(1 * OC* C * sizeof(__fp16));
    //     dbias     = (__fp16*)snrt_l1alloc(1 * 1 * OC* sizeof(__fp16));
    //     doutT     = (__fp16*)snrt_l1alloc(B * T * OC* sizeof(__fp16));
    //     dout      = (__fp16*)snrt_l1alloc(B * T * OC* sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dinp   , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dweight, data1_dram, 1 * OC* C * sizeof(__fp16));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * OC* C * sizeof(__fp16));
    //     snrt_dma_start_1d(dbias  , data1_dram, 1 * 1 * OC* sizeof(__fp16));
    //     snrt_dma_start_1d(dout   , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // matmul_backward(dinp, dweight, dbias, dout, doutT, inp, weight, fp_tmp, B, T, C, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dinp     , dinpG   , B * T * C);
    //     printf("CHECK RESULTS2\n");
    //     check_result(dweight  , dweightG, 1 * OC* C);
    //     printf("CHECK RESULTS3\n");
    //     check_result(dbias    , dbiasG  , 1 * 1 *OC);        
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- attention_forward
    // fp_bits_t* scale;
    // fp_bits_t* val;
    // fp_bits_t* expsum;
    // fp_bits_t* expsum_inv;
    // fp_bits_t* att_btht2;
    // fp_bits_t* maxval;
    // // T = T/2;
    // NH=1;
    // if (cid == 0) {
    //     scale       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     val         = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     expsum      = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     expsum_inv  = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     att_btht2   = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     maxval      = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     temp    = (__fp16*)snrt_l1alloc(B * T  * C     * sizeof(__fp16));
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * 3*C   * sizeof(__fp16));
    //     preatt  = (__fp16*)snrt_l1alloc(B * NH * T * T * sizeof(__fp16));
    //     att     = (__fp16*)snrt_l1alloc(B * NH * T * T * sizeof(__fp16));
    //     out     = (__fp16*)snrt_l1alloc(B * T  * C     * sizeof(__fp16));
    //     for (int i = 0; i < 3; i++){
    //         snrt_dma_start_1d(inp    + i*(B * T * C)         , data1_dram, B * T  * C        * sizeof(__fp16));
    //     }
    //     snrt_dma_start_1d(preatt , data1_dram, B * NH * T * T    * sizeof(__fp16));
    //     snrt_dma_start_1d(att    , data1_dram, B * NH * T * T    * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // attention_forward(out, preatt, att, inp, temp, scale, val, expsum, expsum_inv, att_btht2, maxval, B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS2\n");
    //     check_result(preatt  , preattG, B * NH * T * T);    
    //     printf("CHECK RESULTS3\n");
    //     check_result(att     , attG   , B * NH * T * T);
    //     printf("CHECK RESULTS1\n");
    //     check_result(out     , outG   , B * T * C);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- attention_backward
    // T = T/2;
    // NH=1;
    // fp_bits_t* scale;
    // fp_bits_t* temp_mem;
    // fp_bits_t* bth_tmp;
    // fp_bits_t* dbth_tmp;
    // if (cid == 0) {
    //     scale       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     temp_mem    = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     bth_tmp     = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     dbth_tmp    = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * 3*C   * sizeof(__fp16));
    //     dinp    = (__fp16*)snrt_l1alloc(B * T  * 3*C   * sizeof(__fp16));
    //     dpreatt = (__fp16*)snrt_l1alloc(B * NH * T * T * sizeof(__fp16));
    //     att     = (__fp16*)snrt_l1alloc(B * NH * T * T * sizeof(__fp16));
    //     datt    = (__fp16*)snrt_l1alloc(B * NH * T * T * sizeof(__fp16));
    //     dout    = (__fp16*)snrt_l1alloc(B * T  * C     * sizeof(__fp16));
    //     for (int i = 0; i < 3; i++){
    //         snrt_dma_start_1d(inp    + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(__fp16));
    //         snrt_dma_start_1d(dinp   + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(__fp16));
    //     }
    //     snrt_dma_start_1d(dpreatt                       , data1_dram, B * NH * T * T    * sizeof(__fp16));
    //     snrt_dma_start_1d(att                           , data1_dram, B * NH * T * T    * sizeof(__fp16));
    //     snrt_dma_start_1d(datt                          , data1_dram, B * NH * T * T    * sizeof(__fp16));
    //     snrt_dma_start_1d(dout                          , data1_dram, B * T * C         * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();    
    // start_kernel();
    // attention_backward(dinp, dpreatt, datt, dout, inp, att, scale, temp_mem, bth_tmp, dbth_tmp, B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(datt     , dattG   , B * NH * T * T);
    //     printf("CHECK RESULTS2\n");
    //     check_result(dpreatt  , dpreattG, B * NH * T * T);
    //     printf("CHECK RESULTS3\n");
    //     check_result(dinp     , dinpG   , B *  T * 3*C);        
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- gelu_forward
    // int N = B*T*C;
    // if (cid == 0) {        
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     out     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     snrt_dma_start_1d(inp, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();    
    // start_kernel();
    // gelu_forward(out, inp, temp, temp2, B*T*C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out     , outG   , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- gelu_backward
    // int N = B*T*C;
    // if (cid == 0) {
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     dinp    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     dout    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dout, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();    
    // start_kernel();    
    // gelu_backward(dinp, inp, dout, temp, temp1, temp2, B*T*C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dinp     , dinpG   , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- residual_forward
    // int N = B*T*C;
    // if (cid == 0) {
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     dinp    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     out     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // residual_forward(out, inp, dinp, B*T*C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out     , outG   , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- residual_backward
    // int N = B*T*C;
    // if (cid == 0) {
    //     inp     = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     dinp    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     dout    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(dout, data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // residual_backward(inp, dinp, dout, B*T*C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(inp      , inpG    , B * T * C);   
    //     printf("CHECK RESULTS2\n");
    //     check_result(dinp     , dinpG   , B * T * C);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- softmax_forward
    // int V  = C;
    // int Vp = C;
    // fp_bits_t* sum;
    // fp_bits_t* maxval;
    // if (cid == 0) {
    //     sum       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     maxval       = (fp_bits_t*)snrt_l1alloc(sizeof(fp_bits_t));
    //     inp    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     out    = (__fp16*)snrt_l1alloc(B * T  * C   * sizeof(__fp16));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // softmax_forward(out, inp, temp, sum, maxval, B, T, C, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , probsG    , B * T * C);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- crossentropy_forward
    // int V  = C;
    // int Vp = C;
    // if (cid == 0) {
    //     losses       = (__fp16*)snrt_l1alloc(B * T         * sizeof(__fp16));
    //     probs        = (__fp16*)snrt_l1alloc(B * T  * Vp   * sizeof(__fp16));
    //     targets      = (int  *)snrt_l1alloc(B * T         * sizeof(int));
    //     snrt_dma_start_1d(probs  , data1_dram, B * T * Vp * sizeof(__fp16));
    //     snrt_dma_start_1d(targets, inp_dram  , B * T      * sizeof(int));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // crossentropy_forward(losses, probs, targets, B, T, C);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(losses      , lossesG    , B * T);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- crossentropy_softmax_backward
    // int V  = C;
    // int Vp = C;
    // if (cid == 0) {
    //     dlosses     = (__fp16*)snrt_l1alloc(B * T  * sizeof(__fp16));
    //     dlogits     = (__fp16*)snrt_l1alloc(B * T  * Vp   * sizeof(__fp16));
    //     probs       = (__fp16*)snrt_l1alloc(B * T  * Vp   * sizeof(__fp16));
    //     targets     = (int  *)snrt_l1alloc(B * T  * sizeof(int));
    //     snrt_dma_start_1d(dlosses, data1_dram, B * T * sizeof(__fp16));
    //     snrt_dma_start_1d(dlogits, data1_dram, B * T * Vp * sizeof(__fp16));
    //     snrt_dma_start_1d(probs  , data1_dram, B * T * Vp * sizeof(__fp16));
    //     snrt_dma_start_1d(targets, inp_dram  , B * T * sizeof(int));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // crossentropy_softmax_backward(dlogits, dlosses, probs, targets, B, T, V, Vp, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dlogits      , dlogitsG    , B * T * Vp);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- fast_exp
    // if (cid == 0) {
    //     inp       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     out       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < ((B*T*C)/VLMAX); i++){
    //     fast_exp(inp+i*VLMAX, out+i*VLMAX, 1, 0, 8, VLMAX, vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- fast_cosh
    // if (cid == 0) {
    //     inp       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     out       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < ((B*T*C)/(VLMAX)); i++){
    //     // fast_cosh(inp+i*(VLMAX/2), out+i*(VLMAX/2), temp , (VLMAX/2), vl);
    //     fast_cosh_v2(inp+i*(VLMAX), out+i*(VLMAX), temp2, 1, 8, (VLMAX), vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- fast_tanh
    // if (cid == 0) {
    //     inp       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     out       = (__fp16*)snrt_l1alloc(B * T * C * sizeof(__fp16));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(__fp16));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < ((B*T*C)/(VLMAX)); i++){
    //     // fast_tanh(inp+i*(VLMAX/2), out+i*(VLMAX/2), temp , (VLMAX/2), vl);
    //     fast_tanh_v2(inp+i*(VLMAX), out+i*(VLMAX), temp1, 1, 16, (VLMAX), vl);
    //     // fast_tanh_v3(inp+i*(VLMAX), out+i*(VLMAX), temp1, 1, 16, (VLMAX), vl);

    // }   
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    //---------------------------------------------------------


    // End timer and check if new best runtime
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    snrt_cluster_hw_barrier();
    // Display runtime
    if (cid == 0) {
        printf("The execution took %u cycles.\n", timer);
    }

    // if (cid == 0) {
    //     int error =
    //         verify_matrix(c, (const __fp16 *)gemm_checksum, gemm_l.M, gemm_l.N);

    //     if (error != 0) {
    //     printf("Error core %d: c[%d]=%u\n", cid, error, (int)c[error]);
    //     return error;
    //     }
    // }

    return errors;


}
