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

// #include "data/data.h"
#include "data/data2.h"
#include "Golden/gold.h"
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
void plp_SVM_linear(svm_model model, float *data_model, int *Pred, float *sv_coef, float *bias, unsigned int vl){

  int l = model.SVS;
  int f_dim = model.F_DIM;
  int nr_class = model.N_CLASS;

  asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
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
      //   printf("temp[%d]=%f\n",k,temp[k]);
    }
    inter=0;
    asm volatile("vfmv.s.f v30, %0" : "=f"(inter));
    int idx=0;
    while(idx+128 < coef_dim){
      asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(128));//VLMAX = LMUL*VLEN/SEW =128
      asm volatile("vle32.v v8 , (%0)" :: "r"(temp+idx));
      asm volatile("vle32.v v16, (%0)" :: "r"(sv_coef+idx));
      asm volatile("vfmul.vv v16, v16, v8");
      asm volatile("vfredsum.vs v29, v16, v30");
      asm volatile("vfmv.f.s %0, v29" : "=f"(inter));
      asm volatile("vfmv.s.f v30, %0" : "=f"(inter));

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
      // if(i == 6){
      //   asm volatile("vse32.v v16, (%0)" :: "r"(temp2));
      //   for(int i=0; i < coef_dim; i++){
      //     // for(int i=0; i < coef_dim-idx; i++){
      //     printf("mul res[%d] = %f\n",i,temp2[i]);
      //   }
      // }
      asm volatile("vfredsum.vs v29, v16, v30");
      asm volatile("vfmv.f.s %0, v29" : "=f"(inter));
      asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(f_dim));//VLMAX = LMUL*VLEN/SEW =128
    }
    // if(i==6 || i==12 || i==27)
    //   printf("inter[%d]=%f\n",i,inter);
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
        temp = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));
        temp2 = (float   *)snrt_l1alloc(COEF_DIM_*F_DIM_ * sizeof(float));





        // membership     = (int   *)snrt_l1alloc(N_OBJECTS * sizeof(int));
        // for (int i = 0; i < N_CLUSTERS; i++){
        //     clusters[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
        //     snrt_dma_start_1d(clusters[i], clusters_dram, N_COORDS* sizeof(float));
        // }
        // temp     = (float   *)snrt_l1alloc(N_COORDS * sizeof(float));

        // for (int i = 0; i < N_OBJECTS; i++){
        //     objects[i] = (float *)snrt_l1alloc(N_COORDS * sizeof(float));
        //     snrt_dma_start_1d(objects[i], objects_dram[i], N_COORDS* sizeof(float));
        // }


    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();
    printf("plp_SVM Start\n");
    // dwt_step (input, output, 200, 111, vl);
    // dwt_step (input, output, 100, 46, vl);


    // int output_dim = DWT_LEN_OUT;
    // int level_dim = DWT_LEN;

    // for (int i = 0; i < LEVELS; i++)
    // {
    //   size_t input_dim = level_dim;
    //   level_dim = (level_dim + NC -1)/2;
    //   output_dim -= level_dim;
    //   printf("LEVEL=%d\tin_dim=%d\tout_dim=%d\n",i,input_dim,output_dim);
    // //   asm volatile("":::"memory");
    //   dwt_step (input, output, input_dim, output_dim,vl);
    // }
    switch(kernel_type)
    {
      case 0:
           printf("plp_SVM_linear\n");
           plp_SVM_linear (model_par,data_model,predictions,sv_coef, bias,vl);
           break;
      case 2:
            printf("plp_SVM_RBF\n");      
            plp_SVM_RBF(model_par,data_model,sv_coef, bias, x_ref,predictions,vl);
            break;
          }    
    printf("plp_SVM Done\n");
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

    check_result_int(predictions,result,N_DEC_VALUES_);




    snrt_cluster_hw_barrier();
    return errors;

}
