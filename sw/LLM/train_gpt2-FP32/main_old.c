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
#include "data/data.h"
#include "golden/gold.h"
#include "benchmark/benchmark.c"
//=====================================
#include "sp-fmatmul.c"
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
#define VLMAX 128

void check_result(float *x,float *ref, int r){    
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

union {
    float f;
    uint32_t i;
} result;
void fast_exp(float* inp, float* out, int memload, int vnum, int lmul, int size, unsigned int vl){//size <= vlmax
    if(memload){
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle32.v v24, (%0)" :: "r"(inp));
        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));
        // asm volatile("vse32.v v24, (%0)" :: "r"(out));
        // for (int i = 0; i < size; i++){
        //     result.f = out[i];
        //     result.i = (uint32_t)result.f;
        //     out[i] = result.f;
        // }
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vse32.v v16, (%0)" :: "r"(out));

    } else {
        if(vnum == 0){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
            asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
            asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
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

void fast_cosh_v2(float* inp, float* out, float* temp, int memload ,int vnum,int size, unsigned int vl){
    if(memload){
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
        asm volatile("vle32.v v24, (%0)" :: "r"(inp));
        
        asm volatile("vfmul.vf v16, v24, %[A]" ::[A] "f"(-1.0f));

        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(1064866805.0f));

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
        asm volatile("vfdiv.vf    v16, v16, %[A]" ::[A] "f"(2.0f));
        asm volatile("vse32.v v16, (%0)" :: "r"(out));
    } else if(vnum == 8){
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
        // asm volatile("vle32.v v8, (%0)" :: "r"(inp));
        
        asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(-1.0f));

        asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(1064866805.0f));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(1064866805.0f));

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
        asm volatile("vfdiv.vf    v8, v16, %[A]" ::[A] "f"(2.0f));
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

void fast_tanh_v2(float* inp, float* out, float* temp, int memload ,int vnum,int size, unsigned int vl){
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
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
            asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(2.0f));
    
            asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(12102203.0f));
            asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(1064866805.0f));
    
            // asm volatile("vse32.v v16, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v16, (%0)" :: "r"(temp));
            // asm volatile("vfcvt.rtz.xu.f.v v16, v16");

            
            asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(1.0f));
            asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(1.0f));
    
            // asm volatile("vfdiv.vv v8, v16, v8");
            // asm volatile("vse32.v v28, (%0)" :: "r"(out));            
        }else if(vnum == 16){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vle32.v v12, (%0)" :: "r"(inp));
            asm volatile("vfmul.vf v24, v16, %[A]" ::[A] "f"(2.0f));
    
            asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
            asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));
    
            // asm volatile("vse32.v v24, (%0)" :: "r"(temp));
            // for (int i = 0; i < size; i++){
            //     result.f = temp[i];
            //     result.i = (uint32_t)result.f;
            //     temp[i] = result.f;
            // }
            // asm volatile("vle32.v v24, (%0)" :: "r"(temp));
            asm volatile("vfcvt.rtz.xu.f.v v24, v24");

    
            asm volatile("vfadd.vf v16, v24, %[A]" ::[A] "f"(1.0f));
            asm volatile("vfsub.vf v24, v24, %[A]" ::[A] "f"(1.0f));
    
            asm volatile("vfdiv.vv v16, v24, v16");   
        }
    }
}
// ----------------------------------------------------------------------------
// all the individual layers' forward and backward passes
// B = batch_size, T = sequence_length, C = channels, V = vocab_size

void encoder_forward(float* out,
    int* inp, float* wte, float* wpe,
    int B, int T, int C,
    unsigned int vl) {
    // out is (B,T,C). At each position (b,t), a C-dimensional vector summarizing token & position
    // inp is (B,T) of integers, holding the token ids at each (b,t) position
    // wte is (V,C) of token embeddings, short for "weight token embeddings"
    // wpe is (maxT,C) of position embeddings, short for "weight positional embedding"
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the output position in out[b,t,:]
            float* out_bt = out + b * T * C + t * C;
            // get the index of the token at inp[b, t]
            int ix = inp[b * T + t];
            // seek to the position in wte corresponding to the token
            float* wte_ix = wte + ix * C;
            // seek to the position in wpe corresponding to the position
            float* wpe_t = wpe + t * C;
            // add the two vectors and store the result in out[b,t,:]

            // WTE_IX V0 -V8
            // WPE_T  V8 -V15
            // out_bt V16-V23
            int itr = 0;
            int num_elements = C;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(wte_ix+itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(wpe_t +itr*VLMAX));
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(wte_ix+itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(wpe_t +itr*VLMAX));
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
            }
        }
    }
}

void encoder_backward(float* dwte, float* dwpe,
                      float* dout, int* inp,
                      int B, int T, int C,
                      unsigned int vl) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dout_bt = dout + b * T * C + t * C;
            int ix = inp[b * T + t];
            float* dwte_ix = dwte + ix * C;
            float* dwpe_t = dwpe + t * C;

            //DWTE_IX V0 -V7
            //DWPE_T  V8 -V15
            //DOUT_BT V16-V23
            int itr = 0;
            int num_elements = C;            
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(dwte_ix +itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(dwpe_t  +itr*VLMAX));
                asm volatile("vle32.v v16, (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vfadd.vv v0, v0, v16");
                asm volatile("vfadd.vv v8, v8, v16");
                asm volatile("vse32.v v0 , (%0)" :: "r"(dwte_ix +itr*VLMAX));
                asm volatile("vse32.v v8 , (%0)" :: "r"(dwpe_t  +itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(dwte_ix +itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(dwpe_t  +itr*VLMAX));
                asm volatile("vle32.v v16, (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vfadd.vv v0, v0, v16");
                asm volatile("vfadd.vv v8, v8, v16");
                asm volatile("vse32.v v0 , (%0)" :: "r"(dwte_ix +itr*VLMAX));
                asm volatile("vse32.v v8 , (%0)" :: "r"(dwpe_t  +itr*VLMAX));
            }
        }        
    }
}

void layernorm_forward(float* out, float* mean, float* rstd,
                       float* inp, float* weight, float* bias,
                       int B, int T, int C,
                       unsigned int vl) {
    // reference: https://pytorch.org/docs/stable/generated/torch.nn.LayerNorm.html
    // both inp and out are (B,T,C) of the activations
    // mean and rstd are (B,T) buffers, to be used later in backward pass
    // at each position (b,t) of the input, the C-dimensional vector
    // of activations gets normalized, then scaled and shifted
    float eps = 1e-5f;
    float t0 = 0;
    float m;
    float v;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // seek to the input position inp[b,t,:]
            float* x = inp + b * T * C + t * C;
            // calculate the mean
            m = 0.0f;
            //X V0 - V7
            //m V8
            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            int itr = 0;
            int num_elements = C;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(x +itr*VLMAX));
                asm volatile("vfredsum.vs v8 , v0 , v31");
                asm volatile("vfmv.f.s %0 , v8" : "=f"(m));
                asm volatile("vfmv.s.f v31, %0" : "=f"(m));//BUG
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(x +itr*VLMAX));
                asm volatile("vfredsum.vs v8 , v0 , v31");
                asm volatile("vfmv.f.s %0 , v8" : "=f"(m));
            }
            m = m/C;
            // cache the mean for the backward pass later
            mean[b * T + t] = m;//I MOVE THIS HERE OTHERWISE DOES NOT WORK
            // calculate the variance (without any bias correction)
            v = 0.0f;
            //X        V0 - V7
            //XSHIFT   V8 - V15
            //XSHIFT^2 V8 - V15
            //V        V16
            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            itr = 0;
            num_elements = C;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            snrt_cluster_hw_barrier();//IT SHOULD BE HERE OTEHRWISE IT STUCKS

            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(x +itr*VLMAX));
                asm volatile("vfsub.vf v8, v0, %[A]" ::[A] "f"(m));
                asm volatile("vfmul.vv v8, v8, v8");
                asm volatile("vfredsum.vs v16 , v8 , v31");
                asm volatile("vfmv.f.s %0 , v16" : "=f"(v));
                asm volatile("vfmv.s.f v31, %0 " : "=f"(v));//BUG
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(x +itr*VLMAX));
                asm volatile("vfsub.vf v8, v0, %[A]" ::[A] "f"(m));
                asm volatile("vfmul.vv v8, v8, v8");
                asm volatile("vfredsum.vs v16 , v8 , v31");
                asm volatile("vfmv.f.s %0 , v16" : "=f"(v));
            }
            v = v/C;
            // calculate the rstd (reciprocal standard deviation)
            float s = 1.0f / sqrtf(v + eps);
            snrt_cluster_hw_barrier();// BY REMOVING THiS IT DOES NOT WORK. MATBE THE SAME ISSUE AS DIV THAT IS NOT PIPELINED
            // cache the rstd for the backward pass later
            // seek to the output position in out[b,t,:]
            float* out_bt = out + b * T * C + t * C;
            //X        V0 - V7
            //X-M      V0 - V7
            //S*(X-M)  V0 - V7
            //WEIGHT   V8 - V15
            //N*WEIGHT V8 - V15
            //XSHIFT^2 V8 - V15
            //V        V16
            itr = 0;
            num_elements = C;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0  , (%0)" :: "r"(x      +itr*VLMAX));
                asm volatile("vle32.v v8  , (%0)" :: "r"(weight +itr*VLMAX));
                asm volatile("vle32.v v16 , (%0)" :: "r"(bias   +itr*VLMAX));
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(m));
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(s));
                asm volatile("vfmul.vv v8 , v0, v8");
                asm volatile("vfadd.vv v16, v8, v16");
                asm volatile("vse32.v v16 , (%0)" :: "r"(out_bt+itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0  , (%0)" :: "r"(x      +itr*VLMAX));
                asm volatile("vle32.v v8  , (%0)" :: "r"(weight +itr*VLMAX));
                asm volatile("vle32.v v16 , (%0)" :: "r"(bias   +itr*VLMAX));
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(m));
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(s));
                asm volatile("vfmul.vv v8 , v0, v8");
                asm volatile("vfadd.vv v16, v8, v16");
                asm volatile("vse32.v v16 , (%0)" :: "r"(out_bt+itr*VLMAX));
            }
            rstd[b * T + t] = s;
        }
    }
}

void layernorm_backward(float* dinp, float* dweight, float* dbias,
                        float* dout, float* inp, float* weight, float* mean, float* rstd,
                        int B, int T, int C,
                        unsigned int vl) {
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dout_bt = dout + b * T * C + t * C;
            float* inp_bt  = inp  + b * T * C + t * C;
            float* dinp_bt = dinp + b * T * C + t * C;
            float  mean_bt = mean[b * T + t];
            float  rstd_bt = rstd[b * T + t];

            // first: two reduce operations
            float dnorm_mean = 0.0f;
            float dnorm_norm_mean = 0.0f;

            //INP_BT   V0 - V7
            //NORM_BTI V0 - V7
            //WEIGHT   V8 - V15
            //DOUT_BT  V16- V23
            //DNORM_I  V8 - V15
            //dnorm_mean v24
            //dnorm_norm_mean v25

            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            asm volatile("vfsub.vv v30,v30,v30");

            int itr = 0;
            int num_elements = C;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0  , (%0)" :: "r"(inp_bt  +itr*VLMAX));
                asm volatile("vle32.v v8  , (%0)" :: "r"(weight  +itr*VLMAX));
                asm volatile("vle32.v v16 , (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(mean_bt));
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(rstd_bt));
                asm volatile("vfmul.vv v8 , v8, v16");
                asm volatile("vfredsum.vs v24 , v8  , v31");
                asm volatile("vfmv.f.s %0 , v24" : "=f"(dnorm_mean));
                asm volatile("vfmv.s.f v31, %0"  : "=f"(dnorm_mean));//BUG

                asm volatile("vfmul.vv    v16 , v8  , v0");
                asm volatile("vfredsum.vs v25 , v16 , v30");
                asm volatile("vfmv.f.s %0 , v25" : "=f"(dnorm_norm_mean));
                asm volatile("vfmv.s.f v30, %0"  : "=f"(dnorm_norm_mean));//BUG

                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0  , (%0)" :: "r"(inp_bt  +itr*VLMAX));
                asm volatile("vle32.v v8  , (%0)" :: "r"(weight  +itr*VLMAX));
                asm volatile("vle32.v v16 , (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(mean_bt));
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(rstd_bt));
                asm volatile("vfmul.vv v8 , v8, v16");
                asm volatile("vfredsum.vs v24 , v8  , v31");

                asm volatile("vfmv.f.s %0 , v24" : "=f"(dnorm_mean));
                asm volatile("vfmul.vv    v16 , v8  , v0");
                asm volatile("vfredsum.vs v25 , v16 , v30");
                asm volatile("vfmv.f.s %0 , v25" : "=f"(dnorm_norm_mean));
            }

            dnorm_mean = dnorm_mean / C;
            dnorm_norm_mean = dnorm_norm_mean / C;

            //INP_BT    V0 - V7
            //NORM_BTI  V0 - V7
            //DOUT_BT   V8 - V15
            //dbias     V16- V23/ADD RES
            //dweight   V16- V23/ADD RES
            //weight    V16- V23
            //dnorm_i   V8 - V15
            //----------------------------- 
            //NORM_BTI  V0 - V7
            //dnorm_i   V8 - V15
            //----------------------------- 
            //dinp_bt   V16- v23
            //NORM_BTI* V0 - V7
            //dnorm_i-  V8 - V15
            //dnorm_i-- V8 - V15

            itr = 0;
            num_elements = C;
            // now iterate again and accumulate all the gradients
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v   v0 , (%0)" :: "r"(inp_bt  +itr*VLMAX));
                asm volatile("vfsub.vf  v0 , v0, %[A]" ::[A] "f"(mean_bt));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(rstd_bt));

                asm volatile("vle32.v   v8 , (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vle32.v   v16, (%0)" :: "r"(dbias   +itr*VLMAX));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dbias   +itr*VLMAX));
                
                asm volatile("vle32.v   v16, (%0)" :: "r"(dweight +itr*VLMAX));
                asm volatile("vfmacc.vv v16, v8, v0");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dweight +itr*VLMAX));

                asm volatile("vle32.v   v16, (%0)" :: "r"(weight  +itr*VLMAX));
                asm volatile("vfmul.vv  v8 , v8, v16");
                //---------------------------------------------------------------
                asm volatile("vle32.v   v16, (%0)" :: "r"(dinp_bt +itr*VLMAX));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(dnorm_norm_mean));
                asm volatile("vfsub.vf  v8 , v8, %[A]" ::[A] "f"(dnorm_mean));
                asm volatile("vfsub.vv  v8 , v8, v0");
                asm volatile("vfmul.vf  v8 , v8, %[A]" ::[A] "f"(rstd_bt));
                asm volatile("vfadd.vv  v16 , v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dinp_bt +itr*VLMAX));

                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v   v0 , (%0)" :: "r"(inp_bt  +itr*VLMAX));
                asm volatile("vfsub.vf  v0 , v0, %[A]" ::[A] "f"(mean_bt));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(rstd_bt));

                asm volatile("vle32.v   v8 , (%0)" :: "r"(dout_bt +itr*VLMAX));
                asm volatile("vle32.v   v16, (%0)" :: "r"(dbias   +itr*VLMAX));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dbias   +itr*VLMAX));
                
                asm volatile("vle32.v   v16, (%0)" :: "r"(dweight +itr*VLMAX));
                asm volatile("vfmacc.vv v16, v8, v0");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dweight +itr*VLMAX));

                asm volatile("vle32.v   v16, (%0)" :: "r"(weight  +itr*VLMAX));
                asm volatile("vfmul.vv  v8 , v8, v16");
                //---------------------------------------------------------------
                asm volatile("vle32.v   v16, (%0)" :: "r"(dinp_bt +itr*VLMAX));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(dnorm_norm_mean));
                asm volatile("vfsub.vf  v8 , v8, %[A]" ::[A] "f"(dnorm_mean));
                asm volatile("vfsub.vv  v8 , v8, v0");
                asm volatile("vfmul.vf  v8 , v8, %[A]" ::[A] "f"(rstd_bt));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dinp_bt +itr*VLMAX));
            }
        }
    }
}

void matmul_forward_naive(float* out,
                        const float* inp, const float* weight, float* weightT, const float* bias,
                        int B, int T, int C, int OC,
                        unsigned int vl) {

    // THIS FUNCTION IN THE LLM.C MULTIPLY INP BY TRANSPOSED OF WEIGHT.
    // SO FIRST TRANSPOSED THE WEIGHT IN WEIGHTT AND THEN USE SPATZ MATHMUL.
    // for (int i = 0; i < OC; i++){
    //     for (int j = 0; j < C; j++){
    //         weightT[j*OC+i] = weight[i*C+j];
    //     }
    // }

    // matmul_8xVL(out, inp, weightT, 0, B*T, C, OC, 0, OC);
    matmul_4xVL(out, inp, weight, 0, B*T, C, OC, 0, OC);
    // IF THE MATMUL SUPPORTS GEMM WE SHOULD FIRST INITIAL OUT WITH BIAS AND THEN USE MATHMUL.
    // NOW JUST MATMUL AND THEN ADDING BIAS
    // for (int b = 0; b < B; b++) {
    //     for (int t = 0; t < T; t++) {
    //         int bt = b * T + t;
    //         for (int o = 0; o < OC; o++) {
    //             float val = (bias != NULL) ? bias[o] : 0.0f;
    //             out[bt * OC + o] += val;
    //         }
    //     }
    // }
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            int out_bt = out + (b * T + t)*OC;
            int itr = 0;
            int num_elements = OC;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            asm volatile("vle32.v v8 , (%0)" :: "r"(bias +itr*VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(bias +itr*VLMAX));
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
            }
        }
    }    
}


void matmul_forward_naive_res(float* out, const float* residual,
                        const float* inp, const float* weight, float* weightT, const float* bias,
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
            float* out_bt = out + (b * T + t)*OC;
            const float* res_bt = residual + (b * T + t)*OC;
            int itr = 0;
            int num_elements = OC;
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(bias  +itr*VLMAX));
                asm volatile("vle32.v v16, (%0)" :: "r"(res_bt+itr*VLMAX));
                asm volatile("vfadd.vv v0 , v0, v8");
                asm volatile("vfadd.vv v0 , v0, v16");
                asm volatile("vse32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
                asm volatile("vle32.v v8 , (%0)" :: "r"(bias  +itr*VLMAX));
                asm volatile("vle32.v v16, (%0)" :: "r"(res_bt+itr*VLMAX));
                asm volatile("vfadd.vv v0 , v0, v8");
                asm volatile("vfadd.vv v0 , v0, v16");
                asm volatile("vse32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
            }
        }
    }
}


void matmul_backward(float* dinp, float* dinp_t, float* dweight, float* dweight_t, float* dbias,
                    const float* dout, float* doutT, const float* inp, const float* weight,
                    int B, int T, int C, int OC,
                    unsigned int vl) {
    // most of the running time is spent here and in matmul_forward
    // this backward could be done in a single "round" of loops
    // but that doesn't afford an efficient parallelization strategy

    // IF THE MATMUL SUPPORTS GEMM WE SHOULD FIRST INITIAL OUT WITH BIAS AND THEN USE MATHMUL.
    // NOW JUST MATMUL AND THEN ADDING BIAS
    
    matmul_4xVL(dinp, dout, weight, 0, B*T, OC, C, 0, C);
    // matmul_4xVL(dinp_t, dout, weight, 0, B*T, OC, C, 0, C);
    // int itr = 0;
    // int num_elements = B*T*C;
    // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){
    //     asm volatile("vle32.v v0  , (%0)" :: "r"(dinp_t  +itr*VLMAX));
    //     asm volatile("vle32.v v8  , (%0)" :: "r"(dinp    +itr*VLMAX));
    //     asm volatile("vfadd.vv v8 , v0, v8");
    //     asm volatile("vse32.v v8  , (%0)" :: "r"(dinp    +itr*VLMAX));
    //     itr++;
    // }
    // if(itr*VLMAX < num_elements){
    //     asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    //     asm volatile("vle32.v v0  , (%0)" :: "r"(dinp_t  +itr*VLMAX));
    //     asm volatile("vle32.v v8  , (%0)" :: "r"(dinp    +itr*VLMAX));
    //     asm volatile("vfadd.vv v8 , v0, v8");
    //     asm volatile("vse32.v v8  , (%0)" :: "r"(dinp    +itr*VLMAX));
    // }
    // backward into weight/bias, parallelize over output channels OC
    
    // THIS FUNCTION IN THE LLM.C MULTIPLY INP BY TRANSPOSED OF WEIGHT.
    // SO FIRST TRANSPOSED THE WEIGHT IN WEIGHTT AND THEN USE SPATZ MATHMUL.
    // for (int i = 0; i < B*T; i++){
    //     for (int j = 0; j < OC; j++){
    //         doutT[j*B*T+i] = dout[i*OC+j];
    //     }
    // }

    // for (int i = 0; i < B*T; i++) {
    //     float* src = &dout[i * OC];
    //     float* dst = &doutT[i];  // destination starts at the column

    //     int j = 0;
    //     while (j < OC) {
    //         int vl;
    //         asm volatile (
    //             "vsetvli %[vl], %[oc_remain], e32\n\t"            // Set vector length
    //             : [vl] "=r" (vl)
    //             : [oc_remain] "r" (OC - j)
    //         );

    //         // Load from source (contiguous)
    //         asm volatile ("vle32.v v0, (%[src])" :: [src] "r" (&src[j]));

    //         // Strided store to destination
    //         asm volatile (
    //             "vsse32.v v0, (%[dst]), %[stride]":: [dst] "r" (&dst[j * B*T]),[stride] "r" (B*T * sizeof(float)));

    //         j += vl;
    //     }
    // }
    for (int i = 0; i < B*T; i++){
        float* src = dout + i * OC;
        float* dst = doutT + i;

        int itr = 0;
        int num_elements = OC;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            asm volatile("vle32.v v0  , (%0)" :: "r"(src  +itr*VLMAX));
            asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst +itr*VLMAX*B*T), "r"(B*T * sizeof(float)));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vle32.v v0  , (%0)" :: "r"(src  +itr*VLMAX));
            asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst +itr*VLMAX*B*T), "r"(B*T * sizeof(float)));
        }
    }


    matmul_4xVL(dweight, doutT, inp, 0, OC, B*T, C, 0, C);
    // matmul_4xVL(dweight_t, doutT, inp, 0, OC, B*T, C, 0, C);

    // int itr = 0;
    // int num_elements = OC*C;
    // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    // while(itr*VLMAX + VLMAX <= num_elements){
    //     asm volatile("vle32.v v0  , (%0)" :: "r"(dweight_t  +itr*VLMAX));
    //     asm volatile("vle32.v v8  , (%0)" :: "r"(dweight    +itr*VLMAX));
    //     asm volatile("vfadd.vv v8 , v0, v8");
    //     asm volatile("vse32.v v8  , (%0)" :: "r"(dweight    +itr*VLMAX));
    //     itr++;
    // }
    // if(itr*VLMAX < num_elements){
    //     asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    //     asm volatile("vle32.v v0  , (%0)" :: "r"(dweight_t  +itr*VLMAX));
    //     asm volatile("vle32.v v8  , (%0)" :: "r"(dweight    +itr*VLMAX));
    //     asm volatile("vfadd.vv v8 , v0, v8");
    //     asm volatile("vse32.v v8  , (%0)" :: "r"(dweight    +itr*VLMAX));
    // }    

    // for (int o = 0; o < OC; o++) {
    //     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
    //     asm volatile("vfsub.vv v31,v31,v31");
    //     asm volatile("vle32.v v31  , (%0)" :: "r"(dbias+o));
    //     // asm volatile("vfmv.s.f v31, %0"  : "=f"(dbias[o]));//THIS PART SHOULD BE CHECKED BECAUSE OF VFMV INSTRUCTION
    //     float *doutTaddress = doutT + o*B*T;
    //     int itr = 0;
    //     int num_elements = B*T;
    //     asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    //     while(itr*VLMAX + VLMAX <= num_elements){
    //         asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    //         asm volatile("vle32.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
    //         asm volatile("vfredsum.vs v24 , v0  , v31");
    //         asm volatile("vfmv.f.s %0 , v24" : "=f"(dbias[o]));
    //         //THESE TWO LINES SHOULD NE REPLACED BY VFMV.S.F
    //         asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(1));
    //         asm volatile("vle32.v v31  , (%0)" :: "r"(dbias+o));
    //         itr++;
    //     }
    //     if(itr*VLMAX < num_elements){
    //         asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    //         asm volatile("vle32.v v0  , (%0)" :: "r"(doutTaddress  +itr*VLMAX));
    //         asm volatile("vfredsum.vs v24 , v0  , v31");
    //         asm volatile("vfmv.f.s %0 , v24" : "=f"(dbias[o]));
    //     }
    // }



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


void matmul_backward_res(float* dinp, float* dinp_t, float* dresidual, float* dweight, float* dweight_t, float* dbias,
                    const float* dout, float* doutT, const float* inp, const float* weight,
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
    {
        int itr = 0;
        int num_elements = B*T*OC;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vle32.v v0, (%0)" :: "r"(dout      +itr*VLMAX));
            asm volatile("vle32.v v8, (%0)" :: "r"(dresidual +itr*VLMAX));
            asm volatile("vfadd.vv v8, v0, v8");
            asm volatile("vse32.v v8, (%0)" :: "r"(dresidual +itr*VLMAX));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vle32.v v0, (%0)" :: "r"(dout      +itr*VLMAX));
            asm volatile("vle32.v v8, (%0)" :: "r"(dresidual +itr*VLMAX));
            asm volatile("vfadd.vv v8, v0, v8");
            asm volatile("vse32.v v8, (%0)" :: "r"(dresidual +itr*VLMAX));
        }
    }

    // gradient into the matmul branch -- identical to matmul_backward, using `dout` directly
    matmul_4xVL(dinp, dout, weight, 0, B*T, OC, C, 0, C);

    for (int i = 0; i < B*T; i++){
        float* src = dout + i * OC;
        float* dst = doutT + i;

        int itr = 0;
        int num_elements = OC;
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        while(itr*VLMAX + VLMAX <= num_elements){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            asm volatile("vle32.v v0  , (%0)" :: "r"(src  +itr*VLMAX));
            asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst +itr*VLMAX*B*T), "r"(B*T * sizeof(float)));
            itr++;
        }
        if(itr*VLMAX < num_elements){
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            asm volatile("vle32.v v0  , (%0)" :: "r"(src  +itr*VLMAX));
            asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst +itr*VLMAX*B*T), "r"(B*T * sizeof(float)));
        }
    }


    matmul_4xVL(dweight, doutT, inp, 0, OC, B*T, C, 0, C);
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



void attention_forward(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                // pass 1: calculate query dot key and maxval
                float maxval = -10000.0f; // TODO something better
                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key

                    // (query_t) dot (key_t2)
                    float val = 0.0f;                    
                    //query_t V0 - V7
                    //key_t2  V8 - V15
                    //mul     v8 - v15
                    //val     v16
                    int itr = 0;
                    int num_elements = hs;

                    asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                    asm volatile("vfsub.vv v31,v31,v31");
                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t+itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(key_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v16 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v16" : "=f"(val));
                        // asm volatile("vfmv.s.f    v31, %0"  : "=f"(val));
                        
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t+itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(key_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v16 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v16" : "=f"(val));
                    }
                    val *= scale;
                    if (val > maxval) {
                        maxval = val;
                    }

                    preatt_bth[t2] = val;
                }


                // pass 2: calculate the exp and keep track of sum
                // maxval is being calculated and subtracted only for numerical stability
                // float expsum = 0.0f;
                // for (int t2 = 0; t2 <= t; t2++) {
                //     float expv = expf(preatt_bth[t2] - maxval);//I CHANGE THIS PART
                //     // float expv = preatt_bth[t2] - maxval;
                //     expsum += expv;
                //     att_bth[t2] = expv;
                // }

                // pass 2: calculate the exp and keep track of sum
                // maxval is being calculated and subtracted only for numerical stability
                float expsum = 0.0f;
                //preatt_bth V0 - V7
                //expsum     V8
                int itr = 0;
                int num_elements = t+1;

                asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfsub.vv v31,v31,v31");
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while(itr*VLMAX + VLMAX <= num_elements){

                    asm volatile("vle32.v     v0 , (%0)" :: "r"(preatt_bth+itr*VLMAX));
                    asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval));
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
                    asm volatile("vfmv.f.s    %0 , v8" : "=f"(expsum));
                    // asm volatile("vfmv.s.f    v31, %0"  : "=f"(expsum));
                    asm volatile("vse32.v     v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    itr++;
                }
                if(itr*VLMAX < num_elements){
                    asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle32.v     v0 , (%0)" :: "r"(preatt_bth+itr*VLMAX));
                    asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval));
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
                    asm volatile("vfmv.f.s    %0 , v8" : "=f"(expsum));
                    asm volatile("vse32.v     v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                }



                float expsum_inv = expsum == 0.0f ? 0.0f : 1.0f / expsum;
                snrt_cluster_hw_barrier();//ISSUE
                // pass 3: normalize to get the softmax
                itr = 0;
                num_elements = t+1;
                // num_elements = C;
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while(itr*VLMAX + VLMAX <= num_elements){
                    asm volatile("vle32.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(expsum_inv));
                    asm volatile("vse32.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    itr++;
                }
                if(itr*VLMAX < num_elements){
                    asm volatile("vsetvli   %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle32.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                    asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(expsum_inv));
                    asm volatile("vse32.v   v0 , (%0)" :: "r"(att_bth+itr*VLMAX));
                }
                for (int t2 = t+1; t2 < T; t2++) {
                    // causal attention mask. not strictly necessary to set to zero here
                    // only doing this explicitly for debugging and checking to PyTorch
                    att_bth[t2] = 0.0f;
                }                

                // pass 4: accumulate weighted values into the output of attention
                itr = 0;
                num_elements = hs;
                float* out_bth = out + b * T * C + t * C + h * hs;
                float zero = 0.0f;
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                asm volatile("vfsub.vv v0, v0, v0");//ISSUE
                // asm volatile("vfmv.v.f v0, %[A]" ::[A] "f"(zero));//ISSUE
                while(itr*VLMAX + VLMAX <= num_elements){
                    asm volatile("vse32.v v0 , (%0)" :: "r"(out_bth+itr*VLMAX));
                    itr++;
                }
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                if(itr*VLMAX < num_elements){
                    asm volatile("vse32.v v0 , (%0)" :: "r"(out_bth+itr*VLMAX));
                }
                for (int t2 = 0; t2 <= t; t2++) {
                    float* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    float att_btht2 = att_bth[t2];
                    itr = 0;
                    num_elements = hs;

                    asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2+itr*VLMAX));
                        asm volatile("vle32.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(att_btht2));
                        asm volatile("vse32.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2+itr*VLMAX));
                        asm volatile("vle32.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                        asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(att_btht2));
                        asm volatile("vse32.v    v8 , (%0)" :: "r"(out_bth +itr*VLMAX));
                    }
                }
            }
        }
    }
}


void attention_forward_v2(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    int stridesize_q = C3*4;//C3 * 4 byte (for column-wise access)
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) { 
            for (int t = 0; t < T; t+=4) {
                // printf("t = %d\n",t);
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;
                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vfsub.vv v16,v4,v4");
                asm volatile("vmv.v.v  v20, v16");      
                asm volatile("vmv.v.v  v24, v16");      
                asm volatile("vmv.v.v  v28, v16");      


                // pass 1: calculate query dot key and maxval
                // float maxval = -10000.0f; // TODO something better
                float* key_t2 = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t));//SET THE LMUL TO 4 AS T is 64

                for (int i = 0; i < hs; i++) {

                    float key1 = key_t2[i+0*C3];
                    float key2 = key_t2[i+1*C3];
                    float key3 = key_t2[i+2*C3];
                    float key4 = key_t2[i+3*C3];

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v16 , %[A], v0" ::[A] "f"(key1));

                    asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                    asm volatile("vfmacc.vf  v20 , %[A], v4" ::[A] "f"(key2));

                    asm volatile("vslide1down.vx v8 , v4, %[A]" ::[A] "r"(0));
                    asm volatile("vfmacc.vf  v24 , %[A], v8" ::[A] "f"(key3));

                    asm volatile("vslide1down.vx v12, v8, %[A]" ::[A] "r"(0));
                    asm volatile("vfmacc.vf  v28 , %[A], v12" ::[A] "f"(key4));

                }

                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v20 , v20, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v28 , v28, %[A]" ::[A] "f"(scale));
                
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+0 + 0*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v20, (%0), %1" ::"r"(preatt_bth + t+1 + 1*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-2));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth + t+2 + 2*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-3));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v28, (%0), %1" ::"r"(preatt_bth + t+3 + 3*T), "r"(stridesize_patt));
            }
        }
    }
}


void attention_forward_v3(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    int stridesize_q = C3*4;//C3 * 4 byte (for column-wise access)
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) { 
            for (int t = 0; t < T; t+=4) {
                // printf("t = %d\n",t);
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vfsub.vv v16,v4,v4");
                asm volatile("vmv.v.v  v20, v16");      
                asm volatile("vmv.v.v  v24, v16");      
                asm volatile("vmv.v.v  v28, v16");      


                // pass 1: calculate query dot key and maxval
                // float maxval = -10000.0f; // TODO something better
                float* key_t2 = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                for (int i = 0; i < hs; i++) {

                    float key1 = key_t2[i+0*C3];
                    float key2 = key_t2[i+1*C3];
                    float key3 = key_t2[i+2*C3];
                    float key4 = key_t2[i+3*C3];

                    asm volatile("vfmacc.vf  v16 , %[A], v0" ::[A] "f"(key1));

                    asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                    asm volatile("vfmacc.vf  v20 , %[A], v4" ::[A] "f"(key2));


                    asm volatile("vslide1down.vx v8 , v4, %[A]" ::[A] "r"(0));
                    snrt_cluster_hw_barrier();
                    asm volatile("vfmacc.vf  v24 , %[A], v8" ::[A] "f"(key3));
                    // printf("HI\n");


                    asm volatile("vslide1down.vx v12, v8, %[A]" ::[A] "r"(0));
                    asm volatile("vfmacc.vf  v28 , %[A], v12" ::[A] "f"(key4));

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));

                }

                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v20 , v20, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v28 , v28, %[A]" ::[A] "f"(scale));
                
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+0 + 0*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v20, (%0), %1" ::"r"(preatt_bth + t+1 + 1*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-2));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth + t+2 + 2*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-3));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v28, (%0), %1" ::"r"(preatt_bth + t+3 + 3*T), "r"(stridesize_patt));
            }
        }
    }
}

void attention_forward_v4(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    int stridesize_q = C3*4;//C3 * 4 byte (for column-wise access)
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) { 
            for (int t = 0; t < T; t+=4) {
                // printf("t = %d\n",t);
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vfsub.vv v16,v4,v4");
                asm volatile("vmv.v.v  v20, v16");      
                asm volatile("vmv.v.v  v24, v16");      
                asm volatile("vmv.v.v  v28, v16");

                float key1;
                float key2;
                float key3;
                float key4;
                float key5;
                float key6;
                float key7;
                float key8;

                // pass 1: calculate query dot key and maxval
                // float maxval = -10000.0f; // TODO something better
                float* key_t2 = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                key1 = key_t2[0*C3];
                key2 = key_t2[1*C3];
                key3 = key_t2[2*C3];
                key4 = key_t2[3*C3];
                key5 = key_t2[1+0*C3];
                key6 = key_t2[1+1*C3];
                key7 = key_t2[1+2*C3];
                key8 = key_t2[1+3*C3];

                for (int i = 0; i < hs; i+=2) {

                    asm volatile("vlse32.v v4, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v8  , %[A], v0" ::[A] "f"(key1));
                    key1 = key_t2[i+2+0*C3];
                    asm volatile("vfmacc.vf  v12 , %[A], v0" ::[A] "f"(key2));
                    key2 = key_t2[i+2+1*C3];                    
                    asm volatile("vfmacc.vf  v16 , %[A], v0" ::[A] "f"(key3));
                    key3 = key_t2[i+2+2*C3];
                    asm volatile("vfmacc.vf  v20 , %[A], v0" ::[A] "f"(key4));
                    key4 = key_t2[i+2+3*C3];

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+2), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v8  , %[A], v4" ::[A] "f"(key5));
                    key5 = key_t2[i+3+0*C3];
                    asm volatile("vfmacc.vf  v12 , %[A], v4" ::[A] "f"(key6));
                    key6 = key_t2[i+3+1*C3];
                    asm volatile("vfmacc.vf  v16 , %[A], v4" ::[A] "f"(key7));
                    key7 = key_t2[i+3+2*C3];
                    asm volatile("vfmacc.vf  v20 , %[A], v4" ::[A] "f"(key8));
                    key8 = key_t2[i+3+3*C3];

                }


                // asm volatile("vslidedown.vx v0 , v12, %[A]" ::[A] "r"(1));
                // asm volatile("vslidedown.vx v4 , v16, %[A]" ::[A] "r"(2));
                // asm volatile("vslidedown.vx v24 , v20, %[A]" ::[A] "r"(3));

                asm volatile("vfmul.vf  v8 , v8, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v4 , v4, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale));
                
                asm volatile("vsse32.v v8, (%0), %1" ::"r"(preatt_bth + t+0 + 0*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v0, (%0), %1" ::"r"(preatt_bth + t+1 + 1*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-2));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v4, (%0), %1" ::"r"(preatt_bth + t+2 + 2*T), "r"(stridesize_patt));

                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(T-t-3));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth + t+3 + 3*T), "r"(stridesize_patt));
            }
        }
    }
}

void attention_forward_v5(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    int stridesize_q = C3*4;//C3 * 4 byte (for column-wise access)
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) { 
            for (int t = 0; t < T; t+=8) {
                // printf("t = %d\n",t);
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vfsub.vv v16,v4,v4");
                asm volatile("vmv.v.v  v20, v16");  
                asm volatile("vmv.v.v  v24, v16");
                asm volatile("vmv.v.v  v28, v16");

                float key01;float key11;
                float key02;float key12;
                float key03;float key13;
                float key04;float key14;
                float key05;float key15;
                float key06;float key16;
                float key07;float key17;
                float key08;float key18;

                // pass 1: calculate query dot key and maxval
                // float maxval = -10000.0f; // TODO something better
                float* key_t2 = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                int numElem1 = (T-t >> 1);
                int numElem2 = T-t - numElem1;
                
                //######## FIRST HALF

                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(numElem1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                key01 = key_t2[0*C3];
                key02 = key_t2[1*C3];
                key03 = key_t2[2*C3];
                key04 = key_t2[3*C3];
                key05 = key_t2[4*C3];
                key06 = key_t2[5*C3];
                key07 = key_t2[6*C3];
                key08 = key_t2[7*C3];

                key11 = key_t2[1+0*C3];
                key12 = key_t2[1+1*C3];
                key13 = key_t2[1+2*C3];
                key14 = key_t2[1+3*C3];
                key15 = key_t2[1+4*C3];
                key16 = key_t2[1+5*C3];
                key17 = key_t2[1+6*C3];
                key18 = key_t2[1+7*C3];

                for (int i = 0; i < hs; i+=2) {

                    asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                    key01 = key_t2[i+2+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                    key02 = key_t2[i+2+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                    key03 = key_t2[i+2+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                    key04 = key_t2[i+2+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                    key05 = key_t2[i+2+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                    key06 = key_t2[i+2+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                    key07 = key_t2[i+2+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                    key08 = key_t2[i+2+7*C3];                    

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+2), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                    key01 = key_t2[i+3+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                    key02 = key_t2[i+3+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                    key03 = key_t2[i+3+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                    key04 = key_t2[i+3+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                    key05 = key_t2[i+3+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                    key06 = key_t2[i+3+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                    key07 = key_t2[i+3+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                    key08 = key_t2[i+3+7*C3];
                }


                // asm volatile("vslidedown.vx v0 , v12, %[A]" ::[A] "r"(1));
                // asm volatile("vslidedown.vx v4 , v16, %[A]" ::[A] "r"(2));
                // asm volatile("vslidedown.vx v24 , v20, %[A]" ::[A] "r"(3));

                asm volatile("vfmul.vf  v4  , v4 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v4 , (%0), %1" ::"r"(preatt_bth + t+0), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v6  , v6 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v6 , (%0), %1" ::"r"(preatt_bth + t+1), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v8  , v8 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v8 , (%0), %1" ::"r"(preatt_bth + t+2), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v10 , v10, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v10, (%0), %1" ::"r"(preatt_bth + t+3), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v12 , v12, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(preatt_bth + t+4), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v14 , v14, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v14, (%0), %1" ::"r"(preatt_bth + t+5), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+6), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v18 , v18, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v18, (%0), %1" ::"r"(preatt_bth + t+7), "r"(stridesize_patt));

                //######## SECOND HALF
                query_t = inp + b * T * C3 + (t+numElem1) * C3 + h * hs;
                preatt_bth = preatt + b*NH*T*T + h*T*T + (t+numElem1)*T;
                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(numElem2));//SET THE LMUL TO 4 AS T is 64

                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                key01 = key_t2[0*C3];
                key02 = key_t2[1*C3];
                key03 = key_t2[2*C3];
                key04 = key_t2[3*C3];
                key05 = key_t2[4*C3];
                key06 = key_t2[5*C3];
                key07 = key_t2[6*C3];
                key08 = key_t2[7*C3];

                key11 = key_t2[1+0*C3];
                key12 = key_t2[1+1*C3];
                key13 = key_t2[1+2*C3];
                key14 = key_t2[1+3*C3];
                key15 = key_t2[1+4*C3];
                key16 = key_t2[1+5*C3];
                key17 = key_t2[1+6*C3];
                key18 = key_t2[1+7*C3];

                for (int i = 0; i < hs; i+=2) {

                    asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                    key01 = key_t2[i+2+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                    key02 = key_t2[i+2+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                    key03 = key_t2[i+2+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                    key04 = key_t2[i+2+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                    key05 = key_t2[i+2+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                    key06 = key_t2[i+2+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                    key07 = key_t2[i+2+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                    key08 = key_t2[i+2+7*C3];                    

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+2), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                    key01 = key_t2[i+3+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                    key02 = key_t2[i+3+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                    key03 = key_t2[i+3+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                    key04 = key_t2[i+3+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                    key05 = key_t2[i+3+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                    key06 = key_t2[i+3+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                    key07 = key_t2[i+3+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                    key08 = key_t2[i+3+7*C3];
                }


                // asm volatile("vslidedown.vx v0 , v12, %[A]" ::[A] "r"(1));
                // asm volatile("vslidedown.vx v4 , v16, %[A]" ::[A] "r"(2));
                // asm volatile("vslidedown.vx v24 , v20, %[A]" ::[A] "r"(3));

                asm volatile("vfmul.vf  v4  , v4 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v4 , (%0), %1" ::"r"(preatt_bth + t+0), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v6  , v6 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v6 , (%0), %1" ::"r"(preatt_bth + t+1), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v8  , v8 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v8 , (%0), %1" ::"r"(preatt_bth + t+2), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v10 , v10, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v10, (%0), %1" ::"r"(preatt_bth + t+3), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v12 , v12, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(preatt_bth + t+4), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v14 , v14, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v14, (%0), %1" ::"r"(preatt_bth + t+5), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+6), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v18 , v18, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v18, (%0), %1" ::"r"(preatt_bth + t+7), "r"(stridesize_patt));
            }
        }
    }
}


void attention_forward_v6(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;

    int stridesize_q = C3*4;//C3 * 4 byte (for column-wise access)
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) { 
            for (int t = 0; t < T; t+=8) {
                // printf("t = %d\n",t);
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;

                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(-1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vfsub.vv v16,v4,v4");
                asm volatile("vmv.v.v  v20, v16");  
                asm volatile("vmv.v.v  v24, v16");
                asm volatile("vmv.v.v  v28, v16");

                float key01;float key11;
                float key02;float key12;
                float key03;float key13;
                float key04;float key14;
                float key05;float key15;
                float key06;float key16;
                float key07;float key17;
                float key08;float key18;

                // pass 1: calculate query dot key and maxval
                // float maxval = -10000.0f; // TODO something better
                float* key_t2 = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                int numElem1 = (T-t >> 1);
                int numElem2 = T-t - numElem1;
                
                //######## FIRST HALF

                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(numElem1));//SET THE LMUL TO 4 AS T is 64
                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                // key01 = key_t2[0*C3];
                // key02 = key_t2[1*C3];
                // key03 = key_t2[2*C3];
                // key04 = key_t2[3*C3];
                // key05 = key_t2[4*C3];
                // key06 = key_t2[5*C3];
                // key07 = key_t2[6*C3];
                // key08 = key_t2[7*C3];

                // key11 = key_t2[1+0*C3];
                // key12 = key_t2[1+1*C3];
                // key13 = key_t2[1+2*C3];
                // key14 = key_t2[1+3*C3];
                // key15 = key_t2[1+4*C3];
                // key16 = key_t2[1+5*C3];
                // key17 = key_t2[1+6*C3];
                // key18 = key_t2[1+7*C3];

                for (int i = 0; i < hs-2; i+=2) {

                    asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                    key01 = key_t2[i+2+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                    key02 = key_t2[i+2+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                    key03 = key_t2[i+2+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                    key04 = key_t2[i+2+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                    key05 = key_t2[i+2+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                    key06 = key_t2[i+2+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                    key07 = key_t2[i+2+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                    key08 = key_t2[i+2+7*C3];                    

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+2), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                    key01 = key_t2[i+3+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                    key02 = key_t2[i+3+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                    key03 = key_t2[i+3+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                    key04 = key_t2[i+3+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                    key05 = key_t2[i+3+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                    key06 = key_t2[i+3+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                    key07 = key_t2[i+3+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                    key08 = key_t2[i+3+7*C3];
                }
                //######## LAST ITERATION AND PREPARE DATA FOR SECOND HALF

                asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+hs-1), "r"(stridesize_q));
                query_t = inp + b * T * C3 + (t+numElem1) * C3 + h * hs; // update query

                asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                key01 = key_t2[0*C3];
                asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                key02 = key_t2[1*C3];                    
                asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                key03 = key_t2[2*C3];
                asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                key04 = key_t2[3*C3];
                asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                key05 = key_t2[4*C3];
                asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                key06 = key_t2[5*C3];                    
                asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                key07 = key_t2[6*C3];
                asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                key08 = key_t2[7*C3];                    

                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));// FOR NEXT ITER
                asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                key01 = key_t2[1+0*C3];
                asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                key02 = key_t2[1+1*C3];                    
                asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                key03 = key_t2[1+2*C3];
                asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                key04 = key_t2[1+3*C3];
                asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                key05 = key_t2[1+4*C3];
                asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                key06 = key_t2[1+5*C3];                    
                asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                key07 = key_t2[1+6*C3];
                asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                key08 = key_t2[1+7*C3];

                asm volatile("vfmul.vf  v4  , v4 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v4 , (%0), %1" ::"r"(preatt_bth + t+0), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v6  , v6 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v6 , (%0), %1" ::"r"(preatt_bth + t+1), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v8  , v8 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v8 , (%0), %1" ::"r"(preatt_bth + t+2), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v10 , v10, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v10, (%0), %1" ::"r"(preatt_bth + t+3), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v12 , v12, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(preatt_bth + t+4), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v14 , v14, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v14, (%0), %1" ::"r"(preatt_bth + t+5), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+6), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v18 , v18, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v18, (%0), %1" ::"r"(preatt_bth + t+7), "r"(stridesize_patt));

                //######## SECOND HALF
                // query_t = inp + b * T * C3 + (t+numElem1) * C3 + h * hs;
                preatt_bth = preatt + b*NH*T*T + h*T*T + (t+numElem1)*T;
                asm volatile("vsetvli  %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(numElem2));//SET THE LMUL TO 4 AS T is 64

                asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));
                // key01 = key_t2[0*C3];
                // key02 = key_t2[1*C3];
                // key03 = key_t2[2*C3];
                // key04 = key_t2[3*C3];
                // key05 = key_t2[4*C3];
                // key06 = key_t2[5*C3];
                // key07 = key_t2[6*C3];
                // key08 = key_t2[7*C3];

                // key11 = key_t2[1+0*C3];
                // key12 = key_t2[1+1*C3];
                // key13 = key_t2[1+2*C3];
                // key14 = key_t2[1+3*C3];
                // key15 = key_t2[1+4*C3];
                // key16 = key_t2[1+5*C3];
                // key17 = key_t2[1+6*C3];
                // key18 = key_t2[1+7*C3];

                for (int i = 0; i < hs-2; i+=2) {

                    asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+i+1), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                    key01 = key_t2[i+2+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                    key02 = key_t2[i+2+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                    key03 = key_t2[i+2+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                    key04 = key_t2[i+2+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                    key05 = key_t2[i+2+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                    key06 = key_t2[i+2+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                    key07 = key_t2[i+2+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                    key08 = key_t2[i+2+7*C3];                    

                    asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t+i+2), "r"(stridesize_q));
                    asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                    key01 = key_t2[i+3+0*C3];
                    asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                    key02 = key_t2[i+3+1*C3];                    
                    asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                    key03 = key_t2[i+3+2*C3];
                    asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                    key04 = key_t2[i+3+3*C3];
                    asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                    key05 = key_t2[i+3+4*C3];
                    asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                    key06 = key_t2[i+3+5*C3];                    
                    asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                    key07 = key_t2[i+3+6*C3];
                    asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                    key08 = key_t2[i+3+7*C3];
                }

                //######## LAST ITERATION AND PREPARE DATA FOR NEXT FIRST HALF
                //----------------------------------------------------------------------------------------------------

                asm volatile("vlse32.v v2, (%0), %1" :: "r"(query_t+hs-1), "r"(stridesize_q));
                query_t = inp + b * T * C3 + (t+numElem1) * C3 + h * hs; // update query

                asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(key01));
                key01 = key_t2[0*C3];
                asm volatile("vfmacc.vf  v6 , %[A], v0" ::[A] "f"(key02));
                key02 = key_t2[1*C3];                    
                asm volatile("vfmacc.vf  v8 , %[A], v0" ::[A] "f"(key03));
                key03 = key_t2[2*C3];
                asm volatile("vfmacc.vf  v10, %[A], v0" ::[A] "f"(key04));
                key04 = key_t2[3*C3];
                asm volatile("vfmacc.vf  v12, %[A], v0" ::[A] "f"(key05));
                key05 = key_t2[4*C3];
                asm volatile("vfmacc.vf  v14, %[A], v0" ::[A] "f"(key06));
                key06 = key_t2[5*C3];                    
                asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(key07));
                key07 = key_t2[6*C3];
                asm volatile("vfmacc.vf  v18, %[A], v0" ::[A] "f"(key08));
                key08 = key_t2[7*C3];                    

                // asm volatile("vlse32.v v0, (%0), %1" :: "r"(query_t), "r"(stridesize_q));// FOR NEXT ITER
                asm volatile("vfmacc.vf  v4 , %[A], v2" ::[A] "f"(key11));
                key01 = key_t2[1+0*C3];
                asm volatile("vfmacc.vf  v6 , %[A], v2" ::[A] "f"(key12));
                key02 = key_t2[1+1*C3];                    
                asm volatile("vfmacc.vf  v8 , %[A], v2" ::[A] "f"(key13));
                key03 = key_t2[1+2*C3];
                asm volatile("vfmacc.vf  v10, %[A], v2" ::[A] "f"(key14));
                key04 = key_t2[1+3*C3];
                asm volatile("vfmacc.vf  v12, %[A], v2" ::[A] "f"(key15));
                key05 = key_t2[1+4*C3];
                asm volatile("vfmacc.vf  v14, %[A], v2" ::[A] "f"(key16));
                key06 = key_t2[1+5*C3];                    
                asm volatile("vfmacc.vf  v16, %[A], v2" ::[A] "f"(key17));
                key07 = key_t2[1+6*C3];
                asm volatile("vfmacc.vf  v18, %[A], v2" ::[A] "f"(key18));
                key08 = key_t2[1+7*C3];
                //----------------------------------------------------------------------------------------------------

                asm volatile("vfmul.vf  v4  , v4 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v4 , (%0), %1" ::"r"(preatt_bth + t+0), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v6  , v6 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v6 , (%0), %1" ::"r"(preatt_bth + t+1), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v8  , v8 , %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v8 , (%0), %1" ::"r"(preatt_bth + t+2), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v10 , v10, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v10, (%0), %1" ::"r"(preatt_bth + t+3), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v12 , v12, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(preatt_bth + t+4), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v14 , v14, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v14, (%0), %1" ::"r"(preatt_bth + t+5), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth + t+6), "r"(stridesize_patt));
                
                asm volatile("vfmul.vf  v18 , v18, %[A]" ::[A] "f"(scale));
                asm volatile("vsse32.v v18, (%0), %1" ::"r"(preatt_bth + t+7), "r"(stridesize_patt));
            }
        }
    }
}


void attention_forward_rev(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
                       float* mask,
                       float* max_arr,
                       float* temp2,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART
    // float scale = 1.0 / hs;
    
    int avl;
    int stride = B*T * sizeof(float);
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vle32.v v4  , (%0)" :: "r"(mask));
    // asm volatile("vfmul.vf  v4 , v4, %[A]" ::[A] "f"(scale));


    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {
            for (int i = 0; i < T; i++){
                float* src = inp + b * T * C3 + h * hs + i * C3;
                float* dst = temp + i;
                avl = hs;

                // while (avl > 0){
                    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vle32.v v0  , (%0)" :: "r"(src));
                    asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride));
                    // src += vl;
                    // dst += vl*T;
                    // avl -= vl;
                // }
            }


            for (int t = 0; t < T; t+=4) {// rows (columns) of Q*K output (TxT)
                float* key_t2  = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key

                // float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T;
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T + t;
                float* query_t = temp + t;


                float key0 = key_t2[0*C3];
                float key1 = key_t2[1*C3];
                float key2 = key_t2[2*C3];
                float key3 = key_t2[3*C3];
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));

                for (int i = 0; i < hs; i++) { // rows of Q and elements of K
                    query_t += T;
                    // avl = T-t;
                    // while (avl > 0){
                    //     asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                    //     asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));
                    //     float key0 = key_t2[i];
                    //     asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                    //     asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(key0));
                    //     float key1 = key_t2[i+1];
                    //     asm volatile("vslidedown.vx v8 , v0, %[A]" ::[A] "r"(2));
                    //     asm volatile("vfmacc.vf  v20 , %[A], v4 " ::[A] "f"(key1));
                    //     float key2 = key_t2[i+2];
                    //     asm volatile("vslidedown.vx v12, v0, %[A]" ::[A] "r"(3));
                    //     asm volatile("vfmacc.vf  v24 , %[A], v8 " ::[A] "f"(key2));
                    //     float key3 = key_t2[i+3];
                    //     asm volatile("vfmacc.vf  v28 , %[A], v12" ::[A] "f"(key3));
                    //     query_t += vl;
                    //     avl -= vl;
                    // }
                    // do{
                        // asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                        // if(t == 0 || t == 4){
                        //     printf("key0[%d]=%f\tkey1[%d]=%f\tkey2[%d]=%f\tkey3[%d]=%f\n",i,key0,i,key1,i,key2,i,key3);
                        // }
                        if(i == 0){
                            asm volatile("vfmul.vf  v16, v0, %0" ::"f"(key0));
                            key0 = key_t2[i+0*C3+1];
                            asm volatile("vfmul.vf  v20, v0, %0" ::"f"(key1));
                            key1 = key_t2[i+1*C3+1];
                            asm volatile("vfmul.vf  v24, v0, %0" ::"f"(key2));
                            key2 = key_t2[i+2*C3+1];
                            asm volatile("vfmul.vf  v28, v0, %0" ::"f"(key3));
                            asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));
                            key3 = key_t2[i+3*C3+1];
                        }else{
                            asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(key0));
                            key0 = key_t2[i+0*C3+1];
                            asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(key1));
                            key1 = key_t2[i+1*C3+1];
                            asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(key2));
                            key2 = key_t2[i+2*C3+1];
                            asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(key3));
                            asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));
                            key3 = key_t2[i+3*C3+1];
                        }

                    //     query_t += vl;
                    //     avl -= vl;
                    // }while (avl > 0);
                }
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v20 , v20, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v28 , v28, %[A]" ::[A] "f"(scale));

                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth+0), "r"(stridesize_patt));
                // asm volatile("vsse32.v v20, (%0), %1" ::"r"(preatt_bth+1), "r"(stridesize_patt));
                // asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth+2), "r"(stridesize_patt));
                // asm volatile("vsse32.v v28, (%0), %1" ::"r"(preatt_bth+3), "r"(stridesize_patt));
                asm volatile("vse32.v v16, (%0)" :: "r"(preatt_bth+0*T));
                asm volatile("vse32.v v20, (%0)" :: "r"(preatt_bth+1*T));
                asm volatile("vse32.v v24, (%0)" :: "r"(preatt_bth+2*T));
                asm volatile("vse32.v v28, (%0)" :: "r"(preatt_bth+3*T));
            }// LOOP OVER T

            // MAXVAL & EXP
            avl = T;
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            float* preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle32.v v0  , (%0)" :: "r"(preatt_bth));
            for (int t = 0; t < T; t++) {
                preatt_bth += T;
                asm volatile("vle32.v v4  , (%0)" :: "r"(preatt_bth));
                asm volatile("vfmax.vv v0, v0, v4");
            }

            // asm volatile("vse32.v v16, (%0)" :: "r"(temp2));



            // asm volatile("vse32.v v0, (%0)" :: "r"(max_arr));
            // for (int i = 0; i < T; i++){
            //     printf("max[%d] = %f \n",i,max_arr[i]);
            // }
            
            // avl = T;
            // asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            // float* preatt_bth = preatt + b*NH*T*T + h*T*T;
            // asm volatile("vle32.v v0  , (%0)" :: "r"(preatt_bth));
            // preatt_bth += T;
            // asm volatile("vle32.v v4  , (%0)" :: "r"(preatt_bth));
            // preatt_bth += T;
            // for (int t = 0; t < T; t+=2) {
            //     asm volatile("vfmax.vv v0, v0, v4");
            //     asm volatile("vle32.v v8  , (%0)" :: "r"(preatt_bth));
            //     preatt_bth += T;
            //     asm volatile("vfmax.vv v0, v0, v8");
            //     asm volatile("vle32.v v4  , (%0)" :: "r"(preatt_bth));
            //     preatt_bth += T;
            // }
            // asm volatile("vse32.v v0, (%0)" :: "r"(max_arr));


            float* att_bth    = att    + b*NH*T*T + h*T*T;
            preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle32.v v8  , (%0)" :: "r"(preatt_bth));
            // asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
            // for (int i = 0; i < T; i++){
            //     printf("temp[%d] = %f \n",i,temp[i]);
            // }
            preatt_bth += T+1;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                asm volatile("vle32.v v12  , (%0)" :: "r"(preatt_bth));
                // preatt_bth += T+t+2;
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v8 , v8, v0");
                
                // if(t == 0){
                //     asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
                //     for (int i = 0; i < 3; i++){
                //         printf("temp2[%d] = %f \n",i,temp2[i]);
                //     }
                // }
                //------------ FAST_EXP
                asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(12102203.0f));
                // snrt_cluster_hw_barrier();
                asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(1064866805.0f));
                // snrt_cluster_hw_barrier();
                asm volatile("vfcvt.rtz.xu.f.v v8, v8");
                // asm volatile("vfcvt.xu.f.v v8, v8");
                // snrt_cluster_hw_barrier();

                //------------
                
                // if(t == 0){
                //     asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
                //     for (int i = 0; i < 10; i++){
                //         printf("semi_res[%d] = %f \n",i,temp2[i]);
                //     }
                // }

                // asm volatile("vsse32.v v8, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse32.v v8, (%0)" :: "r"(att_bth));
                // att_bth += T+t+1; 
                att_bth += T+1; 
                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v0 , v4, %[A]" ::[A] "r"(0));
                asm volatile("vle32.v v8  , (%0)" :: "r"(preatt_bth));
                // preatt_bth += T+t+3;
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v12 , v12, v4");
                //------------ FAST_EXP
                asm volatile("vfmul.vf v12, v12, %[A]" ::[A] "f"(12102203.0f));
                asm volatile("vfadd.vf v12, v12, %[A]" ::[A] "f"(1064866805.0f));
                asm volatile("vfcvt.rtz.xu.f.v v12, v12");
                //------------
                // asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse32.v v12, (%0)" :: "r"(att_bth));
                // att_bth += T+t+2; 
                att_bth += T+1; // I think it is stored transposed !!!!!!!
            }

            
            // //------------ FAST_EXP
            // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(size));
            // asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
            // asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
            // asm volatile("vfcvt.rtz.xu.f.v v0, v0");
            // //------------

            // pass 3: normalize to get the softmax
            att_bth    = att    + b*NH*T*T + h*T*T;
            avl = T;//CHECK BETWEEN T OR HS
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(1.0f));
            asm volatile("vle32.v v0  , (%0)" :: "r"(att_bth));
            // asm volatile("vsse32.v v0, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
            // att_bth_correct_form++;
            att_bth += T;
            for (int t = 1; t < T; t++) {
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                // asm volatile("vsse32.v v4, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                // att_bth_correct_form++;
                att_bth += T;
                asm volatile("vfadd.vv v0, v0, v4");
            }
            // snrt_cluster_hw_barrier();
            asm volatile("vfdiv.vv v0,v8,v0");// 1/expsum
            // snrt_cluster_hw_barrier();


            // asm volatile("vse32.v v0, (%0)" :: "r"(temp2));
            // for (int i = 0; i < T; i++){
            //     printf("expsum_inv[%d] = %f \n",i,temp2[i]);
            // }
            
            // float * att_bth_correct_form = temp;
            att_bth    = att    + b*NH*T*T + h*T*T;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v0, v4");
                asm volatile("vslide1down.vx v8 , v0, %[A]" ::[A] "r"(0));
                asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                // asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                // att_bth_correct_form++;
                att_bth += T+1;

                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v8, v4");
                asm volatile("vslide1down.vx v0 , v8, %[A]" ::[A] "r"(0));
                asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                // asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                // att_bth_correct_form++;
                att_bth += T+1;

            }

            // pass 4: accumulate weighted values into the output of attention
            for (int t = 0; t < T; t++) {//
                // itr = 0;
                // num_elements = hs;
                // float* out_bth = out + b * T * C + t * C + h * hs;

                // for (int t2 = 0; t2 <= t; t2++) {
                //     float* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                //     float att_btht2 = att_bth[t2];

                //     avl = hs;
                //     asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                //     asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                //     if(t2 == t){
                //         asm volatile("vfmul.vf   v4 ,v0 ,%[A]" ::[A] "f"(att_btht2));
                //         asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth));
                //     }else{
                //         asm volatile("vle32.v    v4 , (%0)" :: "r"(out_bth));
                //         asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(att_btht2));
                //         asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth));
                //     }
                // }

                // avl = hs;
                // asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // att_bth    = att    + b*NH*T*T + h*T*T;
                // float* value_t2 = inp + b * T * C3 + t * C3 + h * hs + C*2; // +C*2 because it's value
                // float* out_bth = out + b * T * C + t * C + h * hs;
                // asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                // // printf("t = %d\n",t);
                // for (int t2 = t; t2 < T; t2++){
                //     if(t == 0){
                //         float att_btht2 = att_bth[T*t2];
                //         asm volatile("vfmul.vf   v4 ,v0 ,%[A]" ::[A] "f"(att_btht2));
                //         asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth+(t-t2)*C));
                //     }else{
                //         float att_btht2 = att_bth[T*t2];
                //         asm volatile("vle32.v    v4 , (%0)" :: "r"(out_bth+(t-t2)*C));
                //         asm volatile("vfmacc.vf  v4 , %[A], v0" ::[A] "f"(att_btht2));
                //         asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth+(t-t2)*C));
                //     }
                // }

                avl = hs;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                att_bth    = att    + b*NH*T*T + h*T*T + t;
                // float* value_t2 = inp + b * T * C3 + t * C3 + h * hs + C*2; // +C*2 because it's value
                float* value_t2 = inp + b * T * C3 + h * hs + C*2; // +C*2 because it's value
                float* out_bth = out + b * T * C + t * C + h * hs;
                asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                float att_btht2 = att_bth[0];
                value_t2 += C3;

                for (int t2 = 0; t2 <= t; t2++){
                        if(t2 == 0){
                            asm volatile("vfmul.vf   v4 ,v0 ,%[A]" ::[A] "f"(att_btht2));                            
                        }else{
                            asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(att_btht2));
                        }
                        att_btht2 = att_bth[T*(t2+1)];
                        asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                        value_t2 += C3;
                        // asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth+(t-t2)*C));
                        // float att_btht2 = att_bth[T*t2];

                }
                asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth));


            // for (int i = 0; i < T; i++){
            //     float* src = inp + b * T * C3 + h * hs + i * C3;
            //     float* dst = temp + i;
            //     avl = hs;

            //     // while (avl > 0){
            //         asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            //         asm volatile("vle32.v v0  , (%0)" :: "r"(src));
            //         asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride));
            //         // src += vl;
            //         // dst += vl*T;
            //         // avl -= vl;
            //     // }
            // }

            }
        }
    }
}

void attention_forward_rev2(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
                       float* mask,
                       float* max_arr,
                       float* temp2,
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
    float scale = 1.0 / sqrtf(hs);//I CHANGE THIS PART

    int avl;
    int stride = B*T * sizeof(float);
    int stridesize_patt = T*4;//C3 * 4 byte (for column-wise access)

    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vle32.v v4  , (%0)" :: "r"(mask));


    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {
            for (int i = 0; i < T; i++){
                float* src = inp + b * T * C3 + h * hs + i * C3;
                float* dst = temp + i;
                avl = hs;

                // while (avl > 0){
                    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vle32.v v0  , (%0)" :: "r"(src));
                    asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride));
                    // src += vl;
                    // dst += vl*T;
                    // avl -= vl;
                // }
            }


            for (int t = 0; t < T; t+=4) {// rows (columns) of Q*K output (TxT)
                float* key_t2  = inp + b * T * C3 + t * C3 + h * hs + C; // +C because it's key
                float* preatt_bth = preatt + b*NH*T*T + h*T*T + t*T + t;
                float* query_t = temp + t;


                float key0 = key_t2[0*C3];
                float key1 = key_t2[1*C3];
                float key2 = key_t2[2*C3];
                float key3 = key_t2[3*C3];
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));

                for (int i = 0; i < hs; i++) { // rows of Q and elements of K
                    query_t += T;
                    // do{
                        if(i == 0){
                            asm volatile("vfmul.vf  v16, v0, %0" ::"f"(key0));
                            key0 = key_t2[i+0*C3+1];
                            asm volatile("vfmul.vf  v20, v0, %0" ::"f"(key1));
                            key1 = key_t2[i+1*C3+1];
                            asm volatile("vfmul.vf  v24, v0, %0" ::"f"(key2));
                            key2 = key_t2[i+2*C3+1];
                            asm volatile("vfmul.vf  v28, v0, %0" ::"f"(key3));
                            asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));
                            key3 = key_t2[i+3*C3+1];
                        }else{
                            asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(key0));
                            key0 = key_t2[i+0*C3+1];
                            asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(key1));
                            key1 = key_t2[i+1*C3+1];
                            asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(key2));
                            key2 = key_t2[i+2*C3+1];
                            asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(key3));
                            asm volatile("vle32.v v0  , (%0)" :: "r"(query_t));
                            key3 = key_t2[i+3*C3+1];
                        }

                    //     query_t += vl;
                    //     avl -= vl;
                    // }while (avl > 0);
                }
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vfmul.vf  v16 , v16, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v20 , v20, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v24 , v24, %[A]" ::[A] "f"(scale));
                asm volatile("vfmul.vf  v28 , v28, %[A]" ::[A] "f"(scale));

                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // asm volatile("vsse32.v v16, (%0), %1" ::"r"(preatt_bth+0), "r"(stridesize_patt));
                // asm volatile("vsse32.v v20, (%0), %1" ::"r"(preatt_bth+1), "r"(stridesize_patt));
                // asm volatile("vsse32.v v24, (%0), %1" ::"r"(preatt_bth+2), "r"(stridesize_patt));
                // asm volatile("vsse32.v v28, (%0), %1" ::"r"(preatt_bth+3), "r"(stridesize_patt));
                asm volatile("vse32.v v16, (%0)" :: "r"(preatt_bth+0*T));
                asm volatile("vse32.v v20, (%0)" :: "r"(preatt_bth+1*T));
                asm volatile("vse32.v v24, (%0)" :: "r"(preatt_bth+2*T));
                asm volatile("vse32.v v28, (%0)" :: "r"(preatt_bth+3*T));
            }// LOOP OVER T

            avl = T;
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vfsub.vv  v0 , v0, v0");
            for (int t = 0; t < T; t++) {
                asm volatile("vse32.v v0, (%0)" :: "r"(temp+t*T));
            }

            // MAXVAL & EXP
            avl = T;
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            float* preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle32.v v0  , (%0)" :: "r"(preatt_bth));
            for (int t = 0; t < T; t++) {
                preatt_bth += T;
                asm volatile("vle32.v v4  , (%0)" :: "r"(preatt_bth));
                asm volatile("vfmax.vv v0, v0, v4");
            }

            // float* att_bth    = att    + b*NH*T*T + h*T*T;
            float* att_bth    = temp;
            preatt_bth = preatt + b*NH*T*T + h*T*T;
            asm volatile("vle32.v v8  , (%0)" :: "r"(preatt_bth));
            preatt_bth += T+1;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v4 , v0, %[A]" ::[A] "r"(0));
                asm volatile("vle32.v v12  , (%0)" :: "r"(preatt_bth));
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v8 , v8, v0");
                
                //------------ FAST_EXP
                asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(12102203.0f));
                asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(1064866805.0f));
                asm volatile("vfcvt.rtz.xu.f.v v8, v8");
                //------------


                // asm volatile("vsse32.v v8, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse32.v v8, (%0)" :: "r"(att_bth));
                att_bth += T+1; 
                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vslide1down.vx v0 , v4, %[A]" ::[A] "r"(0));
                asm volatile("vle32.v v8  , (%0)" :: "r"(preatt_bth));
                preatt_bth += T+1;
                asm volatile("vfsub.vv    v12 , v12, v4");
                //------------ FAST_EXP
                asm volatile("vfmul.vf v12, v12, %[A]" ::[A] "f"(12102203.0f));
                asm volatile("vfadd.vf v12, v12, %[A]" ::[A] "f"(1064866805.0f));
                asm volatile("vfcvt.rtz.xu.f.v v12, v12");
                //------------
                // asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth), "r"(stridesize_patt));
                asm volatile("vse32.v v12, (%0)" :: "r"(att_bth));
                att_bth += T+1; // I think it is stored transposed !!!!!!!
            }

            // pass 3: normalize to get the softmax
            // att_bth    = att    + b*NH*T*T + h*T*T;
            att_bth    = temp;
            avl = T;//CHECK BETWEEN T OR HS
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vfmv.v.f v8, %[A]" ::[A] "f"(1.0f));
            asm volatile("vle32.v v0  , (%0)" :: "r"(att_bth));
            att_bth += T;
            for (int t = 1; t < T; t++) {
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                att_bth += T;
                asm volatile("vfadd.vv v0, v0, v4");
            }

            asm volatile("vfdiv.vv v0,v8,v0");// 1/expsum

            
            float * att_bth_correct_form = att    + b*NH*T*T + h*T*T;
            att_bth    = temp;
            // att_bth    = att    + b*NH*T*T + h*T*T;
            for (int t = 0; t < T; t+=2) {
                avl = T-t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v0, v4");
                asm volatile("vslide1down.vx v8 , v0, %[A]" ::[A] "r"(0));
                // asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                att_bth_correct_form += T+1;
                att_bth += T+1;

                avl = T-t-1;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v4  , (%0)" :: "r"(att_bth));
                asm volatile("vfmul.vv  v12, v8, v4");
                asm volatile("vslide1down.vx v0 , v8, %[A]" ::[A] "r"(0));
                // asm volatile("vse32.v v12  , (%0)" :: "r"(att_bth));
                asm volatile("vsse32.v v12, (%0), %1" ::"r"(att_bth_correct_form), "r"(stridesize_patt));
                att_bth_correct_form += T+1;
                att_bth += T+1;

            }

            // pass 4: accumulate weighted values into the output of attention
            for (int t = 0; t < T; t++) {//
                avl = hs;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // att_bth    = att    + b*NH*T*T + h*T*T + t;
                att_bth    = att    + b*NH*T*T + h*T*T + t*T;
                float* value_t2 = inp + b * T * C3 + h * hs + C*2; // +C*2 because it's value
                float* out_bth = out + b * T * C + t * C + h * hs;
                asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                float att_btht2 = att_bth[0];
                value_t2 += C3;

                for (int t2 = 0; t2 <= t; t2++){
                    if(t2 == 0){
                        asm volatile("vfmul.vf   v4 ,v0 ,%[A]" ::[A] "f"(att_btht2));                            
                    }else{
                        asm volatile("vfmacc.vf  v4, %0, v0" ::"f"(att_btht2));
                    }
                    // att_btht2 = att_bth[T*(t2+1)];
                    att_btht2 = att_bth[t2+1];
                    asm volatile("vle32.v    v0 , (%0)" :: "r"(value_t2));
                    value_t2 += C3;
                }
                asm volatile("vse32.v    v4 , (%0)" :: "r"(out_bth));
            }
        }
    }
}



void attention_backward(float* dinp, float* dpreatt, float* datt,
                        float* dout, float* inp, float* att,
                        int B, int T, int C, int NH,
                        unsigned int vl) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.f / sqrtf(hs);//I CHANGE THIS PART (I DID NOT CHECK THIS)
    // float scale = 1.f / hs;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            for (int h = 0; h < NH; h++) {
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;
                float* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;

                // backward pass 4, through the value accumulation
                float* dout_bth = dout + b * T * C + t * C + h * hs;
                for (int t2 = 0; t2 <= t; t2++) {
                    float* value_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C*2; // +C*2 because it's value
                    float* dvalue_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C*2;
                    //dout_bth  V0 - V7
                    //value_t2  V8 - V15
                    //dvalue_t2 V16- V24
                    //mul       V8 - V15
                    //datt_bth  V24
                    //att_bth   V8 - V15
                    int itr = 0;
                    int num_elements = hs;
                    float temp = 0;

                    asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                    asm volatile("vfsub.vv v31,v31,v31");
                    // asm volatile("vle32.v v31  , (%0)" :: "r"(datt_bth[t2]));//THERE IS SOME ISSUE WITH THIS
                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(dout_bth  +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(value_t2  +itr*VLMAX));
                        asm volatile("vle32.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v24 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v24" : "=f"(temp));
                        // asm volatile("vfmv.s.f    v31, %0"  : "=f"(temp));//BUG
                        asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(att_bth[t2]));
                        asm volatile("vse32.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(dout_bth  +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(value_t2  +itr*VLMAX));
                        asm volatile("vle32.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                        asm volatile("vfmul.vv    v8, v0, v8");
                        asm volatile("vfredsum.vs v24 , v8  , v31");
                        asm volatile("vfmv.f.s    %0 , v24" : "=f"(temp));
                        asm volatile("vfmacc.vf   v16, %[A], v0" ::[A] "f"(att_bth[t2]));
                        asm volatile("vse32.v     v16, (%0)" :: "r"(dvalue_t2 +itr*VLMAX));
                    }
                    datt_bth[t2] = temp + datt_bth[t2];
                }

                // backward pass 2 & 3, the softmax
                // note that softmax (like e.g. tanh) doesn't need the input (preatt) to backward
                for (int t2 = 0; t2 <= t; t2++) {
                    dpreatt_bth[t2] += datt_bth[t2] * att_bth[t2];
                    int itr = 0;
                    int num_elements = t+1;
                    float temp = 0;
                    //att_bth      V0 - V7
                    //dpreatt_bth  V8 - V15
                    //loc          V16- V24
                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth      +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dpreatt_bth  +itr*VLMAX));
                        asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( att_bth[t2]));
                        asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(datt_bth[t2]));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dpreatt_bth +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth      +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dpreatt_bth  +itr*VLMAX));
                        asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( att_bth[t2]));
                        asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(datt_bth[t2]));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dpreatt_bth +itr*VLMAX));
                    }
                }

                // backward pass 1, the query @ key matmul
                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    int itr = 0;
                    int num_elements = hs;
                    float temp = dpreatt_bth[t2] * scale;
                    //key_t2      V0 - V7
                    //dquery_t    V8 - V15
                    //query_t     V0 - V7
                    //dkey_t2     V8 - V15
                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while(itr*VLMAX + VLMAX <= num_elements){
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));

                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        itr++;
                    }
                    if(itr*VLMAX < num_elements){
                        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t +itr*VLMAX));

                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  +itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  +itr*VLMAX));
                    }
                }
            }
        }
    }
}

void attention_backward_rev2(float* dinp, float* dpreatt, float* datt,
                              float* dout, float* inp, float* att,
                              float* temp, float* dscratch, float* mask,
                              int B, int T, int C, int NH,
                              unsigned int vl) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.f / sqrtf(hs);

    int avl;
    int stride_T = T * sizeof(float);        // transpose stride for dout -> temp
    int stridesize_patt = T * 4;              // transpose-back stride for dscratch -> datt

    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vle32.v v4  , (%0)" :: "r"(mask));

    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {

            // ---------------------------------------------------------------
            // Step 1: transpose dout (T,hs) -> temp (hs,T), exactly like the
            // Q transpose in attention_forward_rev2, just with dout as source.
            // ---------------------------------------------------------------
            for (int i = 0; i < T; i++) {
                float* src = dout + b * T * C + h * hs + i * C;
                float* dst = temp + i;
                avl = hs;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(src));
                asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride_T));
            }

            // ---------------------------------------------------------------
            // Step 2: datt[t][t2] = dout[t] . value[t2] for t2 <= t, computed
            // 4 value-rows (t2) at a time, vectorized across the query/time
            // axis (t) instead of the reduction axis (hs) -- same trick as
            // the Q.K computation in attention_forward_rev2. No `scale` here.
            // Result is written into dscratch in TRANSPOSED (value-major)
            // layout: dscratch[t2*T + t].
            // ---------------------------------------------------------------
            for (int t2b = 0; t2b < T; t2b += 4) {
                float* value_t2  = inp + b * T * C3 + t2b * C3 + h * hs + C*2; // +C*2 -> value
                float* doutT_col = temp + t2b;
                float* draw_bth  = dscratch + t2b * T + t2b;

                float v0v = value_t2[0*C3];
                float v1v = value_t2[1*C3];
                float v2v = value_t2[2*C3];
                float v3v = value_t2[3*C3];
                avl = T - t2b;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(doutT_col));

                for (int i = 0; i < hs; i++) {
                    doutT_col += T;
                    if (i == 0) {
                        asm volatile("vfmul.vf  v16, v0, %0" ::"f"(v0v));
                        v0v = value_t2[i+0*C3+1];
                        asm volatile("vfmul.vf  v20, v0, %0" ::"f"(v1v));
                        v1v = value_t2[i+1*C3+1];
                        asm volatile("vfmul.vf  v24, v0, %0" ::"f"(v2v));
                        v2v = value_t2[i+2*C3+1];
                        asm volatile("vfmul.vf  v28, v0, %0" ::"f"(v3v));
                        asm volatile("vle32.v v0  , (%0)" :: "r"(doutT_col));
                        v3v = value_t2[i+3*C3+1];
                    } else {
                        asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(v0v));
                        v0v = value_t2[i+0*C3+1];
                        asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(v1v));
                        v1v = value_t2[i+1*C3+1];
                        asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(v2v));
                        v2v = value_t2[i+2*C3+1];
                        asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(v3v));
                        asm volatile("vle32.v v0  , (%0)" :: "r"(doutT_col));
                        v3v = value_t2[i+3*C3+1];
                    }
                }

                // causal-edge mask for rows t2b+1, t2b+2, t2b+3 (t2b+0 needs none,
                // since its valid range starts exactly at query = t2b)
                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vse32.v v16, (%0)" :: "r"(draw_bth+0*T));
                asm volatile("vse32.v v20, (%0)" :: "r"(draw_bth+1*T));
                asm volatile("vse32.v v24, (%0)" :: "r"(draw_bth+2*T));
                asm volatile("vse32.v v28, (%0)" :: "r"(draw_bth+3*T));
            }

            // ---------------------------------------------------------------
            // Step 3: transpose dscratch (value-major) back into the real,
            // query-major datt array, accumulating (datt is +=, like naive).
            // ---------------------------------------------------------------
            for (int t2 = 0; t2 < T; t2++) {
                float* draw_row = dscratch + t2 * T + t2;                          // contiguous
                float* datt_col = datt + b*NH*T*T + h*T*T + t2*T + t2;             // strided (col t2)
                avl = T - t2;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v  v0, (%0)"       :: "r"(draw_row));
                asm volatile("vlse32.v v8, (%0), %1"   :: "r"(datt_col), "r"(stridesize_patt));
                asm volatile("vfadd.vv v0, v0, v8");
                asm volatile("vsse32.v v0, (%0), %1"   :: "r"(datt_col), "r"(stridesize_patt));
            }

            // ---------------------------------------------------------------
            // Step 4: dvalue[t2] += sum_{t>=t2} att[t][t2] * dout[t].
            // Naturally vectorized over hs already (no reduction needed) --
            // same shape as attention_forward_rev2's pass 4 (weighted value
            // accumulation), just with the roles of t/t2 transposed. The
            // accumulator is kept resident in a vector register across the
            // whole inner t loop instead of reloading/storing every
            // iteration, since t2 (and hence the target address) is fixed
            // for the whole inner loop here.
            // ---------------------------------------------------------------
            for (int t2 = 0; t2 < T; t2++) {
                float* dvalue_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C*2;
                int itr = 0;
                int num_elements = hs;
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while (itr*VLMAX + VLMAX <= num_elements) {
                    asm volatile("vle32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
                    for (int t = t2; t < T; t++) {
                        float* dout_bth = dout + b*T*C + t*C + h*hs + itr*VLMAX;
                        float  w = att[b*NH*T*T + h*T*T + t*T + t2];
                        asm volatile("vle32.v    v0 , (%0)" :: "r"(dout_bth));
                        asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(w));
                    }
                    asm volatile("vse32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
                    itr++;
                }
                if (itr*VLMAX < num_elements) {
                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
                    for (int t = t2; t < T; t++) {
                        float* dout_bth = dout + b*T*C + t*C + h*hs + itr*VLMAX;
                        float  w = att[b*NH*T*T + h*T*T + t*T + t2];
                        asm volatile("vle32.v    v0 , (%0)" :: "r"(dout_bth));
                        asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(w));
                    }
                    asm volatile("vse32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
                }
            }

            // ---------------------------------------------------------------
            // Step 5: softmax backward, closed-form / single-reduction per
            // row instead of naive's O(t)-outer-loop-of-O(t)-vector-passes:
            //     S           = sum_{i<=t} att[i] * datt[i]      (ONE reduce)
            //     dpreatt[i] += att[i] * (datt[i] - S)           for i<=t
            // This replaces naive's O(T^3) total vector work with O(T^2).
            // ---------------------------------------------------------------
            for (int t = 0; t < T; t++) {
                float* att_bth     = att     + b*NH*T*T + h*T*T + t*T;
                float* datt_bth    = datt    + b*NH*T*T + h*T*T + t*T;
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                int num_elements = t + 1;
                int itr = 0;
                float S = 0.f;

                asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfsub.vv v31,v31,v31");
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while (itr*VLMAX + VLMAX <= num_elements) {
                    float partial;
                    asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth  + itr*VLMAX));
                    asm volatile("vle32.v     v8 , (%0)" :: "r"(datt_bth + itr*VLMAX));
                    asm volatile("vfmul.vv    v0 , v0, v8");
                    asm volatile("vfredsum.vs v16, v0, v31");
                    asm volatile("vfmv.f.s    %0 , v16" : "=f"(partial));
                    S += partial;
                    itr++;
                }
                if (itr*VLMAX < num_elements) {
                    float partial;
                    asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth  + itr*VLMAX));
                    asm volatile("vle32.v     v8 , (%0)" :: "r"(datt_bth + itr*VLMAX));
                    asm volatile("vfmul.vv    v0 , v0, v8");
                    asm volatile("vfredsum.vs v16, v0, v31");
                    asm volatile("vfmv.f.s    %0 , v16" : "=f"(partial));
                    S += partial;
                }

                itr = 0;
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                while (itr*VLMAX + VLMAX <= num_elements) {
                    asm volatile("vle32.v    v0 , (%0)" :: "r"(att_bth     + itr*VLMAX));
                    asm volatile("vle32.v    v8 , (%0)" :: "r"(datt_bth    + itr*VLMAX));
                    asm volatile("vle32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
                    asm volatile("vfsub.vf   v8 , v8, %[A]" ::[A] "f"(S));
                    asm volatile("vfmacc.vv  v16, v0, v8");
                    asm volatile("vse32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
                    itr++;
                }
                if (itr*VLMAX < num_elements) {
                    asm volatile("vsetvli    %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                    asm volatile("vle32.v    v0 , (%0)" :: "r"(att_bth     + itr*VLMAX));
                    asm volatile("vle32.v    v8 , (%0)" :: "r"(datt_bth    + itr*VLMAX));
                    asm volatile("vle32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
                    asm volatile("vfsub.vf   v8 , v8, %[A]" ::[A] "f"(S));
                    asm volatile("vfmacc.vv  v16, v0, v8");
                    asm volatile("vse32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
                }
            }

            // ---------------------------------------------------------------
            // Step 6: dquery/dkey from dpreatt -- unchanged from naive, since
            // this part of attention_backward was already vectorized the
            // good way (pure vfmacc.vf over hs, no reduction involved).
            // ---------------------------------------------------------------
            for (int t = 0; t < T; t++) {
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                float* query_t  = inp  + b * T * C3 + t * C3 + h * hs;

                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2  = inp  + b * T * C3 + t2 * C3 + h * hs + C;
                    float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C;
                    float sc = dpreatt_bth[t2] * scale;
                    int itr = 0;
                    int num_elements = hs;

                    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
                    while (itr*VLMAX + VLMAX <= num_elements) {
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   + itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));

                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  + itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
                        itr++;
                    }
                    if (itr*VLMAX < num_elements) {
                        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                        asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   + itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));

                        asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  + itr*VLMAX));
                        asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
                        asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
                        asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
                    }
                }
            }
        }
    }
}


void attention_backward_rev3(float* dinp, float* dpreatt, float* datt,
                              float* dout, float* inp, float* att,
                              float* temp, float* dscratch, float* mask,
                              int B, int T, int C, int NH,
                              unsigned int vl) {
    // inp/dinp are (B, T, 3C) Q,K,V
    // att/datt/dpreatt are (B, NH, T, T)
    // dout is (B, T, C)
    int C3 = C*3;
    int hs = C / NH; // head size
    float scale = 1.f / sqrtf(hs);

    int avl;
    int stride_T = T * sizeof(float);        // transpose stride for dout -> temp
    int stridesize_patt = T * 4;              // transpose-back stride for dscratch -> datt


    for (int b = 0; b < B; b++) {
        for (int h = 0; h < NH; h++) {
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(-1));
            asm volatile("vle32.v v4  , (%0)" :: "r"(mask));

            // ---------------------------------------------------------------
            // Step 1: transpose dout (T,hs) -> temp (hs,T), exactly like the
            // Q transpose in attention_forward_rev2, just with dout as source.
            // ---------------------------------------------------------------
            // temp =  dout + b * T * C + h * hs;
            // dscratch = datt +  b*NH*T*T + h*T*T;
            for (int i = 0; i < T; i++) {
                float* src = dout + b * T * C + h * hs + i * C;
                float* dst = temp + i;
                avl = hs;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(src));
                asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride_T));
            }

            for (int i = 0; i < T; i++) {
                float* src = datt +  b*NH*T*T + h*T*T + i*T;
                float* dst = dscratch + i;
                avl = T;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(src));
                asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(stride_T));
            }

            // ---------------------------------------------------------------
            // Step 2: datt[t][t2] = dout[t] . value[t2] for t2 <= t, computed
            // 4 value-rows (t2) at a time, vectorized across the query/time
            // axis (t) instead of the reduction axis (hs) -- same trick as
            // the Q.K computation in attention_forward_rev2. No `scale` here.
            // Result is written into dscratch in TRANSPOSED (value-major)
            // layout: dscratch[t2*T + t].
            // ---------------------------------------------------------------

            for (int t = 0; t < T; t += 4) {
                float* value_t2  = inp + b * T * C3 + t * C3 + h * hs + C*2; // +C*2 -> value
                float* dout_bth_T = temp + t;
                float* datt_bth = datt + b*NH*T*T + h*T*T + t*T + t;
                float* datt_bth_T = dscratch + t * T + t;


                float v0 = value_t2[0*C3];
                float v1 = value_t2[1*C3];
                float v2 = value_t2[2*C3];
                float v3 = value_t2[3*C3];
                avl = T - t;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v v0  , (%0)" :: "r"(dout_bth_T));

                asm volatile("vle32.v v16 , (%0)" :: "r"(datt_bth_T+0*T));
                asm volatile("vle32.v v20 , (%0)" :: "r"(datt_bth_T+1*T));
                asm volatile("vle32.v v24 , (%0)" :: "r"(datt_bth_T+2*T));
                asm volatile("vle32.v v28 , (%0)" :: "r"(datt_bth_T+3*T));


                for (int i = 0; i < hs; i++) {
                    dout_bth_T += T;
                    asm volatile("vfmacc.vf  v16 , %[A], v0 " ::[A] "f"(v0));
                    v0 = value_t2[i+0*C3+1];
                    asm volatile("vfmacc.vf  v20 , %[A], v0 " ::[A] "f"(v1));
                    v1 = value_t2[i+1*C3+1];
                    asm volatile("vfmacc.vf  v24 , %[A], v0 " ::[A] "f"(v2));
                    v2 = value_t2[i+2*C3+1];
                    asm volatile("vfmacc.vf  v28 , %[A], v0 " ::[A] "f"(v3));
                    asm volatile("vle32.v v0  , (%0)" :: "r"(dout_bth_T));
                    v3 = value_t2[i+3*C3+1];
                }

                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16));
                asm volatile("vfmul.vv  v20 , v20, v4");
                asm volatile("vfmul.vv  v24 , v24, v5");
                asm volatile("vfmul.vv  v28 , v28, v6");

                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                // asm volatile("vse32.v v16, (%0)" :: "r"(datt_bth+0*T));// FOR NOW TRANSPOSED TO WHAT SUPPOSED TO BE AND I DRAW IN THE FIGURES
                // // moshkel ine ke man avale kar balaye ghotr ro sefr mikonam vali mogheye store mikham zire ghotr sefr bashan
                // asm volatile("vse32.v v20, (%0)" :: "r"(datt_bth+1*T));
                // asm volatile("vse32.v v24, (%0)" :: "r"(datt_bth+2*T));
                // asm volatile("vse32.v v28, (%0)" :: "r"(datt_bth+3*T));

                asm volatile("vsse32.v v16, (%0), %1" ::"r"(datt_bth+0), "r"(stridesize_patt));
                asm volatile("vsse32.v v20, (%0), %1" ::"r"(datt_bth+1), "r"(stridesize_patt));
                asm volatile("vsse32.v v24, (%0), %1" ::"r"(datt_bth+2), "r"(stridesize_patt));
                asm volatile("vsse32.v v28, (%0), %1" ::"r"(datt_bth+3), "r"(stridesize_patt));

            }
            // ---------------------------------------------------------------
            // Step 2.5: 
            // ---------------------------------------------------------------
            for (int t = 0; t < T; t += 4) {// ASSUME UPPER DIAGONAL ELEMENTS OF ATT ARE 0
                float* att_bth = att + b*NH*T*T + h*T*T + t*T + t;// move on diagonal elements
                float* dout_bth = dout + b * T * C + t * C + h * hs;
                float* dvalue_t2 = dinp + b * T * C3 + t * C3 + h * hs + C*2;

                avl = hs;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                float a0 = att_bth[0];
                float a1 = att_bth[1];
                float a2 = att_bth[2];
                float a3 = att_bth[3];

                asm volatile("vle32.v v0  , (%0)" :: "r"(dvalue_t2+0*C3));
                asm volatile("vle32.v v4  , (%0)" :: "r"(dvalue_t2+1*C3));
                asm volatile("vle32.v v8  , (%0)" :: "r"(dvalue_t2+2*C3));
                asm volatile("vle32.v v12 , (%0)" :: "r"(dvalue_t2+3*C3));

                asm volatile("vle32.v v16  , (%0)" :: "r"(dout_bth));
                dout_bth += hs;

                for (int i = 0; i < T-t; i+=2) {
                    asm volatile("vfmacc.vf  v0 , %[A], v16 " ::[A] "f"(a0));
                    a0 = att_bth[(i+1)*T+0];
                    asm volatile("vfmacc.vf  v4 , %[A], v16 " ::[A] "f"(a1));
                    a1 = att_bth[(i+1)*T+1];
                    asm volatile("vfmacc.vf  v8 , %[A], v16 " ::[A] "f"(a2));
                    a2 = att_bth[(i+1)*T+2];
                    asm volatile("vfmacc.vf  v12, %[A], v16 " ::[A] "f"(a3));
                    a3 = att_bth[(i+1)*T+3];
                    asm volatile("vle32.v v20  , (%0)" :: "r"(dout_bth));
                    dout_bth += hs;

                    asm volatile("vfmacc.vf  v0 , %[A], v20 " ::[A] "f"(a0));
                    a0 = att_bth[(i+2)*T+0];
                    asm volatile("vfmacc.vf  v4 , %[A], v20 " ::[A] "f"(a1));
                    a1 = att_bth[(i+2)*T+1];
                    asm volatile("vfmacc.vf  v8 , %[A], v20 " ::[A] "f"(a2));
                    a2 = att_bth[(i+2)*T+2];
                    asm volatile("vfmacc.vf  v12, %[A], v20 " ::[A] "f"(a3));
                    a3 = att_bth[(i+2)*T+3];
                    asm volatile("vle32.v v16  , (%0)" :: "r"(dout_bth));
                    dout_bth += hs;
                }

                asm volatile("vse32.v v0 , (%0)" :: "r"(dvalue_t2+0*C3));
                asm volatile("vse32.v v4 , (%0)" :: "r"(dvalue_t2+1*C3));
                asm volatile("vse32.v v8 , (%0)" :: "r"(dvalue_t2+2*C3));
                asm volatile("vse32.v v12, (%0)" :: "r"(dvalue_t2+3*C3));

            }
            // ---------------------------------------------------------------
            // Step 3: backward pass 2 & 3, the softmax
            // note that softmax (like e.g. tanh) doesn't need the input (preatt) to backward
            // ---------------------------------------------------------------

            // for (int t = 0; t < T; t ++) {
            //     float* att_bth = att + b*NH*T*T + h*T*T + t*T;
            //     float* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
            //     float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;

            //     avl = t+1;
            //     asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            //     asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth));                
            //     for (int t2 = 0; t2 <= t; t2++) {
            //         dpreatt_bth[t2] += datt_bth[t2] * att_bth[t2];
                    
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(dpreatt_bth));
            //         asm volatile("vfmul.vf    v16, v0, %[A] " ::[A] "f"( att_bth[t2]));
            //         asm volatile("vfnmsac.vf  v8 , %[A], v16" ::[A] "f"(datt_bth[t2]));
            //         asm volatile("vse32.v     v8 , (%0)" :: "r"(dpreatt_bth));

            //     }
            // }


            for (int t = 0; t < T; t ++) {
                float* att_bth = att + b*NH*T*T + h*T*T + t*T;
                float* datt_bth = datt + b*NH*T*T + h*T*T + t*T;
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;

                avl = t+1;
                asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth));
                asm volatile("vle32.v     v4 , (%0)" :: "r"(datt_bth));
                asm volatile("vle32.v     v8 , (%0)" :: "r"(dpreatt_bth));

                asm volatile("vfmul.vv    v12 , v0 , v4");
                asm volatile("vfadd.vv    v8  , v12, v8");
                asm volatile("vse32.v     v12 , (%0)" :: "r"(temp));


                for (int t2 = 0; t2 <= t; t2++) {
                    asm volatile("vfnmsac.vf  v8 , %[A], v0" ::[A] "f"(temp[t2]));
                }
                asm volatile("vse32.v     v8 , (%0)" :: "r"(dpreatt_bth));
            }




            // for (int t = 0; t < T; t ++) {
            //     int avl = hs;
            //     asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            //     float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
            //     float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
            //     float* query_t = inp + b * T * C3 + t * C3 + h * hs;
            //     for (int t2 = 0; t2 <= t; t2++) {
            //         float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
            //         float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key



            //         float temp = dpreatt_bth[t2] * scale;

            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t));
            //         asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
            //         asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t));

            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2));
            //         asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
            //         asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2));

            //     }
            // }

            for (int t = 0; t < T; t ++) {
                int avl = hs;
                asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
                float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
                float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
                float* query_t = inp + b * T * C3 + t * C3 + h * hs;

                asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t));
                asm volatile("vle32.v     v16, (%0)" :: "r"(query_t));

                for (int t2 = 0; t2 <= t; t2++) {
                    float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
                    float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key



                    float temp = dpreatt_bth[t2] * scale;

                    asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2));
                    // asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t));
                    asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
                    // asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t));

                    asm volatile("vle32.v     v12, (%0)" :: "r"(dkey_t2));
                    asm volatile("vfmacc.vf   v12, %[A], v16" ::[A] "f"(temp));
                    asm volatile("vse32.v     v12, (%0)" :: "r"(dkey_t2));

                }
                    asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t));
            }




            // for (int t = 0; t < T; t ++) {
            //     int avl = hs;
            //     asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            //     float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T + t;
            //     float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
            //     float* query_t = inp + b * T * C3 + t * C3 + h * hs;
            //     for (int t2 = 0; t2 <= t; t2++) {
            //         float* key_t2 = inp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key
            //         float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C; // +C because it's key



            //         float temp = dpreatt_bth[t2] * scale;

            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t));
            //         asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
            //         asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t));

            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2));
            //         asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));
            //         asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2));

            //     }
            // }

            // // ---------------------------------------------------------------
            // // Step 3: transpose dscratch (value-major) back into the real,
            // // query-major datt array, accumulating (datt is +=, like naive).
            // // ---------------------------------------------------------------
            // for (int t2 = 0; t2 < T; t2++) {
            //     float* draw_row = dscratch + t2 * T + t2;                          // contiguous
            //     float* datt_col = datt + b*NH*T*T + h*T*T + t2*T + t2;             // strided (col t2)
            //     avl = T - t2;
            //     asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));
            //     asm volatile("vle32.v  v0, (%0)"       :: "r"(draw_row));
            //     asm volatile("vlse32.v v8, (%0), %1"   :: "r"(datt_col), "r"(stridesize_patt));
            //     asm volatile("vfadd.vv v0, v0, v8");
            //     asm volatile("vsse32.v v0, (%0), %1"   :: "r"(datt_col), "r"(stridesize_patt));
            // }

            // // ---------------------------------------------------------------
            // // Step 4: dvalue[t2] += sum_{t>=t2} att[t][t2] * dout[t].
            // // Naturally vectorized over hs already (no reduction needed) --
            // // same shape as attention_forward_rev2's pass 4 (weighted value
            // // accumulation), just with the roles of t/t2 transposed. The
            // // accumulator is kept resident in a vector register across the
            // // whole inner t loop instead of reloading/storing every
            // // iteration, since t2 (and hence the target address) is fixed
            // // for the whole inner loop here.
            // // ---------------------------------------------------------------
            // for (int t2 = 0; t2 < T; t2++) {
            //     float* dvalue_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C*2;
            //     int itr = 0;
            //     int num_elements = hs;
            //     asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            //     while (itr*VLMAX + VLMAX <= num_elements) {
            //         asm volatile("vle32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
            //         for (int t = t2; t < T; t++) {
            //             float* dout_bth = dout + b*T*C + t*C + h*hs + itr*VLMAX;
            //             float  w = att[b*NH*T*T + h*T*T + t*T + t2];
            //             asm volatile("vle32.v    v0 , (%0)" :: "r"(dout_bth));
            //             asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(w));
            //         }
            //         asm volatile("vse32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
            //         itr++;
            //     }
            //     if (itr*VLMAX < num_elements) {
            //         asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            //         asm volatile("vle32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
            //         for (int t = t2; t < T; t++) {
            //             float* dout_bth = dout + b*T*C + t*C + h*hs + itr*VLMAX;
            //             float  w = att[b*NH*T*T + h*T*T + t*T + t2];
            //             asm volatile("vle32.v    v0 , (%0)" :: "r"(dout_bth));
            //             asm volatile("vfmacc.vf  v16, %[A], v0" ::[A] "f"(w));
            //         }
            //         asm volatile("vse32.v v16, (%0)" :: "r"(dvalue_t2 + itr*VLMAX));
            //     }
            // }

            // // ---------------------------------------------------------------
            // // Step 5: softmax backward, closed-form / single-reduction per
            // // row instead of naive's O(t)-outer-loop-of-O(t)-vector-passes:
            // //     S           = sum_{i<=t} att[i] * datt[i]      (ONE reduce)
            // //     dpreatt[i] += att[i] * (datt[i] - S)           for i<=t
            // // This replaces naive's O(T^3) total vector work with O(T^2).
            // // ---------------------------------------------------------------
            // for (int t = 0; t < T; t++) {
            //     float* att_bth     = att     + b*NH*T*T + h*T*T + t*T;
            //     float* datt_bth    = datt    + b*NH*T*T + h*T*T + t*T;
            //     float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
            //     int num_elements = t + 1;
            //     int itr = 0;
            //     float S = 0.f;

            //     asm volatile("vsetvli  %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            //     asm volatile("vfsub.vv v31,v31,v31");
            //     asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            //     while (itr*VLMAX + VLMAX <= num_elements) {
            //         float partial;
            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth  + itr*VLMAX));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(datt_bth + itr*VLMAX));
            //         asm volatile("vfmul.vv    v0 , v0, v8");
            //         asm volatile("vfredsum.vs v16, v0, v31");
            //         asm volatile("vfmv.f.s    %0 , v16" : "=f"(partial));
            //         S += partial;
            //         itr++;
            //     }
            //     if (itr*VLMAX < num_elements) {
            //         float partial;
            //         asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            //         asm volatile("vle32.v     v0 , (%0)" :: "r"(att_bth  + itr*VLMAX));
            //         asm volatile("vle32.v     v8 , (%0)" :: "r"(datt_bth + itr*VLMAX));
            //         asm volatile("vfmul.vv    v0 , v0, v8");
            //         asm volatile("vfredsum.vs v16, v0, v31");
            //         asm volatile("vfmv.f.s    %0 , v16" : "=f"(partial));
            //         S += partial;
            //     }

            //     itr = 0;
            //     asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            //     while (itr*VLMAX + VLMAX <= num_elements) {
            //         asm volatile("vle32.v    v0 , (%0)" :: "r"(att_bth     + itr*VLMAX));
            //         asm volatile("vle32.v    v8 , (%0)" :: "r"(datt_bth    + itr*VLMAX));
            //         asm volatile("vle32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
            //         asm volatile("vfsub.vf   v8 , v8, %[A]" ::[A] "f"(S));
            //         asm volatile("vfmacc.vv  v16, v0, v8");
            //         asm volatile("vse32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
            //         itr++;
            //     }
            //     if (itr*VLMAX < num_elements) {
            //         asm volatile("vsetvli    %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            //         asm volatile("vle32.v    v0 , (%0)" :: "r"(att_bth     + itr*VLMAX));
            //         asm volatile("vle32.v    v8 , (%0)" :: "r"(datt_bth    + itr*VLMAX));
            //         asm volatile("vle32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
            //         asm volatile("vfsub.vf   v8 , v8, %[A]" ::[A] "f"(S));
            //         asm volatile("vfmacc.vv  v16, v0, v8");
            //         asm volatile("vse32.v    v16, (%0)" :: "r"(dpreatt_bth + itr*VLMAX));
            //     }
            // }

            // // ---------------------------------------------------------------
            // // Step 6: dquery/dkey from dpreatt -- unchanged from naive, since
            // // this part of attention_backward was already vectorized the
            // // good way (pure vfmacc.vf over hs, no reduction involved).
            // // ---------------------------------------------------------------
            // for (int t = 0; t < T; t++) {
            //     float* dpreatt_bth = dpreatt + b*NH*T*T + h*T*T + t*T;
            //     float* dquery_t = dinp + b * T * C3 + t * C3 + h * hs;
            //     float* query_t  = inp  + b * T * C3 + t * C3 + h * hs;

            //     for (int t2 = 0; t2 <= t; t2++) {
            //         float* key_t2  = inp  + b * T * C3 + t2 * C3 + h * hs + C;
            //         float* dkey_t2 = dinp + b * T * C3 + t2 * C3 + h * hs + C;
            //         float sc = dpreatt_bth[t2] * scale;
            //         int itr = 0;
            //         int num_elements = hs;

            //         asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            //         while (itr*VLMAX + VLMAX <= num_elements) {
            //             asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   + itr*VLMAX));
            //             asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));
            //             asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
            //             asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));

            //             asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  + itr*VLMAX));
            //             asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
            //             asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
            //             asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
            //             itr++;
            //         }
            //         if (itr*VLMAX < num_elements) {
            //             asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
            //             asm volatile("vle32.v     v0 , (%0)" :: "r"(key_t2   + itr*VLMAX));
            //             asm volatile("vle32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));
            //             asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
            //             asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t + itr*VLMAX));

            //             asm volatile("vle32.v     v0 , (%0)" :: "r"(query_t  + itr*VLMAX));
            //             asm volatile("vle32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
            //             asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(sc));
            //             asm volatile("vse32.v     v8 , (%0)" :: "r"(dkey_t2  + itr*VLMAX));
            //         }
            //     }
            // }
        }
    }
}

#define GELU_SCALING_FACTOR 0.797884561f
void gelu_forward(float* out, float* inp, float* temp, float* temp2, int N, unsigned int vl) {
    // // (approximate) GeLU elementwise non-linearity in the MLP block of Transformer

    int itr = 0;
    int num_elements = N;
    //inp      V0 - V7
    //cube     V8 - V15
    //out      V8 - V15
    // asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp      +itr*VLMAX));
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));

        //----------------------  BUILT IN TANH
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
        // snrt_cluster_hw_barrier();
        // for (int i = 0; i < VLMAX; i++){
        //     out[itr*VLMAX + i] = tanhf(out[itr*VLMAX + i]);
        // }     
        // asm volatile("vle32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));

        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        // asm volatile("vfmul.vv    v8 , v0, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
        // itr++;

        //----------------------  FAST TANH V1
        snrt_cluster_hw_barrier();
        asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX/2));
        asm volatile("vse32.v     v12 , (%0)" :: "r"(temp2));        
        fast_tanh_v1(temp, temp, temp, 0, 8, (VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
        // printf("1\n");
        asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));

        asm volatile("vle32.v     v8 , (%0)" :: "r"(temp2));        
        fast_tanh_v1(temp, temp, temp, 0, 8, (VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
        // printf("2\n");

        asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        asm volatile("vfmul.vv    v8 , v4, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX+VLMAX/2));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp      +itr*VLMAX));
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));
        //----------------------  BUILT IN TANH
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
        // for (int i = 0; i < num_elements - itr*VLMAX; i++){
        //     out[itr*VLMAX + i] = tanhf(out[itr*VLMAX + i]);
        // }
        // asm volatile("vle32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));

        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        // asm volatile("vfmul.vv    v8 , v0, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
        //----------------------  FAST TANH
        snrt_cluster_hw_barrier();
        if(num_elements - itr*VLMAX >= VLMAX/2){
            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX - VLMAX/2));
            asm volatile("vse32.v     v12 , (%0)" :: "r"(temp2));

            fast_tanh_v1(temp, temp, temp, 0, 8, (VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL        
            asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
            asm volatile("vfmul.vv    v8 , v0, v8");
            asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
            asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));

            asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX - VLMAX/2));
            asm volatile("vle32.v     v8 , (%0)" :: "r"(temp2));
            fast_tanh_v1(temp, temp, temp, 0, 8, (num_elements - itr*VLMAX - VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
            asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
            asm volatile("vfmul.vv    v8 , v4, v8");
            asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
            asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX + VLMAX/2));
        }else{
            fast_tanh_v1(temp, temp, temp, 0, 8, (num_elements - itr*VLMAX), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
            asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
            asm volatile("vfmul.vv    v8 , v0, v8");
            asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
            asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
        }
    }
}




void gelu_backward(float* dinp, float* inp, float* dout,
                   float* temp, float* temp1, float* temp2,
                   int N, unsigned int vl) {
    // for (int i = 0; i < N; i++) {
    //     float x = inp[i];
    //     float cube = 0.044715f * x * x * x;
    //     float tanh_arg = GELU_SCALING_FACTOR * (x + cube);
    //     // float tanh_out = tanhf(tanh_arg);//I CHANGED THIS LINE
    //     float tanh_out = tanh_arg;
    //     // float coshf_out = coshf(tanh_arg);//I CHANGED THIS LINE
    //     float coshf_out = tanh_arg;
    //     float sech_out = 1.0f / (coshf_out * coshf_out);
    //     float local_grad = 0.5f * (1.0f + tanh_out) + x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x);
    //     dinp[i] += local_grad * dout[i];
    // }
    int itr = 0;
    int num_elements = N;
    // float temp = 0;
    //FIT EVERYTHING IN VREGF AFTER ADDING TANHF AND COSHF
    //THERE IS A PROBLEM WITH asm volatile("vfdiv.vv    v24  , v24, v16");
    //  WHEN THE VD IS THE SAME AS VS1/VS2
    //inp        V0 - V7
    //cube       V8 - V15
    //tanh_arg   V8 - V15
    //tanh_out   V16- V23
    //cosh_out   V8 - V16
    //sech_out   V8 - V16 
    //---------------------------------------------------------
    //SOMul      V8 - V16
    //XMul       V0 - V7
    //THMul      V16- V23
    //local_grad V0 - V7
    //dout       V8 - V15
    //dout       V16- V24
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp      +itr*VLMAX));
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));//tanh_arg

        //----------------------  BUILT IN TANH
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(temp+itr*VLMAX));
        // snrt_cluster_hw_barrier();
        // for (int i = 0; i < VLMAX; i++){
        //     temp1[itr*VLMAX + i] = tanhf(temp[itr*VLMAX + i]);
        //     temp2[itr*VLMAX + i] = coshf(temp[itr*VLMAX + i]);
        // }     

        // asm volatile("vle32.v     v8 , (%0)" :: "r"(temp2+itr*VLMAX));//cosh_out
        // asm volatile("vle32.v     v16, (%0)" :: "r"(temp1+itr*VLMAX));//tanh_out

        //----------------------  FAST TANH/COSH V2
        asm volatile("vse32.v     v8 , (%0)" :: "r"(temp+itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp+itr*VLMAX));
        snrt_cluster_hw_barrier();

        fast_tanh_v2(temp1, temp1, temp1, 0, 16, (VLMAX), vl);
        // printf("1\n");
        asm volatile("vse32.v     v16 , (%0)" :: "r"(temp1+itr*VLMAX));

        fast_cosh_v2(temp2, temp2, temp2, 0, 8, (VLMAX), vl);
        // printf("2\n");

        //LOAD THE TANH BACK
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp1+itr*VLMAX));//tanh_out

        //---------------------------------------------------------
        asm volatile("vfmul.vv    v8, v8, v8");//sech_out
        asm volatile("vfmul.vv    v24 , v0, v0");//x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.134145f));//3.0f * 0.044715f * x * x
        asm volatile("vfadd.vf    v24 , v24, %[A] " ::[A] "f"(1.0f));//1.0f + 3.0f * 0.044715f * x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.5f));
        asm volatile("vfmul.vv    v24 , v24, v0");//x * 0.5f *  GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)
        asm volatile("vfdiv.vv    v0  , v24, v8");//x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)

        asm volatile("vfadd.vf    v16, v16, %[A] " ::[A] "f"(1.0f));//(1.0f + tanh_out)
        asm volatile("vfmul.vf    v16, v16, %[A] " ::[A] "f"(0.5f));//0.5f * (1.0f + tanh_out)
      
        asm volatile("vfadd.vv    v0 , v16, v0");
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dout      +itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp      +itr*VLMAX));

        asm volatile("vfmacc.vv    v8 , v16, v0");
        //---------------------------------------------------------
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp +itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp      +itr*VLMAX));
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));//tanh_arg

        //----------------------  BUILT IN TANH
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(temp+itr*VLMAX));
        // snrt_cluster_hw_barrier();
        // for (int i = 0; i < num_elements - itr*VLMAX; i++){
        //     temp1[itr*VLMAX + i] = tanhf(temp[itr*VLMAX + i]);
        //     temp2[itr*VLMAX + i] = coshf(temp[itr*VLMAX + i]);
        // }     
        // asm volatile("vle32.v     v8 , (%0)" :: "r"(temp2+itr*VLMAX));//cosh_out
        // asm volatile("vle32.v     v16, (%0)" :: "r"(temp1+itr*VLMAX));//tanh_out

        //----------------------  FAST TANH/COSH V2
        asm volatile("vse32.v     v8 , (%0)" :: "r"(temp+itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp+itr*VLMAX));
        snrt_cluster_hw_barrier();

        fast_tanh_v2(temp1, temp1, temp1, 0, 16, (num_elements - itr*VLMAX), vl);
        asm volatile("vse32.v     v16 , (%0)" :: "r"(temp1+itr*VLMAX));

        fast_cosh_v2(temp2, temp2, temp2, 0, 8, (num_elements - itr*VLMAX), vl);
        //LOAD THE TANH BACK
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp1+itr*VLMAX));//tanh_out

        //---------------------------------------------------------
        asm volatile("vfmul.vv    v8, v8, v8");//sech_out
        asm volatile("vfmul.vv    v24 , v0, v0");//x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.134145f));//3.0f * 0.044715f * x * x
        asm volatile("vfadd.vf    v24 , v24, %[A] " ::[A] "f"(1.0f));//1.0f + 3.0f * 0.044715f * x * x
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));
        asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.5f));
        asm volatile("vfmul.vv    v24 , v24, v0");//x * 0.5f *  GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)
        asm volatile("vfdiv.vv    v0  , v24, v8");//x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)

        asm volatile("vfadd.vf    v16, v16, %[A] " ::[A] "f"(1.0f));//(1.0f + tanh_out)
        asm volatile("vfmul.vf    v16, v16, %[A] " ::[A] "f"(0.5f));//0.5f * (1.0f + tanh_out)
      
        asm volatile("vfadd.vv    v0 , v16, v0");
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dout      +itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp      +itr*VLMAX));

        asm volatile("vfmacc.vv    v8 , v16, v0");
        //---------------------------------------------------------
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp +itr*VLMAX));        
        // asm volatile("vle32.v     v0 , (%0)" :: "r"(inp      +itr*VLMAX));
        // asm volatile("vfmul.vv    v8 , v0, v0");
        // asm volatile("vfmul.vv    v8 , v0, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        // asm volatile("vfadd.vv    v8 , v8, v0");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));//tanh_arg
        // asm volatile("vfmul.vv    v16, v8, v8");//sech_out
        // //---------------------------------------------------------
        // asm volatile("vfmul.vv    v24 , v0, v0");//x * x
        // asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.134145f));//3.0f * 0.044715f * x * x
        // asm volatile("vfadd.vf    v24 , v24, %[A] " ::[A] "f"(1.0f));//1.0f + 3.0f * 0.044715f * x * x
        // asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));
        // asm volatile("vfmul.vf    v24 , v24, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vfmul.vv    v24 , v24, v0");//x * 0.5f *  GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)
        // asm volatile("vfdiv.vv    v0  , v24, v16");//x * 0.5f * sech_out * GELU_SCALING_FACTOR * (1.0f + 3.0f * 0.044715f * x * x)

        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));//(1.0f + tanh_out)
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));//0.5f * (1.0f + tanh_out)
      
        // asm volatile("vfadd.vv    v0 , v8, v0");
        // asm volatile("vle32.v     v8 , (%0)" :: "r"(dout      +itr*VLMAX));
        // asm volatile("vle32.v     v16, (%0)" :: "r"(dinp      +itr*VLMAX));

        // asm volatile("vfmacc.vv    v8 , v16, v0");
        // //---------------------------------------------------------
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp +itr*VLMAX));
    }

}

void residual_forward(float* out, float* inp1, float* inp2, int N, unsigned int vl) {
    int itr = 0;
    int num_elements = N;
    float temp = 0;
    //inp1     V0 - V7
    //inp2     V8 - V15
    //out      V8 - V15
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp1 +itr*VLMAX));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(inp2 +itr*VLMAX));
        asm volatile("vfadd.vv    v8 , v0, v8");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out  +itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp1 +itr*VLMAX));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(inp2 +itr*VLMAX));
        asm volatile("vfadd.vv    v8 , v0, v8");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out  +itr*VLMAX));
    }
}

void residual_backward(float* dinp1, float* dinp2, float* dout, int N, unsigned int vl) {
    int itr = 0;
    int num_elements = N;
    float temp = 0;
    //inp1     V0 - V7
    //inp2     V8 - V15
    //out      V8 - V15
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
        asm volatile("vle32.v     v0 , (%0)" :: "r"(dout  +itr*VLMAX));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dinp1 +itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp2 +itr*VLMAX));
        asm volatile("vfadd.vv    v8 , v0, v8 ");
        asm volatile("vfadd.vv    v16, v0, v16");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp1 +itr*VLMAX));
        asm volatile("vse32.v     v16, (%0)" :: "r"(dinp2 +itr*VLMAX));
        itr++;
    }
    if(itr*VLMAX < num_elements){
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
        asm volatile("vle32.v     v0 , (%0)" :: "r"(dout  +itr*VLMAX));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dinp1 +itr*VLMAX));
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp2 +itr*VLMAX));
        asm volatile("vfadd.vv    v8 , v0, v8 ");
        asm volatile("vfadd.vv    v16, v0, v16");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp1 +itr*VLMAX));
        asm volatile("vse32.v     v16, (%0)" :: "r"(dinp2 +itr*VLMAX));
    }
}

void softmax_forward(float* probs, float* logits, float* temp, int B, int T, int V, int Vp, unsigned int vl) {
    // output: probs are (B,T,Vp) of the probabilities (sums to 1.0 in each b,t position)
    // input: logits is (B,T,Vp) of the unnormalized log probabilities
    // Vp is the padded vocab size (for efficiency), V is the "real" vocab size
    // example: Vp is 50304 and V is 50257
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // probs <- softmax(logits)
            float* logits_bt = logits + b * T * Vp + t * Vp;
            float* probs_bt = probs + b * T * Vp + t * Vp;

            // maxval is only calculated and subtracted for numerical stability
            float maxval = -10000.0f; // TODO something better
            int itr = 0;
            int num_elements = V;
            //logits_bt    V0 - V7
            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            
            //FIXING THE ISSUE OF THIS INSTRUCTION FOR WRITING BACK THE V31 in maxval
            // asm volatile("vfmv.s.f v31, %0"  : "=f"(maxval));
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt  +itr*VLMAX));
                asm volatile("vfredmax.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(maxval));
                asm volatile("vfmv.s.f v31, %0"  : "=f"(maxval));//BUG
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt  +itr*VLMAX));
                asm volatile("vfredmax.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(maxval));
            }
            snrt_cluster_hw_barrier();
            snrt_cluster_hw_barrier();//ISSUE

            float sum = 0.0f;
            itr = 0;
            num_elements = V;
            //logits_bt    V0 - V7
            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt  +itr*VLMAX));
                asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval));

                //----------------------  BUILT IN EXP
                // asm volatile("vse32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));
                // snrt_cluster_hw_barrier();
                // for (int i = 0; i < VLMAX; i++){
                //     probs_bt[itr*VLMAX + i] = expf(temp[itr*VLMAX + i]);
                // }     
                // asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt+itr*VLMAX));

                //----------------------  FAST EXP
                fast_exp(temp, probs_bt+itr*VLMAX, 0, 0, 8, VLMAX, vl);
                asm volatile("vse32.v     v0 , (%0)" :: "r"(probs_bt+itr*VLMAX));

                asm volatile("vfredsum.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(sum));
                asm volatile("vfmv.s.f v31, %0"  : "=f"(sum));//BUG
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt  +itr*VLMAX));
                asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval));

                //----------------------  BUILT IN EXP
                // asm volatile("vse32.v     v0 , (%0)" :: "r"(temp+itr*VLMAX));
                // snrt_cluster_hw_barrier();
                // for (int i = 0; i < num_elements - itr*VLMAX; i++){
                //     probs_bt[itr*VLMAX + i] = expf(temp[itr*VLMAX + i]);
                // }     
                // asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt+itr*VLMAX));
                
                //----------------------  FAST EXP
                fast_exp(temp, probs_bt+itr*VLMAX, 0, 0, 8, (num_elements - itr*VLMAX), vl);
                asm volatile("vse32.v     v0 , (%0)" :: "r"(probs_bt+itr*VLMAX));
                asm volatile("vfredsum.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(sum));
            }

            // THERE IS A PROBLEM WITH VSETVLI
            itr = 0;
            num_elements = V;
            //probs_bt    V0 - V7
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt  +itr*VLMAX));
                asm volatile("vfdiv.vf    v8, v0, %[A]" ::[A] "f"(sum));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(probs_bt   +itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt  +itr*VLMAX));
                asm volatile("vfdiv.vf    v8, v0, %[A]" ::[A] "f"(sum));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(probs_bt   +itr*VLMAX));
            }
            // for extra super safety we may wish to include this too,
            // forcing the probabilities here to be zero, but it shouldn't matter
            for (int i = V; i < Vp; i++) {
                probs_bt[i] = 0.0f;
            }
        }
    }
}

void crossentropy_forward(float* losses,
                          float* probs, int* targets,
                          int B, int T, int Vp) {
    // output: losses is (B,T) of the individual losses at each position
    // input: probs are (B,T,Vp) of the probabilities
    // input: targets is (B,T) of integers giving the correct index in logits
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // loss = -log(probs[target])
            float* probs_bt = probs + b * T * Vp + t * Vp;
            int ix = targets[b * T + t];
            losses[b * T + t] = -logf(probs_bt[ix]);
        }
    }
}

void crossentropy_softmax_backward(float* dlogits,
                           float* dlosses, float* probs, int* targets,
                           int B, int T, int V, int Vp, 
                           unsigned int vl) {
    // backwards through both softmax and crossentropy
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* dlogits_bt = dlogits + b * T * Vp + t * Vp;
            float* probs_bt = probs + b * T * Vp + t * Vp;
            float dloss = dlosses[b * T + t];
            int ix = targets[b * T + t];
            // note we only loop to V, leaving the padded dimensions
            // of dlogits untouched, so gradient there stays at zero
            if(ix <= V){
                dlogits_bt[ix] -= dloss;
            }
            int itr = 0;
            int num_elements = V;
            //probs_bt    V0 - V7
            //dlogits_bt  V8 - V16
            asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
            while(itr*VLMAX + VLMAX <= num_elements){
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt    +itr*VLMAX));
                asm volatile("vle32.v     v8 , (%0)" :: "r"(dlogits_bt  +itr*VLMAX));
                asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(dloss));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(dlogits_bt  +itr*VLMAX));
                itr++;
            }
            if(itr*VLMAX < num_elements){
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt    +itr*VLMAX));
                asm volatile("vle32.v     v8 , (%0)" :: "r"(dlogits_bt  +itr*VLMAX));
                asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(dloss));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(dlogits_bt  +itr*VLMAX));
            }
        }
    }
}



int main() {

    volatile int errors = 0;
    
    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid       = snrt_cluster_core_idx();
    snrt_cluster_hw_barrier();

    // Reset timer
    unsigned int timer = (unsigned int)-1;
    int B = B_Size;
    int C = C_Size;
    int T = T_Size;    

    
    int  * inp_int  ;
    float* inp      ;
    float* dinp     ;
    float* dinp_t   ;//TEMP
    float* mean     ;
    float* wte      ;
    float* wpe      ;
    float* dwte     ;
    float* dwpe     ;
    float* rstd     ;
    float* weight   ;
    float* weightT  ;//TRANSPOSED
    float* dweight  ;
    float* dweight_t;//TEMP
    float* bias     ;
    float* dbias    ;
    float* out      ;
    float* dout     ;
    float* doutT    ;//TRANSPOSED
    float* preatt   ;
    float* dpreatt  ;
    float* att      ;
    float* datt     ;
    float* losses   ;
    float* probs    ;
    int  * targets  ;
    float* dlosses  ;
    float* dlogits  ;
    float* temp     ;
    float* temp1    ;
    float* temp2    ;
    float* mask     ;
    float* max_arr  ;
    float* dscratch ;

    int NH = 2;

    if (cid == 0) {

        // temp        = (float*)snrt_l1alloc(B * T * C * sizeof(float));
        // temp1       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
        // temp2       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
        snrt_dma_wait_all();
    }

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    unsigned int vl;
    // Start timer

    // timer = benchmark_get_cycle();
    // Start dump

    //--------------------------------------------------------- encoder_forward
    // if (cid == 0) {
    //     inp_int     = (int  *)snrt_l1alloc(B * T *     sizeof(int  ));
    //     wte         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     wpe         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp_int, inp_dram  , B * T *     sizeof(int  ));
    //     snrt_dma_start_1d(wte    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(wpe    , data1_dram, B * T * C * sizeof(float));
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
    //     dwte         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dwpe         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dout         = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp_int , inp_dram  , B * T *     sizeof(int  ));
    //     snrt_dma_start_1d(dwte    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dwpe    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dout    , data1_dram, B * T * C * sizeof(float));
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
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     mean      = (float*)snrt_l1alloc(B * T * 1 * sizeof(float));
    //     rstd      = (float*)snrt_l1alloc(B * T * 1 * sizeof(float));
    //     weight    = (float*)snrt_l1alloc(1 * C * C * sizeof(float));
    //     bias      = (float*)snrt_l1alloc(1 * 1 * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * 1 * C * sizeof(float));
    //     snrt_dma_start_1d(bias   , data1_dram, 1 * 1 * C * sizeof(float));
    //     snrt_dma_start_1d(mean   , data1_dram, B * T * 1 * sizeof(float));
    //     snrt_dma_start_1d(rstd   , data1_dram, B * T * 1 * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // layernorm_forward(out, mean, rstd, inp, weight, bias, B, T, C, vl);
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
    // if (cid == 0) {
    //     inp    = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dinp   = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     mean   = (float*)snrt_l1alloc(B * T * 1 * sizeof(float));
    //     rstd   = (float*)snrt_l1alloc(B * T * 1 * sizeof(float));
    //     dweight= (float*)snrt_l1alloc(1 * 1 * C * sizeof(float));
    //     weight = (float*)snrt_l1alloc(1 * 1 * C * sizeof(float));
    //     dbias  = (float*)snrt_l1alloc(1 * 1 * C * sizeof(float));
    //     dout   = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp   , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dweight, data1_dram, 1 * 1 * C * sizeof(float));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * 1 * C * sizeof(float));
    //     snrt_dma_start_1d(dbias  , data1_dram, 1 * 1 * C * sizeof(float));
    //     snrt_dma_start_1d(mean   , data1_dram, B * T * 1 * sizeof(float));
    //     snrt_dma_start_1d(rstd   , data1_dram, B * T * 1 * sizeof(float));
    //     snrt_dma_start_1d(dout   , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // layernorm_backward(dinp, dweight, dbias, dout, inp, weight, mean, rstd, B, T, C, vl);
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
    // int OC = C;
    // if (cid == 0) {
    //     inp     = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     weight  = (float*)snrt_l1alloc(1 * OC* C * sizeof(float));
    //     weightT = (float*)snrt_l1alloc(1 * OC* C * sizeof(float));
    //     bias    = (float*)snrt_l1alloc(1 * 1 * OC* sizeof(float));
    //     out     = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * OC* C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(bias   , data1_dram, 1 * 1 * OC* sizeof(float));
    //     snrt_dma_start_1d(out    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // matmul_forward_naive(out, inp, weight, weightT, bias, B, T, C, C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out    , outG   , B * T * C);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- matmul_backward
    // int OC = C;
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dinp      = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     // dinp_t    = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dweight   = (float*)snrt_l1alloc(1 * OC* C * sizeof(float));
    //     // dweight_t = (float*)snrt_l1alloc(1 * OC* C * sizeof(float));
    //     weight    = (float*)snrt_l1alloc(1 * OC* C * sizeof(float));
    //     dbias     = (float*)snrt_l1alloc(1 * 1 * OC* sizeof(float));
    //     doutT     = (float*)snrt_l1alloc(B * T * OC* sizeof(float));
    //     dout      = (float*)snrt_l1alloc(B * T * OC* sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp   , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dweight, data1_dram, 1 * OC* C * sizeof(float));
    //     snrt_dma_start_1d(weight , data1_dram, 1 * OC* C * sizeof(float));
    //     snrt_dma_start_1d(dbias  , data1_dram, 1 * 1 * OC* sizeof(float));
    //     snrt_dma_start_1d(dout   , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // matmul_backward(dinp, dinp_t, dweight, dweight_t, dbias, dout, doutT, inp, weight, B, T, C, C, vl);
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
    // T = T/2;
    //-------------------- MAX UTIL
    // B = 1;
    // NH = 1;
    // C = 64;
    // T = 64;
    // //--------------------
    // if (cid == 0) {
    //     temp    = (float*)snrt_l1alloc(VLMAX     * sizeof(float));
    //     inp     = (float*)snrt_l1alloc(B * T  * 3*C   * sizeof(float));
    //     preatt  = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     att     = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     out     = (float*)snrt_l1alloc(B * T  * C     * sizeof(float));
    //     for (int i = 0; i < 3; i++){
    //         snrt_dma_start_1d(inp    + i*(B * T * C)         , data1_dram, B * T  * C        * sizeof(float));
    //     }
    //     snrt_dma_start_1d(preatt , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(att    , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(out    , data1_dram, B * T * C * sizeof(float));    
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // attention_forward(out, preatt, att, inp, temp, B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out     , outG   , B * T * C);
    //     printf("CHECK RESULTS2\n");
    //     check_result(preatt  , preattG, B * NH * T * T);
    //     printf("CHECK RESULTS3\n");
    //     check_result(att     , attG   , B * NH * T * T);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- attention_forward_v2
    // T = T/2;
    //-------------------- MAX UTIL
    // B = 1;
    // NH = 1;
    // C = 64;
    // T = 64;
    // //--------------------
    // if (cid == 0) {
    //     temp2   = (float*)snrt_l1alloc(T     * sizeof(float));
    //     max_arr = (float*)snrt_l1alloc(T     * sizeof(float));
    //     temp    = (float*)snrt_l1alloc(T*T     * sizeof(float));
    //     mask    = (float*)snrt_l1alloc(3*16    * sizeof(float));
    //     inp     = (float*)snrt_l1alloc(B * T  * 3*C   * sizeof(float));
    //     preatt  = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     att     = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     out     = (float*)snrt_l1alloc(B * T  * C     * sizeof(float));
    //     for (int i = 0; i < 3; i++){
    //         snrt_dma_start_1d(inp    + i*(B * T * C)         , data1_dram, B * T  * C        * sizeof(float));
    //     }
    //     snrt_dma_start_1d(preatt , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(att    , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(out    , data1_dram, B * T * C * sizeof(float));    
    //     snrt_dma_wait_all();
    // }

    // for (int i = 0; i < T; i++){
    //     for (int j = 0; j < T; j++){
    //         if(j > i){
    //             preatt[i*T+j] = 0;
    //             // att[i*T+j] = 0;
    //         }
    //         att[i*T+j] = 0;

    //     }
    // }

    // for (int i = 0; i < 3*16; i++){
    //     mask[i] = 1;
    // }
    // mask[0] = 0;

    // mask[16] = 0;
    // mask[16+1] = 0;

    // mask[2*16] = 0;
    // mask[2*16+1] = 0;
    // mask[2*16+2] = 0;
    
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // // attention_forward_v2(out, preatt, att, inp, temp, B, T, C, NH, vl);
    // // attention_forward_v4(out, preatt, att, inp, temp, B, T, C, NH, vl);
    // // attention_forward_v5(out, preatt, att, inp, temp, B, T, C, NH, vl);
    // // attention_forward_v6(out, preatt, att, inp, temp, B, T, C, NH, vl);
    // // attention_forward_rev(out, preatt, att, inp, temp, mask, max_arr, temp2,B, T, C, NH, vl);
    // attention_forward_rev2(out, preatt, att, inp, temp, mask, max_arr, temp2,B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
    //     // printf("CHECK RESULTS1\n");
    //     // check_result(out     , outG   , B * T * C);

    // // for (int i = 0; i < T; i++){
    // //     for (int j = 0; j < C; j++){
    // //         if(temp[i+j*T] != inp[i*3*C+j]){
    // //             printf("i = %d\t j = %d\tError\n",i,j);
    // //         }
    // //     }
    // // }

    // // for (int i = 0; i < T; i++){
    // //     for (int j = 0; j < T; j++){
    // //         if(fabs(att[i+j*T] - attG[i*T+j]) > THRESHOLD){
    // //             printf("att[%d][%d] = %f \t attG[%d][%d] = %f \n",i,j, att[i+j*T],j,i, attG[i*T+j]);
    // //         }
    // //     }
    // // }
    

    //     // printf("CHECK RESULTS2\n");
    //     // check_result(preatt  , preattG, B * NH * T * T);
    //     // printf("CHECK RESULTS3\n");
    //     // check_result(att     , attG   , B * NH * T * T);
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- attention_backward
    // T = T/2;
    // //-------------------- MAX UTIL
    // B = 1;
    // NH = 1;
    // C = 64;
    // T = 64;
    // //--------------------    
    // if (cid == 0) {
    //     inp     = (float*)snrt_l1alloc(B * T  * 3*C   * sizeof(float));
    //     dinp    = (float*)snrt_l1alloc(B * T  * 3*C   * sizeof(float));
    //     dpreatt = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     att     = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     datt    = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     dout    = (float*)snrt_l1alloc(B * T  * C     * sizeof(float));
    //     for (int i = 0; i < 3; i++){
    //         snrt_dma_start_1d(inp    + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(float));
    //         snrt_dma_start_1d(dinp   + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(float));
    //     }
    //     snrt_dma_start_1d(dpreatt                       , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(att                           , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(datt                          , data1_dram, B * NH * T * T    * sizeof(float));
    //     snrt_dma_start_1d(dout                          , data1_dram, B * T * C         * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // attention_backward(dinp, dpreatt, datt, dout, inp, att, B, T, C, NH, vl);
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

    // --------------------------------------------------------- attention_backward_rev2
    // Same call-site pattern as the other attention_* blocks in main() (see the commented-out
    // attention_backward block and the active attention_forward_rev2 block). Paste this where
    // the original "attention_backward" block was, then wire dscratch's allocation in alongside
    // the others already declared for the forward_rev2 call (temp, mask).
    
    // T = T/2;
    //-------------------- MAX UTIL
    B = 1;
    NH = 1;
    C = 64;
    T = 32;
    // //--------------------
    if (cid == 0) {
        // temp     = (float*)snrt_l1alloc(T * T         * sizeof(float));
        temp     = (float*)snrt_l1alloc(C * T * sizeof(float));   //
        dscratch = (float*)snrt_l1alloc(T * T         * sizeof(float));
        mask     = (float*)snrt_l1alloc(3*16          * sizeof(float));
        inp      = (float*)snrt_l1alloc(B * T  * 3*C  * sizeof(float));
        dinp     = (float*)snrt_l1alloc(B * T  * 3*C  * sizeof(float));
        dpreatt  = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
        att      = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
        datt     = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
        dout     = (float*)snrt_l1alloc(B * T  * C     * sizeof(float));
        for (int i = 0; i < 3; i++){
            snrt_dma_start_1d(inp    + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(float));
            snrt_dma_start_1d(dinp   + i*(B * T * C)    , data1_dram, B * T  * C        * sizeof(float));
        }
        snrt_dma_start_1d(dpreatt                       , data1_dram, B * NH * T * T    * sizeof(float));
        snrt_dma_start_1d(att                           , data1_dram, B * NH * T * T    * sizeof(float));
        snrt_dma_start_1d(datt                          , data1_dram, B * NH * T * T    * sizeof(float));
        snrt_dma_start_1d(dout                          , data1_dram, B * T * C         * sizeof(float));
        snrt_dma_wait_all();
    }
    
    for (int i = 0; i < T; i++){
        for (int j = 0; j < T; j++){
            if(j > i){
            // if(j < i){
                datt[i*T+j] = 0;
                att[i*T+j] = 0;
            }
        }
    }


    // same causal edge mask attention_forward_rev2 uses -- reuse it as-is
    for (int i = 0; i < 3*16; i++){
        mask[i] = 1;
    }
    mask[0] = 0;
    
    mask[16]   = 0;
    mask[16+1] = 0;
    
    mask[2*16]   = 0;
    mask[2*16+1] = 0;
    mask[2*16+2] = 0;
    
    snrt_cluster_hw_barrier();
    timer = benchmark_get_cycle();
    start_kernel();
    attention_backward_rev3(dinp, dpreatt, datt, dout, inp, att, temp, dscratch, mask, B, T, C, NH, vl);
    stop_kernel();
    if(cid == 0){

        // printf("CHECK RESULTS1\n");
        // for (int i = 0; i < T; i++){
        //     for (int j = 0; j < T; j++){
        //         if(j >= i){
        //             if(fabs(datt[j+i*T] - dattG[j*T+i]) > THRESHOLD){
        //                 printf("datt[%d][%d] = %f \t dattG[%d][%d] = %f \n",i,j, datt[j+i*T],j,i, dattG[j*T+i]);
        //             }
        //         }
        //     }
        // }

        // printf("CHECK RESULTS1\n");
        // check_result(datt     , dattG   , B * NH * T * T);
        // printf("CHECK RESULTS2\n");
        // check_result(dpreatt  , dpreattG, B * NH * T * T);
        // printf("CHECK RESULTS3\n");
        // check_result(dinp     , dinpG   , B *  T * 3*C);
    }
    // ---------------------------------------------------------


    //--------------------------------------------------------- gelu_forward
    // int N = B*T*C;
    // if (cid == 0) {
    //     inp     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     out     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     snrt_dma_start_1d(inp, data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // gelu_forward(out, inp, temp ,temp2 ,B*T*C, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out     , outG   , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- gelu_backward
    // int N = B*T*C;
    // if (cid == 0) {
    //     inp     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     dinp    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     dout    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dout, data1_dram, B * T * C * sizeof(float));
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
    //     inp     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     dinp    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     out     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(float));
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
    //     inp     = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     dinp    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     dout    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp, data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dout, data1_dram, B * T * C * sizeof(float));
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
    // // int Vp = C;
    // int Vp = C+3;
    // if (cid == 0) {
    //     inp    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     out    = (float*)snrt_l1alloc(B * T  * C   * sizeof(float));
    //     snrt_dma_start_1d(inp , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // softmax_forward(out, inp, temp, B, T, C, C, vl);
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
    //     losses       = (float*)snrt_l1alloc(B * T         * sizeof(float));
    //     probs        = (float*)snrt_l1alloc(B * T  * Vp   * sizeof(float));
    //     targets      = (int  *)snrt_l1alloc(B * T         * sizeof(int));
    //     snrt_dma_start_1d(probs  , data1_dram, B * T * Vp * sizeof(float));
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
    //     dlosses     = (float*)snrt_l1alloc(B * T  * sizeof(float));
    //     dlogits     = (float*)snrt_l1alloc(B * T  * Vp   * sizeof(float));
    //     probs       = (float*)snrt_l1alloc(B * T  * Vp   * sizeof(float));
    //     targets     = (int  *)snrt_l1alloc(B * T  * sizeof(int));
    //     snrt_dma_start_1d(dlosses, data1_dram, B * T * sizeof(float));
    //     snrt_dma_start_1d(dlogits, data1_dram, B * T * Vp * sizeof(float));
    //     snrt_dma_start_1d(probs  , data1_dram, B * T * Vp * sizeof(float));
    //     snrt_dma_start_1d(targets, inp_dram  , B * T * sizeof(float));
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
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
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
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < ((B*T*C)/(VLMAX/2)); i++){
    //     fast_cosh(inp+i*(VLMAX/2), out+i*(VLMAX/2), temp , (VLMAX/2), vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    //---------------------------------------------------------

    //--------------------------------------------------------- fast_tanh
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < ((B*T*C)/(VLMAX/2)); i++){
    //     fast_tanh(inp+i*(VLMAX/2), out+i*(VLMAX/2), temp , (VLMAX/2), vl);
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
    return errors;

}
