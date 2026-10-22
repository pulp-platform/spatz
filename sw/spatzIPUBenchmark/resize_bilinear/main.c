#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>

#define THRESHOLD       0.00010f

#include "benchmark/benchmark.c"

#include "data/data.h"
#include "Golden/gold.h"


uint8_t *inp_img1;
uint8_t *out_img1;
uint8_t *inp_img2;
uint8_t *out_img2;
uint8_t *params;

uint8_t *temp8;
uint16_t *temp16;
uint32_t *temp32;

uint32_t *temp32R;
uint32_t *temp32G;
uint32_t *temp32B;

void check_result(uint8_t *x, int r){    
    int err = 0;

    for(int i = 0; i < r; i++){  
        // printf("i = %d\n",i);
        if(x[i] != golden[i]){
            err++;
            printf("Error at index %d:\t expected %d\t real %d\n", i, golden[i], x[i]);
        }
    }

    if(err != 0)
        printf("TEST FAILED with %d errors!!\n", err);
    else
        printf("TEST PASSED!!\n");
}

void resize_bilinear5(const uint8_t *src, uint8_t *dst, const uint8_t *params,int y_idx, unsigned int vl){

  int stridesize = 6;//6 element * 1 byte
  
  uint8_t *src_pntr = src;
  uint8_t *dst_pntr = dst;
  uint8_t *params_pntr_wx = params+PARAMS_WX_OFF;

  int avl = OUT_W;// IN_W/2
  const uint32_t scalar = 256;
  const uint32_t one = 1;
  const uint32_t bias   = 32768;
  uint32_t wy   = params[PARAMS_WY_OFF + y_idx];// ADD INDEX LATER
  uint32_t wy_256   = 256-wy;
  
  do{
    //####      e8        ####
    asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(avl));  

    asm volatile("vle8.v v0 , (%0)" :: "r"(params_pntr_wx));            // wx (uint8)
    params_pntr_wx += vl;

    asm volatile("vwmulu.vx v1, v0, %[A]" ::[A] "r"(one));                    //wx v1/2 (uint16)
    //R
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+0), "r"(stridesize));//RT_l
    asm volatile("vwmulu.vx v8, v3, %[A]" ::[A] "r"(1));                      //RT_L         v8/9 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+3), "r"(stridesize));//RT_R
    asm volatile("vwmulu.vv v4 , v3, v0");                                    //RT_R*(wx) -> v4/5  (uint16)
    //G
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+1), "r"(stridesize));//GT_l
    asm volatile("vwmulu.vx v16, v3, %[A]" ::[A] "r"(1));                     //GT_L         v16/17 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+4), "r"(stridesize));//GT_R
    asm volatile("vwmulu.vv v12 , v3, v0");                                   //GT_R*(wx) -> v12/13  (uint16)
    //B
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+2), "r"(stridesize));//BT_l
    asm volatile("vwmulu.vx v24, v3, %[A]" ::[A] "r"(1));                     //BT_L         v24/25 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+5), "r"(stridesize));//BT_R
    src_pntr+=CH*IN_W;
    asm volatile("vwmulu.vv v20, v3, v0");                                    //BT_R*(wx) -> v20/21  (uint16)

    //R
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+0), "r"(stridesize));//RB_l
    asm volatile("vwmulu.vx v10, v3, %[A]" ::[A] "r"(1));                     //RB_L         v10/11 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+3), "r"(stridesize));//RB_R
    asm volatile("vwmulu.vv v6, v3, v0");                                     //RB_R*(wx) -> v6/7 (uint16)
    //G
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+1), "r"(stridesize));//GB_l
    asm volatile("vwmulu.vx v18, v3, %[A]" ::[A] "r"(1));                     //GB_L         v18/19 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+4), "r"(stridesize));//GB_R
    asm volatile("vwmulu.vv v14, v3, v0");                                    //GB_R*(wx) -> v14/15 (uint16)
    //B
    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+2), "r"(stridesize));//BB_l
    asm volatile("vwmulu.vx v26, v3, %[A]" ::[A] "r"(1));                     //BB_L         v26/27 (uint16) //There is no .wx

    asm volatile("vlse8.v v3 , (%0), %1" :: "r"(src_pntr+5), "r"(stridesize));//BB_R
    asm volatile("vwmulu.vv v22, v3, v0");                                    //BB_R*(wx) -> v22/23 (uint16)
    src_pntr+=6*vl - CH*IN_W;

    //####      e16        ####
    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));  

    asm volatile("vrsub.vx v1, v1, %[A]" ::[A] "r"(scalar));                  //(256-wx) v1/2 (uint16)

    //R
    asm volatile("vmacc.vv v4, v8 , v1");                                       //RTOP = RT_R*(wx) + RT_l*(256-wx) -> v4/5 (uint16)
    asm volatile("vmacc.vv v6, v10, v1");                                       //RBOT = RB_R*(wx) + RB_l*(256-wx) -> v6/7 (uint16)
    // v8 to v11 are free 
    //G
    asm volatile("vmacc.vv v12, v16, v1");                                       //GTOP = GT_R*(wx) + GT_l*(256-wx) -> v12/13 (uint16)
    asm volatile("vmacc.vv v14, v18, v1");                                       //GBOT = GB_R*(wx) + GB_l*(256-wx) -> v14/15 (uint16)
    // v16 to v19 are free 
    //B
    asm volatile("vmacc.vv v20, v24, v1");                                       //BTOP = BT_R*(wx) + BT_l*(256-wx) -> v20/21 (uint16)
    asm volatile("vmacc.vv v22, v26, v1");                                       //BBOT = BB_R*(wx) + BB_l*(256-wx) -> v22/23 (uint16)
    // v24 to v27 are free 


    //####      e32        ####
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));  

    asm volatile("vmv.v.x v8 , %[A]" ::[A] "r"(bias));                         //bias 32768 v8 /11 (uint32)
    asm volatile("vmv.v.x v16, %[A]" ::[A] "r"(bias));                         //bias 32768 v16/19 (uint32)
    asm volatile("vmv.v.x v24, %[A]" ::[A] "r"(bias));                         //bias 32768 v24/27 (uint32)


    //####      e16        ####
    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));

    //R
    asm volatile("vwmaccu.vx v8 , %[A], v6" ::[A] "r"(wy));                    //RBOT*(wy) + bias v8/-/11 (uint32)

    asm volatile("vwmaccu.vx v8 , %[A], v4" ::[A] "r"(wy_256));                //RBOT*(wy) + RTOP*(256-wy) + bias v8/-/11 (uint32)
    //G
    asm volatile("vwmaccu.vx v16, %[A], v14" ::[A] "r"(wy));                   //GBOT*(wy) + bias v16/-/19 (uint32)

    asm volatile("vwmaccu.vx v16, %[A], v12" ::[A] "r"(wy_256));               //GBOT*(wy) + GTOP*(256-wy) + bias v16/-/19 (uint32)
    //B
    asm volatile("vwmaccu.vx v24, %[A], v22" ::[A] "r"(wy));                   //BBOT*(wy) + bias v24/-/27 (uint32)

    asm volatile("vwmaccu.vx v24, %[A], v20" ::[A] "r"(wy_256));               //BBOT*(wy) + GTOP*(256-wy) + bias v24/-/27 (uint32)



    //####      e32        ####
    asm volatile("vsetvli %0, %1, e32, m4, ta, ma" : "=r"(vl) : "r"(avl));  

    asm volatile("vsrl.vi v8, v8, 16");

    asm volatile("vse32.v v8, (%0);" ::"r"(temp32));

    asm volatile("vlse8.v v8 , (%0), %1" :: "r"(temp32), "r"(4));

    asm volatile("vsse8.v v8, (%0), %1" ::"r"(dst_pntr), "r"(stridesize/2));


    asm volatile("vsrl.vi v16, v16, 16");

    asm volatile("vse32.v v16, (%0);" ::"r"(temp32));

    asm volatile("vlse8.v v16, (%0), %1" :: "r"(temp32), "r"(4));

    asm volatile("vsse8.v v16, (%0), %1" ::"r"(dst_pntr+1), "r"(stridesize/2));


    asm volatile("vsrl.vi v24, v24, 16");

    asm volatile("vse32.v v24, (%0);" ::"r"(temp32));

    asm volatile("vlse8.v v24, (%0), %1" :: "r"(temp32), "r"(4));

    asm volatile("vsse8.v v24, (%0), %1" ::"r"(dst_pntr+2), "r"(stridesize/2));

    dst_pntr+=3*vl;
    avl-=vl;
  }while(avl > 0);
}


uint8_t bilinear(
    const uint8_t *src, const uint8_t *params, int output_y, int output_x, int channel)
{
    const uint8_t *row0 = src + (size_t)(2*output_y) * IN_W * CH;
    const uint8_t *row1 = src + (size_t)(2*output_y+1) * IN_W * CH;
    uint32_t wx = params[PARAMS_WX_OFF+output_x], wy = params[PARAMS_WY_OFF+output_y];
    int left = (2*output_x) * CH + channel;
    int right = (2*output_x+1) * CH + channel;
    uint32_t top = row0[left] * (256 - wx) + row0[right] * wx;
    uint32_t bottom = row1[left] * (256 - wx) + row1[right] * wx;
    return (uint8_t)((top * (256 - wy) + bottom * wy + (1u << 15)) >> 16);
}

void resize_bilinear_scalar(const uint8_t *src, uint8_t *dst, const uint8_t *params)
{
    // for (int output_y = 0; output_y < OUT_H; ++output_y) {
    //     for (int output_x = 0; output_x < OUT_W; ++output_x) {
    //         for (int channel = 0; channel < CH; ++channel) {
    for (int output_y = 0; output_y < 1; ++output_y) {
        for (int output_x = 0; output_x < OUT_W; ++output_x) {
            for (int channel = 0; channel < CH; ++channel) {              
                dst[((size_t)output_y * OUT_W + output_x) * CH + channel] =
                    bilinear(src, params, output_y, output_x, channel);
            }
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

    // Core 0 programs DMA transfer of inputs
    if (cid == 0) {

        temp8  = (uint8_t    *)snrt_l1alloc(100 * sizeof(uint8_t ));
        temp16 = (uint16_t   *)snrt_l1alloc(100 * sizeof(uint16_t));
        temp32 = (uint32_t   *)snrt_l1alloc(100 * sizeof(uint32_t));

        params = (uint8_t   *)snrt_l1alloc(5000 * sizeof(uint8_t));
        snrt_dma_start_1d(params, params_dram, 5000 * sizeof(uint8_t));

        inp_img1 = (uint8_t   *)snrt_l1alloc(2 * CH * IN_W * sizeof(uint8_t));//CONTAINS TWO ROWS
        inp_img2 = (uint8_t   *)snrt_l1alloc(2 * CH * IN_W * sizeof(uint8_t));//CONTAINS TWO ROWS
        snrt_dma_start_1d(inp_img1, image_dram, 2 * CH * IN_W * sizeof(uint8_t));

        out_img1 = (uint8_t   *)snrt_l1alloc(CH * OUT_W * sizeof(uint8_t));
        out_img2 = (uint8_t   *)snrt_l1alloc(CH * OUT_W * sizeof(uint8_t));
        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    
    // Start dump
    start_kernel();

    // resize_bilinear(inp_img, out_img, params, vl);
    // resize_bilinear1(inp_img, out_img, params, vl);
    // resize_bilinear2(inp_img, out_img, params, vl);
    // resize_bilinear3(inp_img, out_img, params, vl);
    // resize_bilinear4(inp_img, out_img, params, vl);
    // resize_bilinear5(inp_img1, out_img1, params, vl);
    // resize_bilinear_scalar(inp_img1, out_img1, params);


    for (int i = 0; i < OUT_H; i+=2){
      snrt_dma_start_1d(inp_img2, image_dram + (i+1)*2*CH*IN_W, 2 * CH * IN_W * sizeof(uint8_t));

      resize_bilinear5(inp_img1, out_img1, params, i, vl);
      // resize_bilinear_scalar(inp_img1, out_img1, params);
      snrt_dma_wait_all();

      snrt_dma_start_1d(out_dram + (i) * CH * OUT_W, out_img1, CH * OUT_W * sizeof(uint8_t));
      snrt_dma_start_1d(inp_img1, image_dram + (i+2)*2*CH*IN_W, 2 * CH * IN_W * sizeof(uint8_t));

      resize_bilinear5(inp_img2, out_img2, params, i+1, vl);
      // resize_bilinear_scalar(inp_img2, out_img2, params);
      snrt_dma_wait_all();

      snrt_dma_start_1d(out_dram + (i+1) * CH * OUT_W, out_img2, CH * OUT_W * sizeof(uint8_t));
    }
    
    
    // End dump
    stop_kernel();

    // End timer and check if new best runtime
    if (cid == 0)
        timer = benchmark_get_cycle() - timer;

    snrt_cluster_hw_barrier();
    // Display runtime
    if (cid == 0) {
        printf("The execution took %u cycles.\n", timer);
    }

    check_result(out_dram,CH*OUT_W*OUT_H);
    // check_result(out_img1,CH*OUT_W);

    snrt_cluster_hw_barrier();
    return errors;

}
