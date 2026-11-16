#include <snrt.h>  
int nof_layers = 4;
float *input_re;
float *port_weights;
extern unsigned int timer;

void precoding_v32b(float *input_re_in_dram, float *port_weights_in_dram, uint16_t *port_re) {
  uint32_t cid = snrt_cluster_core_idx();
  uint32_t num_cores = snrt_cluster_core_num();
  unsigned int dim = (precoding_l.M / num_cores)/2;
  const unsigned int orig_avl = dim;
  unsigned int vl;
  unsigned int vl32, vl16;
  unsigned int stride = 8;
  unsigned int stride_half = stride / 2;
  unsigned int stride_b16 = 4;
    // Allocate the vectors
  if (cid == 0) {
    input_re = (float *)snrt_l1alloc(nof_layers * precoding_l.M * sizeof(float));
    port_weights = (float *)snrt_l1alloc(nof_layers * 2 * sizeof(float));
  }

  // Initialize the matrices
  if (cid == 0) {
    snrt_dma_start_1d(input_re, input_re_in_dram, nof_layers * precoding_l.M * sizeof(float));
    snrt_dma_start_1d(port_weights, port_weights_in_dram, nof_layers * 2 * sizeof(float));
    snrt_dma_wait_all();
  }

    // Wait for all cores to finish
  snrt_cluster_hw_barrier();
    // Calculate internal pointers
  float *input_re_int = input_re + 2 * dim * cid;
  uint16_t *port_re_int = port_re + 2 * dim * cid;
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  if (cid == 0)
    timer = benchmark_get_cycle();
  snrt_cluster_hw_barrier();
  asm volatile("csrwi 0x800, 0x1");
  switch (nof_layers){
    case 1: {
    unsigned int remaining = dim;
    float w0 = port_weights[0];
    float w1 = port_weights[1];
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vfmul.vf v24, v0, %0":: "f"(w0));
      asm volatile("vfmul.vf v28, v0, %0" :: "f"(w1));
      
      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vfmacc.vf v28, %0, v4":: "f"(w0));
      asm volatile("vfnmsac.vf v24, %0, v4" :: "f"(w1));

      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining)); 
      
      asm volatile("vfncvt.f.f.w v8, v24");
      asm volatile("vsse16.v v8, (%0), %1":: "r"(port_re_int),     "r"(stride_half)); 
      
      asm volatile("vfncvt.f.f.w v12, v28");
      asm volatile("vsse16.v v12, (%0), %1":: "r"(port_re_int + 1), "r"(stride_half));

      // Bump pointers
      input_re_int += 2*vl;
      port_re_int  += 2 * vl;
      remaining    -= vl;

    } while (remaining > 0);
    break;
    }

    case 2: {
          unsigned int remaining = dim;
          float w0 = port_weights[0];
          float w1 = port_weights[1];
          float w2 = port_weights[2];
          float w3 = port_weights[3];
          float *input_re_int2 = input_re_int + precoding_l.M;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vfmul.vf v24, v0, %0":: "f"(w0));
      asm volatile("vfmul.vf v28, v0, %0"  :: "f"(w1));

      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v4" :: "f"(w1));
      asm volatile("vfmacc.vf v28, %0, v4":: "f"(w0));

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int2), "r"(stride));
      asm volatile("vfmacc.vf v24, %0, v8"  :: "f"(w2)); 
      asm volatile("vfmacc.vf v28, %0, v8"  :: "f"(w3));

      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int2 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v12":: "f"(w3));
      asm volatile("vfmacc.vf v28, %0, v12" :: "f"(w2));

      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));

      asm volatile("vfncvt.f.f.w v8, v24");
      asm volatile("vsse16.v v8, (%0), %1":: "r"(port_re_int),     "r"(stride_half));

      asm volatile("vfncvt.f.f.w v12, v28");
      asm volatile("vsse16.v v12, (%0), %1":: "r"(port_re_int + 1), "r"(stride_half));

      // Bump pointers
      input_re_int  += 2*vl;
      input_re_int2 += 2*vl;
      port_re_int   += 2*vl;
      remaining     -= vl;
    } while (remaining > 0);
    break;
    }

    case 3: {
          unsigned int remaining = dim;
          float w0 = port_weights[0];
          float w1 = port_weights[1];
          float w2 = port_weights[2];
          float w3 = port_weights[3];
          float w4 = port_weights[4];
          float w5 = port_weights[5];
          float *input_re_int2 = input_re_int + precoding_l.M;
          float *input_re_int3 = input_re_int + 2 * precoding_l.M;
    do {
      // Set the vl
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      // Load chunk a and b
      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride)); 
      asm volatile("vfmul.vf v24, v0, %0":: "f"(w0));
      asm volatile("vfmul.vf v28, v0, %0"  :: "f"(w1));

      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v4" :: "f"(w1));
      asm volatile("vfmacc.vf v28, %0, v4":: "f"(w0));

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int2), "r"(stride)); 
      asm volatile("vfmacc.vf v24, %0, v8"  :: "f"(w2));
      asm volatile("vfmacc.vf v28, %0, v8"  :: "f"(w3));

      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int2 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v12":: "f"(w3));
      asm volatile("vfmacc.vf v28, %0, v12" :: "f"(w2));

      asm volatile("vlse32.v v16, (%0), %1":: "r"(input_re_int3 ), "r"(stride)); 
      asm volatile("vfmacc.vf v24, %0, v16"  :: "f"(w4));
      asm volatile("vfmacc.vf v28, %0, v16"  :: "f"(w5));

      asm volatile("vlse32.v v20, (%0), %1":: "r"(input_re_int3 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v20":: "f"(w5));
      asm volatile("vfmacc.vf v28, %0, v20" :: "f"(w4));

      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));
      asm volatile("vfncvt.f.f.w v8, v24");
      asm volatile("vsse16.v v8, (%0), %1":: "r"(port_re_int),     "r"(stride_half)); 
      asm volatile("vfncvt.f.f.w v12, v28");
      asm volatile("vsse16.v v12, (%0), %1":: "r"(port_re_int + 1), "r"(stride_half));
      // Bump pointers
      input_re_int  += 2*vl;
      input_re_int2 += 2*vl;
      input_re_int3 += 2*vl;
      port_re_int   += 2*vl;
      remaining     -= vl;
    } while (remaining > 0);
    break;
    }

    case 4: {
    unsigned int remaining = dim;

    float w0 = port_weights[0];
    float w1 = port_weights[1];
    float w2 = port_weights[2];
    float w3 = port_weights[3];
    float w4 = port_weights[4];
    float w5 = port_weights[5];
    float w6 = port_weights[6];
    float w7 = port_weights[7];

    float *input_re_int2 = input_re_int + precoding_l.M;
    float *input_re_int3 = input_re_int + 2*precoding_l.M;
    float *input_re_int4 = input_re_int + 3*precoding_l.M;

    do {
      asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(remaining));

      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int), "r"(stride));
      asm volatile("vfmul.vf v24, v0, %0":: "f"(w0));
      asm volatile("vfmul.vf v28, v0, %0"  :: "f"(w1));

      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v4" :: "f"(w1));
      asm volatile("vfmacc.vf v28, %0, v4":: "f"(w0));

      asm volatile("vlse32.v v16, (%0), %1":: "r"(input_re_int2), "r"(stride));
      asm volatile("vfmacc.vf v24, %0, v16"  :: "f"(w2));
      asm volatile("vfmacc.vf v28, %0, v16"  :: "f"(w3));

      asm volatile("vlse32.v v20, (%0), %1":: "r"(input_re_int2 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v20":: "f"(w3));
      asm volatile("vfmacc.vf v28, %0, v20" :: "f"(w2));

      asm volatile("vlse32.v v0, (%0), %1":: "r"(input_re_int3), "r"(stride));
      asm volatile("vfmacc.vf  v24, %0, v0":: "f"(w4));
      asm volatile("vfmacc.vf  v28, %0, v0":: "f"(w5));

      asm volatile("vlse32.v v4, (%0), %1":: "r"(input_re_int3 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v4":: "f"(w5));
      asm volatile("vfmacc.vf  v28, %0, v4":: "f"(w4));

      asm volatile("vlse32.v v8, (%0), %1":: "r"(input_re_int4), "r"(stride));
      asm volatile("vfmacc.vf  v24, %0, v8":: "f"(w6));
      asm volatile("vfmacc.vf  v28, %0, v8":: "f"(w7));

      asm volatile("vlse32.v v12, (%0), %1":: "r"(input_re_int4 + 1), "r"(stride));
      asm volatile("vfnmsac.vf v24, %0, v12":: "f"(w7));
      asm volatile("vfmacc.vf  v28, %0, v12":: "f"(w6));

      asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(remaining));
      asm volatile("vfncvt.f.f.w v8, v24");
      asm volatile("vsse16.v v8, (%0), %1":: "r"(port_re_int), "r"(stride_half));
      asm volatile("vfncvt.f.f.w v12, v28");
      asm volatile("vsse16.v v12, (%0), %1":: "r"(port_re_int + 1), "r"(stride_half));

      // Bump pointers
      input_re_int  += 2*vl;
      input_re_int2 += 2*vl;
      input_re_int3 += 2*vl;
      input_re_int4 += 2*vl;
      port_re_int   += 2*vl;
      remaining    -= vl;
    } while (remaining > 0);
    break;
}
  }
  snrt_cluster_hw_barrier();
    // End timer and check if new best runtime
  if (cid == 0)
    timer = benchmark_get_cycle() - timer;
}
