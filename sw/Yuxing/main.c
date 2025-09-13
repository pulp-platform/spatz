#include "benchmark/benchmark.h"
#include <snrt.h>
#include <stdio.h>
#include "printf.h"

// #include DATAHEADER

float *re_u_i;
float *re_l_i;
float *re_u_o;
float *re_l_o;
float *re_add_o;

int global_var;

int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  // Reset timer
  unsigned int timer = (unsigned int)-1;


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();


  printf("HI!!!, cid = %u\n", cid);


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    printf("Num of Cores: %u\n", num_cores);

    re_u_i = (float *)snrt_l1alloc(12 * sizeof(float));
    re_l_i = (float *)snrt_l1alloc(12 * sizeof(float));
    re_u_o = (float *)snrt_l1alloc(6 * sizeof(float));
    re_l_o = (float *)snrt_l1alloc(6 * sizeof(float));
    re_add_o = (float *)snrt_l1alloc(6 * sizeof(float));
  }

  snrt_cluster_hw_barrier();


  if (cid == 0) {

    float *u_i;
    float *l_i;
    float *u_o;
    float *l_o;
    float *add_o;


    u_i = re_u_i;
    l_i = re_l_i;
    u_o = re_u_o;
    l_o = re_l_o;
    add_o = re_add_o;

    size_t avl = 6;
    size_t vl;

    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));

  


    for(int i = 0; i < 12; ++i) {
      re_u_i[i] = (float) (i+1);
      re_l_i[i] = (float) (i*i);

      // printf("\t u_i[%d] = %f\n", i, u_i[i]);
      // printf("\t l_i[%d] = %f\n", i, l_i[i]);
      // printf("\t re_u_i[%d] = %f\n", i, re_u_i[i]);
      // printf("\t re_l_i[%d] = %f\n", i, re_l_i[i]);
    }


    // printf("vl = %u (cid = %u)\n", vl, cid);
    // printf("avl = %u (cid = %u)\n", avl, cid);

    int stridesize = 2 * sizeof(float);

    // Load a portion of the vector
    // asm volatile("vle32.v v0, (%0);" ::"r"(u_i)); // v0: Re upper wing
    // asm volatile("vle32.v v4, (%0);" ::"r"(l_i)); // v4: Re lower wing

    asm volatile("vlse32.v v0, (%0), %1;" ::"r"(re_u_i), "r"(stridesize)); // v0: Re upper wing

    // printf("WAIT FOR LOAD\n");

    asm volatile("vlse32.v v4, (%0), %1;" ::"r"(re_l_i), "r"(stridesize)); // v4: Re lower wing

    // snrt_cluster_hw_barrier();

    // add
    asm volatile("vfadd.vv v8, v0, v4"); 
    // asm volatile("vfmul.vv v8, v0, v4"); 



    // Store vector
    asm volatile("vse32.v v8, (%0)" ::"r"(add_o));

    asm volatile("vse32.v v0, (%0)" ::"r"(re_u_o));
    asm volatile("vse32.v v4, (%0)" ::"r"(re_l_o));

    // asm volatile("vse32.v v16, (%0)" ::"r"(re_add_o));

    


    for(int i = 0; i < 6; ++i) {
      printf("u_o[%d] = %f\n", i, re_u_o[i]);
      printf("l_o[%d] = %f\n", i, re_l_o[i]);
      printf("add_o[%d] = %f, expected results = %f\n", i, re_add_o[i], re_u_o[i] + re_l_o[i]);
    }
  }
  

  // Wait for core 0 to finish displaying results
  snrt_cluster_hw_barrier();

  return 0;
}