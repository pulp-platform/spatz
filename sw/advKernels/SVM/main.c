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

// #include "data/data-bill.h"
// #include "Golden/gold-bill.h"

#include "data/data-bill-rbf.h"
#include "Golden/gold-bill-rbf.h"

// #include "data/data-cancer.h"
// #include "Golden/gold-cancer.h"

// #include "data/data-cancer-rbf.h"
// #include "Golden/gold-cancer-rbf.h"


// #include "data/data.h"
// #include "data/data2.h"
// #include "Golden/gold.h"
#include "plp_exp.h"


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

float *data_model;
int   *predictions;
float *bias;
float *sv_coef;
float *x_ref;

float *temp;
float *temp2;



volatile typedef struct svm_model
{
	int SVM_TYPE;
	int KERNEL_TYPE;
	int DEGREE_SVM;
	float GAMMA1;
	int COEF0_SVM;
	int SVS;
	int RHO_SIZE;
	int COEF_DIM;
	int F_DIM;
	int N_CLASS;
	int N_DEC_VALUES; 
	int *num_SVs;
	int *LABEL;
} svm_model;




void check_result(float *x, float *check, int r){    
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

void check_result_int(int *x, int *check, int r){    
  float diff = 0.0f;
  int err = 0;

  for(int i = 0; i < r; i++){  
      diff = fabs(x[i] - check[i]);
      if(diff>THRESHOLD){
          err++;
          printf("Error at index %d:\t expected %d\t real %d\t error %f\n", i, check[i], x[i], diff);
      }
  }

  if(err != 0)
      printf("TEST FAILED with %d errors!!\n", err);
  else
      printf("TEST PASSED!!\n");    
}
// int fp_check_32(float val, const float golden) {
//     float diff = val - golden;
//     if (diff < 0)
//         diff = -diff;

//     float eps = 0.01f * golden;
//     if (eps < 0)
//         eps = -eps;

//     return ((diff > eps) && (diff > 1e-5));
// }

// int check_data32(float *a,float *g,int start, int end){
//     int errors = 0;
//     for (int i = start; i < end; ++i){
//         if (fp_check_32((((float *) a)[i]), (((float *) g)[i-start]))) {
//             errors ++;
//             printf("error detected at index %d. Value = %x. Golden = %x \n", i, ((uint32_t *) a)[i], ((uint32_t *) g)[i-start]);
//         }
//     }
//     return errors;
// }

// FDIM <= 128
// V0 - V7  Data
// V8 - V15 SV_coef
// V16- V23 MulRes
// V29      REDSUM RES
// IN BILL THERE IS AN ISSUE WITH INDEX 7 AND 70 COMES FROM TRANSLIB
void plp_SVM_linear(svm_model model, float *data_model, int *Pred, float *sv_coef, float *bias, unsigned int vl){

  int l = model.SVS;
  int f_dim = model.F_DIM;
  int nr_class = model.N_CLASS;

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  float temp;
  for (int i = 0; i < l; i++) {
    for (int j = 0; j < nr_class-1; j++) {
      asm volatile("vle32.v v0, (%0)" :: "r"(data_model+i*f_dim));
      asm volatile("vle32.v v8, (%0)" :: "r"(sv_coef+j*(nr_class-1)));
      asm volatile("vfmul.vv v16, v8, v0");
      
      asm volatile("vfredsum.vs v29, v16, v31");      
      asm volatile("vfmv.f.s %0, v29" : "=f"(temp));

      if (temp + bias[0] >= 0)
          Pred[i*(nr_class-1)+j] = 1 ;
      else
          Pred[i*(nr_class-1)+j] = 0 ;
    }
  }
}

void plp_SVM_linear_v2(svm_model model, float *data_model, int *Pred, float *sv_coef, float *bias, unsigned int vl){

  int l = model.SVS;
  int f_dim = model.F_DIM;
  int nr_class = model.N_CLASS;

  // asm volatile("vfmv.s.f v31, %0" : "=f"(bias[0]));//BUG

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  float temp;
  for (int i = 0; i < l; i++) {
    asm volatile("vle32.v v0, (%0)" :: "r"(data_model+i*f_dim));
    asm volatile("vle32.v v8, (%0)" :: "r"(sv_coef));
    for (int j = 0; j < nr_class-1; j++) {
      asm volatile("vfmul.vv v16, v8, v0");      
      asm volatile("vfredsum.vs v29, v16, v31");
      asm volatile("vle32.v v8, (%0)" :: "r"(sv_coef+(j+1)*(nr_class-1)));

      asm volatile("vfmv.f.s %0, v29" : "=f"(temp));

      if (temp + bias[0]>= 0)
          Pred[i*(nr_class-1)+j] = 1 ;
      else
          Pred[i*(nr_class-1)+j] = 0 ;
    }
  }
}


//ASSUME N-CLASS=2
void plp_SVM_linear_v3(svm_model model, float *data_model, int *Pred, float *sv_coef, float *bias, unsigned int vl){

  int l = model.SVS;
  int f_dim = model.F_DIM;
  int nr_class = model.N_CLASS;
  
  // asm volatile("vfmv.s.f v31, %0" : "=f"(bias[0]));//BUG

  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  float temp;
  asm volatile("vle32.v v8, (%0)" :: "r"(sv_coef));
  asm volatile("vle32.v v0, (%0)" :: "r"(data_model));
  for (int i = 0; i < l; i++) {
    asm volatile("vfmul.vv v16, v8, v0");
    asm volatile("vfredsum.vs v29, v16, v31");
    asm volatile("vle32.v v0, (%0)" :: "r"(data_model+(i+1)*f_dim));

    asm volatile("vfmv.f.s %0, v29" : "=f"(temp));

      // if (temp + bias[0]>= 0)
      if (temp>= 0)
          Pred[i] = 1 ;
      else
          Pred[i] = 0 ;
  }
}

//ASSUME N-CLASS=2
void plp_SVM_linear_v4(svm_model model, float *data_model, int *Pred, float *sv_coef, float *bias, unsigned int vl){

  int l = model.SVS;
  int f_dim = model.F_DIM;
  int nr_class = model.N_CLASS;
  
  // // asm volatile("vfmv.s.f v31, %0" : "=f"(bias[0]));//BUG

  // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  // float temp;
  // asm volatile("vle32.v v8, (%0)" :: "r"(sv_coef));
  // asm volatile("vle32.v v0, (%0)" :: "r"(data_model));
  // for (int i = 0; i < l; i++) {
  //   asm volatile("vfmul.vv v16, v8, v0");
  //   asm volatile("vfredsum.vs v29, v16, v31");
  //   asm volatile("vle32.v v0, (%0)" :: "r"(data_model+(i+1)*f_dim));

  //   asm volatile("vfmv.f.s %0, v29" : "=f"(temp));

  //     // if (temp + bias[0]>= 0)
  //     if (temp>= 0)
  //         Pred[i] = 1 ;
  //     else
  //         Pred[i] = 0 ;
  // }

    int itr = 0;
    int num_elements = l;
    int VLMAX = 128;
    int stridesize = f_dim*4;//fdmi*4 byte
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
    while(itr*VLMAX + VLMAX <= num_elements){
      for (int i = 0; i < f_dim; i++){
        // asm volatile("vle32.v v0, (%0)" :: "r"(data_model+itr*VLMAX+i));
        asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+i), "r"(stridesize));

        float sv = sv_coef[i];
        // snrt_cluster_hw_barrier();
        if(i == 0){
          asm volatile("vfmul.vf  v8, v0, %0" ::"f"(sv));
        }else{
          asm volatile("vfmacc.vf v8, %0, v0" ::"f"(sv));
        }
        // asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
        // for(int i=0; i < f_dim; i++){
        //   printf("sum[%d] = %f\n",i,temp2[i]);
        // }        
      }      
      asm volatile("vfadd.vf  v8, v8, %0" ::"f"(bias[0]));
      asm volatile("vse32.v v8, (%0)" :: "r"(temp+itr*VLMAX));
      itr++;
    }
    if(itr*VLMAX < num_elements){
      asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
      for (int i = 0; i < f_dim; i++){
        // asm volatile("vle32.v v0, (%0)" :: "r"(data_model+itr*VLMAX+i));
        asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+i), "r"(stridesize));
        
        // asm volatile("vse32.v v0, (%0)" :: "r"(temp2));
        // for(int i=0; i < f_dim; i++){
        //   printf("inp[%d] = %f\n",i,temp2[i]);
        // }
        
        float sv = sv_coef[i];
        // snrt_cluster_hw_barrier();
        if(i == 0){
          asm volatile("vfmul.vf  v8, v0, %0" ::"f"(sv));
        }else{
          asm volatile("vfmacc.vf v8, %0, v0" ::"f"(sv));
        }
        // asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
        // for(int i=0; i < f_dim; i++){
        //   printf("mul[%d] = %f\n",i,temp2[i]);
        // }
      }
      asm volatile("vfadd.vf  v8, v8, %0" ::"f"(bias[0]));
      asm volatile("vse32.v v8, (%0)" :: "r"(temp+itr*VLMAX));

      // asm volatile("vand.vi v8, v8, 0x80000000");
      // asm volatile("vse32.v v8, (%0)" :: "r"(Pred+itr*VLMAX));
    }
    for (int i = 0; i < l; i++){
      if (temp[i]>= 0)
          Pred[i] = 1 ;
      else
          Pred[i] = 0 ;
    }
}




// FDIM <= 128
// V0 - V7  DATA_MODEL
// V8 - V15 X_REF
// V8 - V15 MUL RES
// V29      REDSUM RES
static float plp_rbf(float gamma, int f_dim){

  float sum = 0.0f;
  float d;
  float a, b; 

  asm volatile("vfsub.vv v8, v8, v0");
  asm volatile("vfmul.vv v8, v8, v8");

  asm volatile("vfredsum.vs v29, v8, v31");      
  asm volatile("vfmv.f.s %0, v29" : "=f"(sum));
  // printf("sum = %f\texp = %f\n",sum,exp32(-gamma*sum));
  return exp32(-gamma*sum);
}

// FDIM <= 128
// V0 - V7  DATA_MODEL
// V8 - V15 X_REF
// V8 - V15 MUL RES / PLP_RBF RES
// V16- V23 SV_COEF / VFMUL.VF RES
// V29      REDSUM RES
void  plp_SVM_RBF(svm_model model, float *data_model,float *sv_coef, float *bias, float *x_ref,int *Pred,unsigned int vl){
 
  int j;
  int i = 0;
  float gamma1 = model.GAMMA1;
  int l = model.SVS;
  int coef_dim = model.COEF_DIM;
  int f_dim = model.F_DIM;
                
  // for(int i=0; i < l; i++){
  //         float temp,inter;
  //         inter =0;
  //         ptrx = &data_model[i*f_dim];
  //         for (int k=0; k < coef_dim; k++){
  //           ptrs = &x_ref[k*f_dim];
  //           temp = plp_rbf(ptrx, ptrs, gamma1, f_dim);
  //           temp = temp * sv_coef[k];
  //           inter = inter + temp ;
  //         }
  //         if (inter + bias[0] >= 0)
  //                 Pred[i] = 1 ;
  //         else
  //                 Pred[i] = 0 ;        
  // }
  
  asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  // for(int i=0; i < l; i++){
  for(int i=0; i < l; i++){
    float inter;
    asm volatile("vle32.v v0, (%0)" :: "r"(data_model+i*f_dim));

    for (int k=0; k < coef_dim; k++){
      asm volatile("vle32.v v8, (%0)" :: "r"(x_ref+k*f_dim));
      temp[k] = plp_rbf(gamma1, f_dim);
      // if(i==6)
        printf("temp[%d]=%f\n",k,temp[k]);
    }
    inter=0;
    // asm volatile("vfmv.s.f v30, %0" : "=f"(inter));
    int idx=0;
    while(idx+128 < coef_dim){
      asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v8 , (%0)" :: "r"(temp+idx));
      asm volatile("vle32.v v16, (%0)" :: "r"(sv_coef+idx));
      asm volatile("vfmul.vv v16, v16, v8");
      asm volatile("vfredsum.vs v29, v16, v30");
      asm volatile("vfmv.f.s %0, v29" : "=f"(inter));
      // asm volatile("vfmv.s.f v30, %0" : "=f"(inter));

      idx+=128;
    }
    if(coef_dim-idx > 0){
      asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(coef_dim-idx));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v8 , (%0)" :: "r"(temp+idx));
      
      // if(i == 6){
      //   asm volatile("vse32.v v8, (%0)" :: "r"(temp2));
      //   for(int i=0; i < coef_dim; i++){
      //     // for(int i=0; i < coef_dim-idx; i++){
      //     printf("inp[%d] = %f\n",i,temp2[i]);
      //   }
      // }

      asm volatile("vle32.v v16, (%0)" :: "r"(sv_coef+idx));
      
      // if(i == 6){
      //   asm volatile("vse32.v v16, (%0)" :: "r"(temp2));
      //   for(int i=0; i < coef_dim; i++){
      //     // for(int i=0; i < coef_dim-idx; i++){
      //     printf("coef[%d] = %f\n",i,temp2[i]);
      //   }
      // }

      asm volatile("vfmul.vv v16, v16, v8");
      // if(i == 0){
      //   asm volatile("vse32.v v16, (%0)" :: "r"(temp2));
      //   for(int i=0; i < coef_dim; i++){
      //     // for(int i=0; i < coef_dim-idx; i++){
      //     printf("mul res[%d] = %f\n",i,temp2[i]);
      //   }
      // }
      asm volatile("vfredsum.vs v29, v16, v30");
      asm volatile("vfmv.f.s %0, v29" : "=f"(inter));
      // asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
    }
    // if(i==6 || i==12 || i==27)
    printf("inter[%d]=%f\n",i,inter);
    if (inter + bias[0] >= 0)
      Pred[i] = 1 ;
    else
      Pred[i] = 0 ;        
    
    // printf("Pred[%d] = %d\n",i,Pred[i]);
  }
  // for(int i=0; i<model.N_DEC_VALUES; i++){
  //   printf("Pred[%d] = %d\n",i,Pred[i]);
  // }
}


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


void  plp_SVM_RBF_v2(svm_model model, float *data_model,float *sv_coef, float *bias, float *x_ref,int *Pred,unsigned int vl){
 
  int j;
  int i = 0;
  float gamma1 = model.GAMMA1;
  int l = model.SVS;
  int coef_dim = model.COEF_DIM;
  int f_dim = model.F_DIM;

  float inter;
  float temp3;
  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
  asm volatile("vfsub.vv v24, v24, v24");

  for(int i=0; i < l; i++){
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
    asm volatile("vle32.v v16, (%0)" :: "r"(data_model+i*f_dim));
    inter = 0;

    for (int k=0; k < coef_dim; k++){
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v8, (%0)" :: "r"(x_ref+k*f_dim));
      asm volatile("vfsub.vv v0, v16, v8");
      asm volatile("vfmul.vv v0, v0, v0");
      asm volatile("vfredsum.vs v8, v0, v31");      
      asm volatile("vfmul.vf v0, v8, %[A]" ::[A] "f"(-gamma1));
      fast_exp(temp, temp, 0, 0, 1, 1, vl);
      asm volatile("vfmv.f.s %0, v0" : "=f"(temp3));
      inter = inter + temp3 * sv_coef[k];
    }
    if (inter + bias[0] >= 0)
      Pred[i] = 1 ;
    else
      Pred[i] = 0 ;        
  }
}

void  plp_SVM_RBF_v3(svm_model model, float *data_model,float *sv_coef, float *bias, float *x_ref,int *Pred,unsigned int vl){
 
  int j;
  int i = 0;
  float gamma1 = model.GAMMA1;
  int l = model.SVS;
  int coef_dim = model.COEF_DIM;
  int f_dim = model.F_DIM;

  int itr = 0;
  int num_elements = l;
  int VLMAX = 128;
  int stridesize = f_dim*4;//fdmi*4 byte
  asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(VLMAX));
  asm volatile("vfsub.vv v16, v16, v16");

  while(itr*VLMAX + VLMAX <= num_elements){
    for (int i = 0; i < coef_dim; i++){
      float sv = sv_coef[i];
      for (int j = 0; j < f_dim; j++){
        asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+j), "r"(stridesize));
        float x = x_ref[i*f_dim+j];
        asm volatile("vfsub.vf  v8, v0, %0" ::"f"(x));
        if(i == 0){
          asm volatile("vfmul.vv  v16, v8, v8");
        }else{
          asm volatile("vfmacc.vv v16, v8, v8");
        }
      }

      asm volatile("vfmul.vf  v0, v16, %0" ::"f"(-gamma1));

      asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
      asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
      asm volatile("vfcvt.rtz.xu.f.v v0, v0");

      asm volatile("vfmacc.vf v24, %0, v0" ::"f"(sv));
    }

    asm volatile("vfadd.vf  v24, v24, %0" ::"f"(bias[0]));
    asm volatile("vse32.v v24, (%0)" :: "r"(temp+itr*VLMAX));
    itr++;
  }
  if(itr*VLMAX < num_elements){
    asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
    for (int i = 0; i < coef_dim; i++){
      float sv = sv_coef[i];
      for (int j = 0; j < f_dim; j++){
        asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+j), "r"(stridesize));
        
        float x = x_ref[i*f_dim+j];
        
        asm volatile("vfsub.vf  v8, v0, %0" ::"f"(x));
        if(j == 0){
          asm volatile("vfmul.vv  v16, v8, v8");
        }else{
          asm volatile("vfmacc.vv v16, v8, v8");
        }
      }

      asm volatile("vfmul.vf  v0, v16, %0" ::"f"(-gamma1));

      asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
      asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
      asm volatile("vfcvt.rtz.xu.f.v v0, v0");

      asm volatile("vfmacc.vf v24, %0, v0" ::"f"(sv));
    }

    asm volatile("vfadd.vf  v24, v24, %0" ::"f"(bias[0]));
    asm volatile("vse32.v v24, (%0)" :: "r"(temp+itr*VLMAX));
  }
  for (int i = 0; i < l; i++){
    if (temp[i]>= 0)
        Pred[i] = 1 ;
    else
        Pred[i] = 0 ;
  }
}


// void  plp_SVM_RBF_v4(svm_model model, float *data_model,float *sv_coef, float *bias, float *x_ref,int *Pred,unsigned int vl){
//   int j;
//   int i = 0;
//   float gamma1 = model.GAMMA1;
//   int l = model.SVS;
//   int coef_dim = model.COEF_DIM;
//   int f_dim = model.F_DIM;
//   int itr = 0;
//   int num_elements = l;
//   int VLMAX = 64;
//   int stridesize = f_dim*4;//fdmi*4 byte
//   asm volatile("vsetvli  %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(VLMAX));
//   while(itr*VLMAX + VLMAX <= num_elements){
//     for (int i = 0; i < coef_dim; i+=3){
//       float sv0 = sv_coef[i];
//       float sv1 = sv_coef[i];
//       float sv2 = sv_coef[i];
//       for (int j = 0; j < f_dim; j++){
//         asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+j), "r"(stridesize));
//         float x0 = x_ref[i*f_dim+j];
//         float x1 = x_ref[(i+1)*f_dim+j];
//         float x2 = x_ref[(i+2)*f_dim+j];
//         asm volatile("vfsub.vf  v4 , v0, %0" ::"f"(x0));
//         asm volatile("vfsub.vf  v16, v0, %0" ::"f"(x1));
//         asm volatile("vfsub.vf  v24, v0, %0" ::"f"(x2));
//         if(i == 0){
//           asm volatile("vfmul.vv  v8 , v4 , v4 ");
//           asm volatile("vfmul.vv  v20, v16, v16");
//           asm volatile("vfmul.vv  v28, v24, v24");
//         }else{
//           asm volatile("vfmacc.vv v8 , v4 , v4 ");
//           asm volatile("vfmacc.vv v20, v16, v16");
//           asm volatile("vfmacc.vv v28, v24, v24");
//         }
//       }
//
//       asm volatile("vfmul.vf  v8 , v8 , %0" ::"f"(-gamma1));
//       asm volatile("vfmul.vf  v20, v20, %0" ::"f"(-gamma1));
//       asm volatile("vfmul.vf  v28, v28, %0" ::"f"(-gamma1));
//
//       asm volatile("vfmul.vf v8, v8, %[A]" ::[A] "f"(12102203.0f));
//       asm volatile("vfadd.vf v8, v8, %[A]" ::[A] "f"(1064866805.0f));
//       asm volatile("vfcvt.rtz.xu.f.v v8, v0");
//       asm volatile("vfmacc.vf v12, %0, v8" ::"f"(sv0));
//
//
//       asm volatile("vfmul.vf v20, v20, %[A]" ::[A] "f"(12102203.0f));
//       asm volatile("vfadd.vf v20, v20, %[A]" ::[A] "f"(1064866805.0f));
//       asm volatile("vfcvt.rtz.xu.f.v v20, v20");
//       asm volatile("vfmacc.vf v12, %0, v20" ::"f"(sv1));
//
//       asm volatile("vfmul.vf v28, v28, %[A]" ::[A] "f"(12102203.0f));
//       asm volatile("vfadd.vf v28, v28, %[A]" ::[A] "f"(1064866805.0f));
//       asm volatile("vfcvt.rtz.xu.f.v v28, v20");
//       asm volatile("vfmacc.vf v12, %0, v28" ::"f"(sv2));
//
//     }
//
//     asm volatile("vfadd.vf  v12, v12, %0" ::"f"(bias[0]));
//     asm volatile("vse32.v v12, (%0)" :: "r"(temp+itr*VLMAX));
//     itr++;
//   }
//   if(itr*VLMAX < num_elements){
//     asm volatile("vsetvli  %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(num_elements - itr*VLMAX));
//     for (int i = 0; i < coef_dim; i++){
//       float sv = sv_coef[i];
//       for (int j = 0; j < f_dim; j++){
//         asm volatile("vlse32.v v0, (%0), %1" :: "r"(data_model+itr*VLMAX+j), "r"(stridesize));
//        
//         float x = x_ref[i*f_dim+j];
//        
//         asm volatile("vfsub.vf  v8, v0, %0" ::"f"(x));
//         if(j == 0){
//           asm volatile("vfmul.vv  v16, v8, v8");
//         }else{
//           asm volatile("vfmacc.vv v16, v8, v8");
//         }
//       }
//
//       asm volatile("vfmul.vf  v0, v16, %0" ::"f"(-gamma1));
//
//       asm volatile("vfmul.vf v0, v0, %[A]" ::[A] "f"(12102203.0f));
//       asm volatile("vfadd.vf v0, v0, %[A]" ::[A] "f"(1064866805.0f));
//       asm volatile("vfcvt.rtz.xu.f.v v0, v0");
//
//       asm volatile("vfmacc.vf v24, %0, v0" ::"f"(sv));
//     }
//
//     asm volatile("vfadd.vf  v24, v24, %0" ::"f"(bias[0]));
//     asm volatile("vse32.v v24, (%0)" :: "r"(temp+itr*VLMAX));
//   }
//   for (int i = 0; i < l; i++){
//     if (temp[i]>= 0)
//         Pred[i] = 1 ;
//     else
//         Pred[i] = 0 ;
//   }
// }







int main() {

    volatile int errors = 0;

    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;

    printf("start_addr: %x \n", snrt_cluster_memory().end);

    int reorder_enable = 1;

    svm_model model_par;

    model_par.KERNEL_TYPE = KERNEL_TYPE_;
    model_par.GAMMA1  = GAMMA1_;
    model_par.SVS     = SVS_;
    model_par.COEF_DIM  = COEF_DIM_;
    model_par.F_DIM     = F_DIM_;
    model_par.N_CLASS   = N_CLASS_;
    model_par.N_DEC_VALUES =  N_DEC_VALUES_; 
    int kernel_type = model_par.KERNEL_TYPE;


    // Core 0 programs DMA transfer of inputs
    if (cid == 0) {
        // temp2 = (int *)snrt_l1alloc(1 * sizeof(int));
        // for (int i = 0; i < N_CLUSTERS; i++){
        //     newClusters[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
        // }
        data_model = (float   *)snrt_l1alloc(SVS_*F_DIM_ * sizeof(float));
        snrt_dma_start_1d(data_model, data_model_dram, SVS_*F_DIM_ * sizeof(float));
        
        predictions = (int   *)snrt_l1alloc(N_DEC_VALUES_ * sizeof(int));

        bias = (float   *)snrt_l1alloc(sizeof(float));
        snrt_dma_start_1d(bias, bias_dram, sizeof(float));

        sv_coef = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));
        snrt_dma_start_1d(sv_coef, sv_coef_dram, COEF_DIM_*F_DIM_ * sizeof(float));
        if(KERNEL_TYPE_ == 2){
          x_ref = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));
          snrt_dma_start_1d(x_ref, X_ref_dram, COEF_DIM_*F_DIM_ * sizeof(float));
        }
        // temp = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));
        temp = (float   *)snrt_l1alloc(SVS_ * sizeof(float));
        temp2 = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));
    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    // printf("plp_SVM Start\n");
    switch(kernel_type)
    {
      case 0:
          //  printf("plp_SVM_linear\n");
          //  plp_SVM_linear (model_par,data_model,predictions,sv_coef, bias,vl);
          //  plp_SVM_linear_v2 (model_par,data_model,predictions,sv_coef, bias,vl);
          //  plp_SVM_linear_v3 (model_par,data_model,predictions,sv_coef, bias,vl);
           plp_SVM_linear_v4 (model_par,data_model,predictions,sv_coef, bias,vl);
           break;
      case 2:
            // printf("plp_SVM_RBF\n");      
            // plp_SVM_RBF(model_par,data_model,sv_coef, bias, x_ref,predictions,vl);
            // plp_SVM_RBF_v2(model_par,data_model,sv_coef, bias, x_ref,predictions,vl);
            plp_SVM_RBF_v3(model_par,data_model,sv_coef, bias, x_ref,predictions,vl);
            // plp_SVM_RBF_v4(model_par,data_model,sv_coef, bias, x_ref,predictions,vl);
            break;
          }    
    // printf("plp_SVM Done\n");
    // End dump
    stop_kernel();

    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    snrt_cluster_hw_barrier();
    // Display runtime
    if (cid == 0) {
        printf("The execution took %u cycles.\n", timer);
    }

    check_result_int(predictions,result,N_DEC_VALUES_);




    snrt_cluster_hw_barrier();
    return errors;

}
