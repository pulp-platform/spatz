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
        asm volatile("vfmul.vf    v16, v16, %[A]" ::[A] "f"(0.5));
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
        asm volatile("vfmul.vf    v8, v16, %[A]" ::[A] "f"(0.5));
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
    
            // asm volatile("vfdiv.vv v8, v12, v8");
            asm volatile("vfadd.vv v8, v12, v8");//--------------------------!!!!!!!
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

            asm volatile("vfcvt.rtz.xu.f.v v16, v16");
            
            asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(1.0f));
            asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(1.0f));
    
            // asm volatile("vfdiv.vv v8, v16, v8");
            asm volatile("vfadd.vv v8, v16, v8");//--------------------------!!!!!!!
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
    
            // asm volatile("vfdiv.vv v16, v24, v16");
            asm volatile("vfadd.vv v16, v24, v16");   //--------------------------!!!!!!!
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
            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl     -= vl;
                asm volatile("vle32.v v0 , (%0)" :: "r"(wte_ix));
                wte_ix  += vl;
                asm volatile("vle32.v v8 , (%0)" :: "r"(wpe_t ));
                wpe_t   += vl;
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt));
                out_bt  += vl;
            } while (avl > 0);
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

            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl     -= vl;                

                asm volatile("vle32.v v0 , (%0)" :: "r"(dwte_ix));
                // dwte_ix  += vl;

                asm volatile("vle32.v v16, (%0)" :: "r"(dout_bt));
                dout_bt  += vl;

                asm volatile("vfadd.vv v0, v0, v16");

                asm volatile("vle32.v v8 , (%0)" :: "r"(dwpe_t ));
                // dwpe_t   += vl;

                asm volatile("vse32.v v0 , (%0)" :: "r"(dwte_ix));
                dwte_ix  += vl;

                asm volatile("vfadd.vv v8, v8, v16");

                asm volatile("vse32.v v8 , (%0)" :: "r"(dwpe_t ));
                dwpe_t   += vl;

            } while (avl > 0);

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

            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;

                asm volatile("vle32.v v0 , (%0)" :: "r"(x));
                x += vl;
                
                asm volatile("vfredsum.vs v8 , v0 , v31");
                asm volatile("vfmv.f.s %0 , v8" : "=f"(m));//!!!!
                asm volatile("vfmv.s.f v31, %0" : "=f"(m));//!!!!
            } while (avl > 0);
            
            m = m/C;
            // cache the mean for the backward pass later
            mean[b * T + t] = m;//I MOVE THIS HERE OTHERWISE DOES NOT WORK

            // calculate the variance (without any bias correction)
            v = 0.0f;
            
            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            avl = C;
            x = inp + b * T * C + t * C;

            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;

                asm volatile("vle32.v v0 , (%0)" :: "r"(x));
                x += vl;

                asm volatile("vfsub.vf v8, v0, %[A]" ::[A] "f"(m));
                asm volatile("vfmul.vv v8, v8, v8");
                asm volatile("vfredsum.vs v16 , v8 , v31");
                asm volatile("vfmv.f.s %0 , v16" : "=f"(v));
                asm volatile("vfmv.s.f v31, %0 " : "=f"(v));

            } while (avl > 0);
            



            v = v/C;
            float s = 1.0f / sqrtf(v + eps);//I CHANGED THIS LINE
            // float s = 1.0f;//I CHANGED THIS LINE
            rstd[b * T + t] = s;
            // cache the rstd for the backward pass later
            // seek to the output position in out[b,t,:]
            float* out_bt = out + b * T * C + t * C;
            x = inp + b * T * C + t * C;
            float* weight_ptr = weight;
            float* bias_ptr = bias;
            avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v v0  , (%0)" :: "r"(x      ));
                x += vl;
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(m));
                
                asm volatile("vle32.v v8  , (%0)" :: "r"(weight_ptr ));
                weight_ptr += vl;
                
                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(s));
                // asm volatile("vse16.v v0 , (%0)" :: "r"(out_bt));
                asm volatile("vle32.v v16 , (%0)" :: "r"(bias_ptr   ));
                bias_ptr += vl;

                asm volatile("vfmul.vv v8 , v0, v8");

                asm volatile("vfadd.vv v16, v8, v16");

                asm volatile("vse32.v v16 , (%0)" :: "r"(out_bt));
                out_bt += vl;
            } while (avl > 0);
            
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

            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");
            asm volatile("vfsub.vv v30,v30,v30");


            float* weight_ptr = weight;

            int avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v v0  , (%0)" :: "r"(inp_bt));
                inp_bt += vl;
                asm volatile("vfsub.vf v0 , v0, %[A]" ::[A] "f"(mean_bt));

                asm volatile("vle32.v v8  , (%0)" :: "r"(weight_ptr));
                weight_ptr += vl;

                asm volatile("vfmul.vf v0 , v0, %[A]" ::[A] "f"(rstd_bt));

                asm volatile("vle32.v v16 , (%0)" :: "r"(dout_bt));
                dout_bt += vl;

                asm volatile("vfmul.vv v8 , v8, v16");
                


                asm volatile("vfredsum.vs v24 , v8  , v31");
                asm volatile("vfmv.f.s %0 , v24" : "=f"(dnorm_mean));
                // asm volatile("vfmv.s.f v31, %0"  : "=f"(dnorm_mean));// this should be fixed for general form

                asm volatile("vfmul.vv    v16 , v8  , v0");
                asm volatile("vfredsum.vs v25 , v16 , v30");
                asm volatile("vfmv.f.s %0 , v25" : "=f"(dnorm_norm_mean));
                // asm volatile("vfmv.s.f v30, %0"  : "=f"(dnorm_norm_mean));// this should be fixed for general form

                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfmv.v.f v31, %[A]" ::[A] "f"(dnorm_mean));
                asm volatile("vfmv.v.f v30, %[A]" ::[A] "f"(dnorm_norm_mean));

            } while (avl > 0);

            dnorm_mean = dnorm_mean / C;
            dnorm_norm_mean = dnorm_norm_mean / C;

            weight_ptr = weight;
            float *dweight_ptr = dweight;
            float *dbias_ptr = dbias;
            dout_bt = dout + b * T * C + t * C;
            inp_bt  = inp  + b * T * C + t * C;
            avl = C;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v   v0 , (%0)" :: "r"(inp_bt));
                inp_bt += vl;
                asm volatile("vfsub.vf  v0 , v0, %[A]" ::[A] "f"(mean_bt));
                
                asm volatile("vle32.v   v8 , (%0)" :: "r"(dout_bt));
                dout_bt += vl;

                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(rstd_bt));

                asm volatile("vle32.v   v16, (%0)" :: "r"(dbias_ptr));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dbias_ptr));
                dbias_ptr += vl;

                asm volatile("vle32.v   v16, (%0)" :: "r"(dweight_ptr));
                asm volatile("vfmacc.vv v16, v8, v0");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dweight_ptr));
                dweight_ptr += vl;

                asm volatile("vle32.v   v16, (%0)" :: "r"(weight_ptr));
                asm volatile("vfmul.vv  v8 , v8, v16");
                weight_ptr += vl;
                //---------------------------------------------------------------
                asm volatile("vle32.v   v16, (%0)" :: "r"(dinp_bt));
                asm volatile("vfmul.vf  v0 , v0, %[A]" ::[A] "f"(dnorm_norm_mean));
                asm volatile("vfsub.vf  v8 , v8, %[A]" ::[A] "f"(dnorm_mean));
                asm volatile("vfsub.vv  v8 , v8, v0");
                asm volatile("vfmul.vf  v8 , v8, %[A]" ::[A] "f"(rstd_bt));
                asm volatile("vfadd.vv  v16, v8, v16");
                asm volatile("vse32.v   v16, (%0)" :: "r"(dinp_bt));
                dinp_bt += vl;
            } while (avl > 0);
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
    // for (int b = 0; b < B; b++) {
    //     for (int t = 0; t < T; t++) {
    //         int out_bt = out + (b * T + t)*OC;
    //         int itr = 0;
    //         int num_elements = OC;
    //         asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    //         asm volatile("vle32.v v8 , (%0)" :: "r"(bias +itr*VLMAX));
    //         while(itr*VLMAX + VLMAX <= num_elements){
    //             asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
    //             asm volatile("vfadd.vv v16, v0, v8");
    //             asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
    //             itr++;
    //         }
    //         if(itr*VLMAX < num_elements){
    //             asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    //             asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt+itr*VLMAX));
    //             asm volatile("vle32.v v8 , (%0)" :: "r"(bias +itr*VLMAX));
    //             asm volatile("vfadd.vv v16, v0, v8");
    //             asm volatile("vse32.v v16, (%0)" :: "r"(out_bt+itr*VLMAX));
    //         }
    //     }
    // }  
    
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float* out_bt = out + (b * T + t)*OC;
            float* bias_ptr = bias;
            int avl = OC;
            do{
                asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v v0 , (%0)" :: "r"(out_bt));
                asm volatile("vle32.v v8 , (%0)" :: "r"(bias_ptr));
                bias_ptr += vl;
                asm volatile("vfadd.vv v16, v0, v8");
                asm volatile("vse32.v v16, (%0)" :: "r"(out_bt));
                out_bt += vl;
            } while (avl > 0);
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

    for (int i = 0; i < B*T; i++){
        float* src = dout + i * OC;
        float* dst = doutT + i;

        int avl = OC;
        do{
            asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
            avl -= vl;
            asm volatile("vle32.v v0  , (%0)" :: "r"(src));
            src += vl;
            asm volatile("vsse32.v v0, (%0), %1" ::"r"(dst), "r"(B*T * sizeof(float)));            
            dst += vl*B*T;

        } while (avl > 0);
    }


    matmul_4xVL(dweight, doutT, inp, 0, OC, B*T, C, 0, C);

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


void attention_forward_opt(float* out, float* preatt, float* att,
                       float* inp,
                       float* temp,
                       float* mask,
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

void attention_backward_opt(float* dinp, float* dpreatt, float* datt,
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
                // FOR NOW TRANSPOSED TO WHAT SUPPOSED TO BE AND I DRAW IN THE FIGURES
                // // moshkel ine ke man avale kar balaye ghotr ro sefr mikonam vali mogheye store mikham zire ghotr sefr bashan
                // asm volatile("vse32.v v16, (%0)" :: "r"(datt_bth+0*T));
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
                    asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(temp));

                    asm volatile("vle32.v     v12, (%0)" :: "r"(dkey_t2));
                    asm volatile("vfmacc.vf   v12, %[A], v16" ::[A] "f"(temp));
                    asm volatile("vse32.v     v12, (%0)" :: "r"(dkey_t2));

                }
                    asm volatile("vse32.v     v8 , (%0)" :: "r"(dquery_t));
            }
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
        // snrt_cluster_hw_barrier();
        // asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX/2));
        // asm volatile("vse32.v     v12 , (%0)" :: "r"(temp2));        
        // fast_tanh_v1(temp, temp, temp, 0, 8, (VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
        // // printf("1\n");
        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        // asm volatile("vfmul.vv    v8 , v0, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));

        // asm volatile("vle32.v     v8 , (%0)" :: "r"(temp2));        
        // fast_tanh_v1(temp, temp, temp, 0, 8, (VLMAX/2), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
        // // printf("2\n");

        // asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        // asm volatile("vfmul.vv    v8 , v4, v8");
        // asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        // asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX+VLMAX/2));
        // itr++;
        //----------------------  FAST TANH V2
        snrt_cluster_hw_barrier();
        asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
        // asm volatile("vse32.v     v12 , (%0)" :: "r"(temp2));        
        fast_tanh_v2(temp, temp, temp, 0, 8, (VLMAX), vl);// THIS FUNCTION CHANGED THE LMUL AND AVL
        // printf("1\n");
        asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out +itr*VLMAX));
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

void gelu_forward_opt(float* out, float* inp, int N, unsigned int vl) {
    // (approximate) GeLU elementwise non-linearity in the MLP block of Transformer

    int avl = N;

  do {
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
    avl -= vl;
    asm volatile("vle32.v     v0 , (%0)" :: "r"(inp));
    inp += vl;
    asm volatile("vfmul.vv    v8 , v0, v0");
    asm volatile("vfmul.vv    v8 , v0, v8");
    asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
    asm volatile("vfadd.vv    v8 , v8, v0");
    asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));

    //----------------------  FAST TANH V2
    // snrt_cluster_hw_barrier();

    asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(2.0f));

    asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(12102203.0f));
    asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(1064866805.0f));

    asm volatile("vfcvt.rtz.xu.f.v v16, v16");
    
    asm volatile("vfadd.vf v8 , v16, %[A]" ::[A] "f"(1.0f));
    asm volatile("vfsub.vf v16, v16, %[A]" ::[A] "f"(1.0f));

    asm volatile("vfdiv.vv v8, v16, v8");
    // asm volatile("vfadd.vv v8, v16, v8");//--------------------------!!!!!!!
    //----------------------

    asm volatile("vfadd.vf    v8 , v8, %[A] " ::[A] "f"(1.0f));
    asm volatile("vfmul.vv    v8 , v0, v8");
    asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.5f));
    asm volatile("vse32.v     v8 , (%0)" :: "r"(out));
    out += vl;
  } while (avl > 0);
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


void gelu_backward_opt(float* dinp, float* inp, float* dout,
                   float* temp, float* temp1,
                   int N, unsigned int vl) {
    int avl = N;

    do{
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl   -= vl;
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp));
        inp   += vl;
        asm volatile("vfmul.vv    v8 , v0, v0");
        asm volatile("vfmul.vv    v8 , v0, v8");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(0.044715f));
        asm volatile("vfadd.vv    v8 , v8, v0");
        asm volatile("vfmul.vf    v8 , v8, %[A] " ::[A] "f"(GELU_SCALING_FACTOR));//tanh_arg

        //----------------------  FAST TANH V2
        asm volatile("vse32.v     v8 , (%0)" :: "r"(temp));
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp));
        
        asm volatile("vfmul.vf v24, v16, %[A]" ::[A] "f"(2.0f));
        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));

        asm volatile("vfcvt.rtz.xu.f.v v24, v24");

        asm volatile("vfadd.vf v16, v24, %[A]" ::[A] "f"(1.0f));
        asm volatile("vfsub.vf v24, v24, %[A]" ::[A] "f"(1.0f));
        asm volatile("vfdiv.vv v16, v24, v16");

        asm volatile("vse32.v     v16 , (%0)" :: "r"(temp1));

        //----------------------  FAST COSH V2
        asm volatile("vfmul.vf v16, v8, %[A]" ::[A] "f"(-1.0f));

        asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(1064866805.0f));

        asm volatile("vfmul.vf v16, v16, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v16, v16, %[A]" ::[A] "f"(1064866805.0f));

        asm volatile("vfcvt.rtz.xu.f.v v16, v16");
        asm volatile("vfcvt.rtz.xu.f.v v8 , v8");


        asm volatile("vfadd.vv v16, v16, v8");
        asm volatile("vfmul.vf    v8, v16, %[A]" ::[A] "f"(0.5f));

        //---------------------------------------------------------
        // //LOAD THE TANH BACK
        asm volatile("vle32.v     v16, (%0)" :: "r"(temp1));//tanh_out


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
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dout));
        dout  += vl;
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp));

        asm volatile("vfmacc.vv    v8 , v16, v0");
        //---------------------------------------------------------
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp));
        dinp  += vl;

    } while (avl > 0);
}


void residual_forward(float* out, float* inp1, float* inp2, int N, unsigned int vl) {
    int avl = N;
    do{
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle32.v     v0 , (%0)" :: "r"(inp1));
        inp1 += vl;
        asm volatile("vle32.v     v8 , (%0)" :: "r"(inp2));
        inp2 += vl;
        asm volatile("vfadd.vv    v8 , v0, v8");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out));
        out += vl;
    } while (avl > 0);
}

void residual_backward(float* dinp1, float* dinp2, float* dout, int N, unsigned int vl) {
    int avl = N;
    do{
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle32.v     v0 , (%0)" :: "r"(dout));
        dout += vl;
        asm volatile("vle32.v     v8 , (%0)" :: "r"(dinp1));
        asm volatile("vfadd.vv    v8 , v0, v8 ");
        asm volatile("vle32.v     v16, (%0)" :: "r"(dinp2));
        asm volatile("vfadd.vv    v16, v0, v16");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(dinp1));
        dinp1 += vl;
        asm volatile("vse32.v     v16, (%0)" :: "r"(dinp2));
        dinp2 += vl;
    } while (avl > 0);
}

void softmax_forward(float* probs, float* logits, int B, int T, int V, int Vp, unsigned int vl) {
    // output: probs are (B,T,Vp) of the probabilities (sums to 1.0 in each b,t position)
    // input: logits is (B,T,Vp) of the unnormalized log probabilities
    // Vp is the padded vocab size (for efficiency), V is the "real" vocab size
    // example: Vp is 50304 and V is 50257
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // probs <- softmax(logits)
            float* logits_bt = logits + b * T * Vp + t * Vp;
            float* probs_bt = probs + b * T * Vp + t * Vp;

            float maxval = -10000.0f; // TODO something better

            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            int avl = V;
            do{
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt));
                logits_bt += vl;
                asm volatile("vfredmax.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(maxval));
                // asm volatile("vfmv.s.f v31, %0"  : "=f"(maxval));
                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfmv.v.f v31, %[A]" ::[A] "f"(maxval));
            } while (avl > 0);
            // printf("b = %d, t = %d, maxval = %f\t",b,t,maxval);


            // snrt_cluster_hw_barrier();
            // snrt_cluster_hw_barrier();//ISSUE

            float sum = 0.0f;

            asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
            asm volatile("vfsub.vv v31,v31,v31");

            avl = V;
            logits_bt = logits + b * T * Vp + t * Vp;

            do{
                asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v     v0 , (%0)" :: "r"(logits_bt));
                logits_bt += vl;
                asm volatile("vfsub.vf    v0 , v0, %[A]" ::[A] "f"(maxval));
                //----------------------  FAST EXP
                // fast_exp(temp, probs_bt+itr*VLMAX, 0, 0, 8, VLMAX, vl);
                asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
                asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
                asm volatile("vfcvt.rtz.xu.f.v v0, v0");
                //----------------------
                asm volatile("vse32.v     v0 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
                asm volatile("vfredsum.vs v8, v0, v31");
                asm volatile("vfmv.f.s %0 , v8 " : "=f"(sum));
                // asm volatile("vfmv.s.f v31, %0"  : "=f"(sum));
                asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1));
                asm volatile("vfmv.v.f v31, %[A]" ::[A] "f"(sum));                
            } while (avl > 0);
            // printf("sum = %f\n",sum);

            probs_bt = probs + b * T * Vp + t * Vp;
            avl = V;
            do{
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt));
                asm volatile("vfdiv.vf    v8, v0, %[A]" ::[A] "f"(sum));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
            } while (avl > 0);
            
            probs_bt = probs + b * T * Vp + t * Vp;
            // for extra super safety we may wish to include this too,
            // forcing the probabilities here to be zero, but it shouldn't matter
            for (int i = V; i < Vp; i++) {
                probs_bt[i] = 0;
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

            int avl = V;

            do{
                asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
                avl -= vl;
                asm volatile("vle32.v     v0 , (%0)" :: "r"(probs_bt));
                probs_bt += vl;
                asm volatile("vle32.v     v8 , (%0)" :: "r"(dlogits_bt));
                asm volatile("vfmacc.vf   v8 , %[A], v0" ::[A] "f"(dloss));
                asm volatile("vse32.v     v8 , (%0)" :: "r"(dlogits_bt));
                dlogits_bt += vl;
            } while (avl > 0);
        }
    }
}


// void relu_forward(float* out, float* inp, int N, unsigned int vl) {
//     int avl = N;
//     asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(-1));
//     asm volatile("vfsub.vv v0,v0,v0");
//     float *inp_ptr = inp;
//     float *out_ptr = out;
//     do{
//         asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
//         avl -= vl;
//         asm volatile("vle32.v     v8 , (%0)" :: "r"(inp_ptr));
//         inp_ptr += vl;
//         asm volatile("vfmax.vv v8, v0, v8");
//         asm volatile("vse32.v     v8 , (%0)" :: "r"(out_ptr));
//         out_ptr += vl;
//     } while (avl > 0);
// }

void relu_forward(float* out, float* inp, int N, unsigned int vl) {
    int avl = N;
    asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vfsub.vv v0,v0,v0");
    float *inp_ptr = inp;
    float *out_ptr = out;
    do{
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(inp_ptr));
        asm volatile("vfmax.vv v8, v0, v8");
        asm volatile("vse32.v     v8 , (%0)" :: "r"(out_ptr));
        out_ptr += vl;
        avl -= vl;
        inp_ptr += vl;
    } while (avl > 0);
}


void relu_backward(float* dinp, float* inp, float* dout, float* temp, int N, unsigned int vl) {
    int avl = N;
    asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(-1));
    asm volatile("vfsub.vv v0,v0,v0");
    float *inp_ptr = inp;
    float *dinp_ptr = dinp;
    do{
        asm volatile("vsetvli     %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle32.v     v16 , (%0)" :: "r"(dinp_ptr));
        asm volatile("vle32.v     v8 , (%0)" :: "r"(inp_ptr));
        inp_ptr += vl;

        asm volatile("vfmax.vv v8, v0, v8");
        
        asm volatile("vse32.v     v8 , (%0)" :: "r"(temp));
        
        for (int i = 0; i < vl; i++){
            if(temp[i] != 0){
                temp[i] = dout[i];
            }
        }
        
        dout += vl;
        asm volatile("vle32.v     v8 , (%0)" :: "r"(temp));
        temp += vl;
        asm volatile("vfadd.vv v16,v8,v16");
        asm volatile("vse32.v     v16 , (%0)" :: "r"(dinp_ptr));
        dinp_ptr += vl;
    } while (avl > 0);
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
    // // for (int i = 0; i < 32; i++){
    //     encoder_forward(out, inp_int, wte, wpe, B, T, C, vl);
    // // }
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
    // // for (int i = 0; i < 32; i++)
    // // {
    //     encoder_backward(dwte, dwpe, dout, inp_int, B, T, C, vl);
    // // }
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
    //     weight    = (float*)snrt_l1alloc(1 * 1 * C * sizeof(float));
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
    // for (int i = 0; i < 1; i++){
    //     layernorm_forward(out, mean, rstd, inp, weight, bias, B, T, C, vl);
    // }
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
    // // for (int i = 0; i < 16; i++){
    //     layernorm_backward(dinp, dweight, dbias, dout, inp, weight, mean, rstd, B, T, C, vl);
    // // }
    // stop_kernel();
    // // if(cid == 0){
    // //     printf("CHECK RESULTS1\n");
    // //     check_result(dbias    , dbiasG   , 1 * 1 * C);
    // //     printf("CHECK RESULTS2\n");
    // //     check_result(dweight  , dweightG , 1 * 1 * C);
    // //     printf("CHECK RESULTS3\n");
    // //     check_result(dinp     , dinpG    , B * T * C);        
    // // }
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
    // // for (int i = 0; i < 32; i++){
    //     matmul_backward(dinp, dinp_t, dweight, dweight_t, dbias, dout, doutT, inp, weight, B, T, C, C, vl);
    // // }
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

    //--------------------------------------------------------- attention_forward_opt
    // T = T/2;
    //-------------------- MAX UTIL
    // B = 1;
    // NH = 1;
    // C = 64;
    // T = 64;
    //--------------------
    // if (cid == 0) {
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
    //
    // mask[16] = 0;
    // mask[16+1] = 0;
    //
    // mask[2*16] = 0;
    // mask[2*16+1] = 0;
    // mask[2*16+2] = 0;
    //   
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();    
    // attention_forward_opt(out, preatt, att, inp, temp, mask, B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
        // printf("CHECK RESULTS1\n");
        // check_result(out     , outG   , B * T * C);
        // preatt is transposed but there is a problem with 0's for testing.
        // store them in normal order for test.
        // printf("CHECK RESULTS2\n");
        // for (int i = 0; i < T; i++){
        //     for (int j = 0; j < T; j++){
        //         if(fabs(preatt[i+j*T] - preattG[i*T+j]) > THRESHOLD){
        //             printf("preatt[%d][%d] = %f \t preattG[%d][%d] = %f \n",i,j, preatt[i+j*T],j,i, preattG[i*T+j]);
        //         }
        //     }
        // }    
        // printf("CHECK RESULTS2\n");
        // check_result(preatt  , preattG, B * NH * T * T);
        // printf("CHECK RESULTS3\n");
        // check_result(att     , attG   , B * NH * T * T);
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

    // --------------------------------------------------------- attention_backward_opt
    // T = T/2;
    //-------------------- MAX UTIL
    // B = 1;
    // NH = 1;
    // C = 64;
    // T = 64;
    // // //--------------------
    // if (cid == 0) {
    //     // temp     = (float*)snrt_l1alloc(T * T         * sizeof(float));
    //     temp     = (float*)snrt_l1alloc(C * T         * sizeof(float));   //
    //     dscratch = (float*)snrt_l1alloc(T * T         * sizeof(float));
    //     mask     = (float*)snrt_l1alloc(3*16          * sizeof(float));
    //     inp      = (float*)snrt_l1alloc(B * T  * 3*C  * sizeof(float));
    //     dinp     = (float*)snrt_l1alloc(B * T  * 3*C  * sizeof(float));
    //     dpreatt  = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     att      = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     datt     = (float*)snrt_l1alloc(B * NH * T * T * sizeof(float));
    //     dout     = (float*)snrt_l1alloc(B * T  * C     * sizeof(float));
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
    // for (int i = 0; i < T; i++){
    //     for (int j = 0; j < T; j++){
    //         if(j > i){
    //         // if(j < i){
    //             datt[i*T+j] = 0;
    //             att[i*T+j] = 0;
    //         }
    //     }
    // }
    // for (int i = 0; i < 3*16; i++){
    //     mask[i] = 1;
    // }
    // mask[0] = 0;
    // mask[16]   = 0;
    // mask[16+1] = 0;
    // mask[2*16]   = 0;
    // mask[2*16+1] = 0;
    // mask[2*16+2] = 0;
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // attention_backward_opt(dinp, dpreatt, datt, dout, inp, att, temp, dscratch, mask, B, T, C, NH, vl);
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(datt     , dattG   , B * NH * T * T);
    //     printf("CHECK RESULTS2\n");
    //     check_result(dpreatt  , dpreattG, B * NH * T * T);
    //     printf("CHECK RESULTS3\n");
    //     check_result(dinp     , dinpG   , B *  T * 3*C);
    // }
    // ---------------------------------------------------------

    //--------------------------------------------------------- gelu_forward
    // int N = B*T*C;
    // printf("N = %d\n",N);
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

    //--------------------------------------------------------- gelu_forward_opt
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
    // for (int i = 0; i < 1; i++){
    //     gelu_forward_opt(out, inp, B*T*C, vl);
    // }
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
    // //---------------------------------------------------------

    //--------------------------------------------------------- gelu_backward_opt
    // int N = B*T*C;
    // if (cid == 0) {
    //     temp    = (float*)snrt_l1alloc(C            * sizeof(float));
    //     temp1   = (float*)snrt_l1alloc(C            * sizeof(float));
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
    // // for (int i = 0; i < 8; i++){
    //     gelu_backward_opt(dinp, inp, dout, temp, temp1, B*T*C, vl);        
    // // }
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
    // for (int i = 0; i < 32; i++){
    //     residual_forward(out, inp, dinp, B*T*C, vl);
    // }
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
    // for (int i = 0; i < 32; i++){
    //     residual_backward(inp, dinp, dout, B*T*C, vl);
    // }
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
    // softmax_forward(out, inp, B, T, V, Vp, vl);
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
    // for (int i = 0; i < 16; i++)
    // {
    //     crossentropy_softmax_backward(dlogits, dlosses, probs, targets, B, T, V, Vp, vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(dlogits      , dlogitsG    , B * T * Vp);   
    // }
    //---------------------------------------------------------


    // --------------------------------------------------------- relu_forward
    // int N = 4096;
    // B = 1;
    // T = 1;
    // C = 4096;
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // for (int i = 0; i < 1; i++){
    //     relu_forward(out, inp, N, vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    // ---------------------------------------------------------


    // --------------------------------------------------------- relu_backward
    // int N = 128;
    // B = 1;
    // T = 1;
    // C = 128;
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dinp      = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     temp      = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     dout      = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     snrt_dma_start_1d(inp     , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dinp    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(dout    , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();
    // start_kernel();    
    // for (int i = 0; i < 20; i++){
    //     relu_backward(dinp, inp, dout, temp, N, vl);
    // }
    // stop_kernel();
    // if(cid == 0){
    //     printf("CHECK RESULTS1\n");
    //     check_result(out      , outG    , B * T * C);   
    // }
    // ---------------------------------------------------------

    //--------------------------------------------------------- fast_exp
    int N = B*T*C;
    if (cid == 0) {
        inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
        out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
        snrt_dma_start_1d(inp    , data1_dram, B * T * C * sizeof(float));
        snrt_dma_wait_all();
    }
    snrt_cluster_hw_barrier();
    timer = benchmark_get_cycle();
    start_kernel();    
    int avl = N;
    do{
        asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
        avl -= vl;
        asm volatile("vle32.v v24, (%0)" :: "r"(inp));
        inp +=vl;
        asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
        asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));
        asm volatile("vfcvt.rtz.xu.f.v v24, v24");
        asm volatile("vse32.v v24, (%0)" :: "r"(out));
        out += vl;
    } while (avl > 0);
    // do{
    //     asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
    //     asm volatile("vle32.v v24, (%0)" :: "r"(inp));
    //     asm volatile("vfmul.vf v24, v24, %[A]" ::[A] "f"(12102203.0f));
    //     asm volatile("vfadd.vf v24, v24, %[A]" ::[A] "f"(1064866805.0f));
    //     asm volatile("vfcvt.rtz.xu.f.v v24, v24");
    //     asm volatile("vse32.v v24, (%0)" :: "r"(out));
    //     out += vl;
    //     avl -= vl;
    //     inp +=vl;
    // } while (avl > 0);    
    // for (int i = 0; i < N; i++){
    //     float v = inp[i];              // vle32.v      : load float
    //     v = v * 12102203.0f;           // vfmul.vf     : scale
    //     v = v + 1064866805.0f;         // vfadd.vf     : bias
    //     out[i] = (uint32_t)v;
    // }
    
    stop_kernel();
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

    //--------------------------------------------------------- DMA TEST
    // if (cid == 0) {
    //     inp       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    //     out       = (float*)snrt_l1alloc(B * T * C * sizeof(float));
    // }
    // snrt_cluster_hw_barrier();
    // timer = benchmark_get_cycle();
    // start_kernel();
    // for (int i = 0; i < 16; i++)
    // {
    //     snrt_dma_start_1d(inp           , data1_dram, B * T * C * sizeof(float));
    //     snrt_dma_start_1d(data1_dram    , inp       , B * T * C * sizeof(float));

    // }
    // snrt_dma_wait_all();
    // stop_kernel();
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
