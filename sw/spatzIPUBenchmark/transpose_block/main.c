#include <stdint.h>
#include <math.h>
#include <snrt.h>
#include "printf.h"
#include <spatz_cluster_peripheral.h>

#define THRESHOLD       0.00010f

#include "benchmark/benchmark.c"

#include "data/data.h"
#include "Golden/gold.h"


int8_t *src1;
int8_t *dst1;
int8_t *src2;
int8_t *dst2;



void check_result(int8_t *x, int r){    
    int err = 0;

    for(int i = 0; i < r; i++){  
        if(i%1000 == 0)
            // printf("i = %d\n",i);
            snrt_cluster_hw_barrier();
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

void transpose_blocks(
  const int8_t *src, int8_t *dst,
  size_t rows, size_t cols, size_t block_bytes,
  size_t src_row_pitch_bytes, size_t dst_row_pitch_bytes, unsigned int vl){

  size_t avl;
  int8_t *src_pntr;
  int8_t *dst_pntr;
  
  for (int c = 0; c < cols; c++){
    for (int i = 0; i < block_bytes; i++){
      src_pntr = src + i + c*block_bytes;
      dst_pntr = dst + i + c*(rows * block_bytes);
      avl = rows;
      do{
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        
        asm volatile("vlse8.v v0, (%0), %1" :: "r"(src_pntr), "r"(cols*block_bytes));
        src_pntr += vl*cols*block_bytes;
        
        asm volatile("vsse8.v v0, (%0), %1" ::"r"(dst_pntr), "r"(block_bytes));
        dst_pntr += vl*block_bytes;
        
        avl -= vl;
      } while (avl > 0);
    }
  }
}

void transpose_blocks2(
  const int8_t *src, int8_t *dst,
  size_t rows, size_t cols, size_t block_bytes,
  size_t src_row_pitch_bytes, size_t dst_row_pitch_bytes, unsigned int vl){

  size_t avl;
  const int8_t *src_pntr;
  int8_t *dst_pntr;

  for (int c = 0; c < cols; c++){

    if (block_bytes == 2 || block_bytes == 4 || block_bytes == 8){
      src_pntr = src + c*block_bytes;
      dst_pntr = dst + c*(rows * block_bytes);
      avl = rows;
      do{
        if (block_bytes == 2){
          asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
          asm volatile("vlse16.v v0, (%0), %1" :: "r"(src_pntr), "r"(cols*block_bytes));
          asm volatile("vse16.v  v0, (%0)"     :: "r"(dst_pntr));
        } else if (block_bytes == 4){
          asm volatile("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(vl) : "r"(avl));
          asm volatile("vlse32.v v0, (%0), %1" :: "r"(src_pntr), "r"(cols*block_bytes));
          asm volatile("vse32.v  v0, (%0)"     :: "r"(dst_pntr));
        } else { // block_bytes == 8
          asm volatile("vsetvli %0, %1, e64, m8, ta, ma" : "=r"(vl) : "r"(avl));
          asm volatile("vlse64.v v0, (%0), %1" :: "r"(src_pntr), "r"(cols*block_bytes));
          asm volatile("vse64.v  v0, (%0)"     :: "r"(dst_pntr));
        }

        src_pntr += vl*cols*block_bytes;
        dst_pntr += vl*block_bytes;
        avl -= vl;
      } while (avl > 0);

    } else {
      for (int i = 0; i < block_bytes; i++){
        src_pntr = src + i + c*block_bytes;
        dst_pntr = dst + i + c*(rows * block_bytes);
        avl = rows;
        do{
          asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
          asm volatile("vlse8.v v0, (%0), %1" :: "r"(src_pntr), "r"(cols*block_bytes));
          src_pntr += vl*cols*block_bytes;
          asm volatile("vsse8.v v0, (%0), %1" ::"r"(dst_pntr), "r"(block_bytes));
          dst_pntr += vl*block_bytes;
          avl -= vl;
        } while (avl > 0);
      }
    }
  }
}



void transpose_blocks_scalar(
  const int8_t *src, int8_t *dst,
  size_t rows, size_t cols, size_t block_bytes,
  size_t src_row_pitch_bytes, size_t dst_row_pitch_bytes){
  
    if (rows == 0 || cols == 0 || block_bytes == 0) return;
    for (size_t row = 0; row < rows; ++row)
        for (size_t col = 0; col < cols; ++col)
            for (size_t byte = 0; byte < block_bytes; ++byte)
                dst[col * rows*block_bytes + row * block_bytes + byte] =
                    src[row * cols*block_bytes + col * block_bytes + byte];
}



int main() {  // ASSUME ONE COL FIT IN L1 (COLS * BLOCK_BYTES < 24 KB)

    volatile int errors = 0;

    const unsigned int num_cores = snrt_cluster_core_num();
    const unsigned int cid = snrt_cluster_core_idx();

    // Reset timer
    unsigned int timer = (unsigned int)-1;

    printf("start_addr: %x \n", snrt_cluster_memory().end);

    // Core 0 programs DMA transfer of inputs
    int L1_rows = 24*1024/(BLOCK_BYTES*COLS);

    int nchunks = (ROWS + L1_rows - 1) / L1_rows;

    int rows0 = (ROWS < L1_rows) ? ROWS : L1_rows;
    int rows2;
    int next_rows;
    int rem_rows;
    if (cid == 0) {

        src1 = (int8_t   *)snrt_l1alloc(L1_rows * COLS * BLOCK_BYTES * sizeof(int8_t));//24k
        dst1 = (int8_t   *)snrt_l1alloc(L1_rows * COLS * BLOCK_BYTES * sizeof(int8_t));//24k
        src2 = (int8_t   *)snrt_l1alloc(L1_rows * COLS * BLOCK_BYTES * sizeof(int8_t));//24k
        dst2 = (int8_t   *)snrt_l1alloc(L1_rows * COLS * BLOCK_BYTES * sizeof(int8_t));//24k


        snrt_dma_start_2d(src1, src_dram, COLS * BLOCK_BYTES * sizeof(int8_t), COLS * BLOCK_BYTES * sizeof(int8_t), SRC_PITCH, rows0);//is the ROWS correct?

        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();
    unsigned int vl;

    // Start timer
    timer = benchmark_get_cycle();
    int i;    
    // Start dump
    start_kernel();

    // printf("nchunks = %d\n",nchunks);

    for (i = 0; i+1 < nchunks; i += 2) {

      rows2 = ROWS - (i + 1) * L1_rows;

      if (rows2 > L1_rows){
          rows2 = L1_rows;
      }

      snrt_dma_start_2d(src2, src_dram + (i+1) * L1_rows * SRC_PITCH, COLS * BLOCK_BYTES * sizeof(int8_t), COLS * BLOCK_BYTES * sizeof(int8_t), SRC_PITCH, rows2);//is the ROWS correct?

      // transpose_blocks(src1, dst1, L1_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      transpose_blocks2(src1, dst1, L1_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      // transpose_blocks_scalar(src1, dst1, L1_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH);

      snrt_dma_wait_all();

      snrt_dma_start_2d(dst_dram + i * L1_rows * BLOCK_BYTES, dst1, L1_rows * BLOCK_BYTES * sizeof(int8_t), DST_PITCH, L1_rows * BLOCK_BYTES * sizeof(int8_t), COLS);

      if (i + 2 < nchunks) {
          next_rows = ROWS - (i + 2) * L1_rows;
        if (next_rows > L1_rows){
            next_rows = L1_rows;
        }

        snrt_dma_start_2d(src1, src_dram + (i+2) * L1_rows * SRC_PITCH, COLS * BLOCK_BYTES * sizeof(int8_t), COLS * BLOCK_BYTES * sizeof(int8_t), SRC_PITCH, next_rows);//is the ROWS correct?
      }
      // transpose_blocks(src2, dst2, rows2, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      transpose_blocks2(src2, dst2, rows2, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      // transpose_blocks_scalar(src2, dst2, rows2, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH);

      snrt_dma_wait_all();

      snrt_dma_start_2d(dst_dram + (i+1) * L1_rows * BLOCK_BYTES, dst2, rows2 * BLOCK_BYTES * sizeof(int8_t), DST_PITCH, rows2 * BLOCK_BYTES * sizeof(int8_t), COLS);

    }

    if (i < nchunks) {
      rem_rows=ROWS-(L1_rows)*i;

      // transpose_blocks(src1, dst1, rem_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      transpose_blocks2(src1, dst1, rem_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH, vl);
      // transpose_blocks_scalar(src1, dst1, rem_rows, COLS, BLOCK_BYTES, SRC_PITCH, DST_PITCH);

      snrt_dma_start_2d(dst_dram + i* L1_rows * BLOCK_BYTES, dst1, rem_rows * BLOCK_BYTES * sizeof(int8_t), DST_PITCH, rem_rows * BLOCK_BYTES * sizeof(int8_t), COLS);
    }
    snrt_dma_wait_all();
    
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

    check_result(dst_dram,GOLD_SIZE);

    snrt_cluster_hw_barrier();
    return errors;

}
