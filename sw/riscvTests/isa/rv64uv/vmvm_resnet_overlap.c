// SPDX-License-Identifier: SHL-0.51
// ResNet B8 overlap: double-buffered K chunks and row-wise VRF scheduling.
#include <stdint.h>
#include <stdio.h>
#include <printf.h>
#include "snrt.h"
#include "team.h"
#include "dimc_insn.h"
#ifndef RESNET_OVERLAP_HEADER
#define RESNET_OVERLAP_HEADER "dimc_test_matrices.h"
#endif
#include RESNET_OVERLAP_HEADER
#ifndef RESNET_OVERLAP_CHECK
#define RESNET_OVERLAP_CHECK 0
#endif
#ifndef RESNET_OVERLAP_TILE_ROWS
#define RESNET_OVERLAP_TILE_ROWS 216
#endif
_Static_assert(FEAT_COLS % 128 == 0 && OUT_COLS % 8 == 0, "K128 and N8 alignment required");
#define RESIDENT_INPUTS (OUT_ROWS == 1 || (OUT_COLS == 64 && FEAT_COLS <= 640))
#define INPUT_STRIDE (RESIDENT_INPUTS ? FEAT_COLS : 128)
#define S_(x) #x
#define S(x) S_(x)
#define LD8(v,p) asm volatile("vle8.v v" S(v) ", (%0)" :: "r"(p) : "memory")
#define LD32(v,p) asm volatile("vle32.v v" S(v) ", (%0)" :: "r"(p) : "memory")
#define ST32(v,p) asm volatile("vse32.v v" S(v) ", (%0)" :: "r"(p) : "memory")
#define ADD(v,a) asm volatile("vadd.vv v" S(v) ", v" S(v) ", v" S(a) ::: "memory")
#define SF(v,g,f,c) asm volatile(DIMC_ASM(v,g,f,c,0) ::: "memory")
#define CSR(a,v) asm volatile("csrw " S(a) ", %0" :: "r"((uint32_t)(v)) : "memory")
#define E8() asm volatile("vsetvli zero,%0,e8,m1,ta,ma" :: "r"(128) : "memory")
#define E32() asm volatile("vsetvli zero,%0,e32,m1,ta,ma" :: "r"(16) : "memory")
static inline uint32_t cycles(void) {
    uint32_t value;
    asm volatile("rdcycle %0" : "=r"(value));
    return value;
}

// v0-v15: next kernel first half, only after bootstrap output is stored.
// v16/v17 and v18/v19: alternate current/previous row results.
// v20/v21: old partial sums; v30/v31: alternating lookahead features.
// Kernel bootstrap temporarily uses all registers; no live result crosses it.
static __attribute__((always_inline)) inline void bootstrap(
    const uint8_t *kernel, const uint8_t *feature, int prefetched) {
    E8(); CSR(0x7d4,0); CSR(0x7d3,1);
    if (!prefetched) {
        LD8(0,kernel+0*INPUT_STRIDE);
        LD8(1,kernel+1*INPUT_STRIDE);
        LD8(2,kernel+2*INPUT_STRIDE);
        LD8(3,kernel+3*INPUT_STRIDE);
        LD8(4,kernel+4*INPUT_STRIDE);
        LD8(5,kernel+5*INPUT_STRIDE);
        LD8(6,kernel+6*INPUT_STRIDE);
        LD8(7,kernel+7*INPUT_STRIDE);
        LD8(8,kernel+8*INPUT_STRIDE);
        LD8(9,kernel+9*INPUT_STRIDE);
        LD8(10,kernel+10*INPUT_STRIDE);
        LD8(11,kernel+11*INPUT_STRIDE);
        LD8(12,kernel+12*INPUT_STRIDE);
        LD8(13,kernel+13*INPUT_STRIDE);
        LD8(14,kernel+14*INPUT_STRIDE);
        LD8(15,kernel+15*INPUT_STRIDE);
    }
    LD8(31,feature); SF(0,0,31,3); CSR(0x7d4,1); SF(0,1,31,7);
    CSR(0x7d4,0); CSR(0x7d3,1);
    LD8(16,kernel+16*INPUT_STRIDE);
    LD8(17,kernel+17*INPUT_STRIDE);
    LD8(18,kernel+18*INPUT_STRIDE);
    LD8(19,kernel+19*INPUT_STRIDE);
    LD8(20,kernel+20*INPUT_STRIDE);
    LD8(21,kernel+21*INPUT_STRIDE);
    LD8(22,kernel+22*INPUT_STRIDE);
    LD8(23,kernel+23*INPUT_STRIDE);
    LD8(24,kernel+24*INPUT_STRIDE);
    LD8(25,kernel+25*INPUT_STRIDE);
    LD8(26,kernel+26*INPUT_STRIDE);
    LD8(27,kernel+27*INPUT_STRIDE);
    LD8(28,kernel+28*INPUT_STRIDE);
    LD8(29,kernel+29*INPUT_STRIDE);
    LD8(30,kernel+30*INPUT_STRIDE);
    LD8(31,kernel+31*INPUT_STRIDE);
    LD8(2,feature); SF(1,2,2,3); CSR(0x7d4,1); SF(1,3,2,7);
    CSR(0x7d3,0); CSR(0x7d4,0);
}

static __attribute__((always_inline)) inline void prefetch_pair(
    int pair, const uint8_t *kernel) {
    switch (pair) {
    case 0: LD8(0,kernel+0*INPUT_STRIDE); LD8(1,kernel+1*INPUT_STRIDE); break;
    case 1: LD8(2,kernel+2*INPUT_STRIDE); LD8(3,kernel+3*INPUT_STRIDE); break;
    case 2: LD8(4,kernel+4*INPUT_STRIDE); LD8(5,kernel+5*INPUT_STRIDE); break;
    case 3: LD8(6,kernel+6*INPUT_STRIDE); LD8(7,kernel+7*INPUT_STRIDE); break;
    case 4: LD8(8,kernel+8*INPUT_STRIDE); LD8(9,kernel+9*INPUT_STRIDE); break;
    case 5: LD8(10,kernel+10*INPUT_STRIDE); LD8(11,kernel+11*INPUT_STRIDE); break;
    case 6: LD8(12,kernel+12*INPUT_STRIDE); LD8(13,kernel+13*INPUT_STRIDE); break;
    case 7: LD8(14,kernel+14*INPUT_STRIDE); LD8(15,kernel+15*INPUT_STRIDE); break;
    }
}

// Issue the next feature and staged kernel loads after the first current-row
// DIMC instruction. The remaining three instructions reuse the installed feature.
#define ROW(LOW,HIGH,FEATURE,NEXTFEATURE,PREVLOW,PREVHIGH) do { \
    E8(); CSR(0x7d4,0); \
    SF(LOW,0,FEATURE,3); \
    if (r+1 < rows) { LD8(NEXTFEATURE,features+(r+1)*INPUT_STRIDE); } \
    if (next_kernel && r == rows-8) { snrt_dma_wait_all(); } \
    if (next_kernel && r >= rows-8) { prefetch_pair(r-(rows-8),next_kernel); } \
    CSR(0x7d4,1); SF(LOW,1,FEATURE,7); \
    SF(HIGH,2,FEATURE,3); \
    if (chunk) { \
        E32(); LD32(20,out+r*16); LD32(21,out+rows*16+r*16); E8(); \
    } \
    SF(HIGH,3,FEATURE,7); \
    E32(); \
    ST32(PREVLOW,out+(r-1)*16); ST32(PREVHIGH,out+rows*16+(r-1)*16); \
    if (chunk) { \
        ADD(LOW,20); ADD(HIGH,21); \
    } \
} while (0)

static void compute_chunk(const uint8_t *features, const uint8_t *kernel,
                          const uint8_t *next_kernel, uint32_t *out,
                          int rows, int chunk, int prefetched) {
    bootstrap(kernel,features,prefetched);
    E32();
    if (chunk) {
        LD32(20,out); LD32(21,out+rows*16); ADD(0,20); ADD(1,21);
    }
    if (rows == 1) {
        ST32(0,out); ST32(1,out+16);
        return;
    }
    E8(); LD8(30,features+INPUT_STRIDE);
    // Keep the same B8 iteration grouping; results are drained a row at a time
    // to free v0-v15 for next-kernel staging.
    int r=1;
    ROW(16,17,30,31,0,1);
    for (int batch=2; batch<rows; batch+=8) {
        int end=batch+8;
        if(end>rows)end=rows;
        for (r=batch; r+1<end; r+=2) {
            ROW(18,19,31,30,16,17);
            ++r;
            ROW(16,17,30,31,18,19);
            --r;
        }
        if(r<end) { ROW(18,19,31,30,16,17); }
    }
    E32();
    if ((rows-1)&1) { ST32(16,out+(rows-1)*16); ST32(17,out+rows*16+(rows-1)*16); }
    else { ST32(18,out+(rows-1)*16); ST32(19,out+rows*16+(rows-1)*16); }
}


// Both builds execute the same kernel and output DMA. Only the software build
// consumes the reference, after the end-to-end timing interval has ended.
static volatile int failed;
static void stage(uint8_t *a, uint8_t *b, int rb, int rows, int cb, int k) {
    int cols = KERN_ROWS - cb;
    if (cols > 32) cols = 32;
    snrt_dma_start_2d(a, &data_A[rb][k], 128, 128, FEAT_COLS, rows);
    snrt_dma_start_2d(b, &data_B[cb][k], 128, 128, FEAT_COLS, cols);
}
int main(void) {
    snrt_cluster_hw_barrier();
    if (snrt_cluster_core_idx() == 0) {
        uint32_t *result_l2=snrt_l3alloc(OUT_ROWS*OUT_COLS*sizeof(uint32_t));
#if RESIDENT_INPUTS
        const int tile_rows = OUT_ROWS == 1 ? 1 :
            ((120832 - 64*FEAT_COLS) / (2*FEAT_COLS+256) / 8) * 8;
        uint8_t *a[2] = {snrt_l1alloc(tile_rows*FEAT_COLS), snrt_l1alloc(tile_rows*FEAT_COLS)};
        uint8_t *kernels=snrt_l1alloc((OUT_ROWS == 1 ? 32 : OUT_COLS)*FEAT_COLS);
#else
        const int tile_rows = OUT_ROWS < RESNET_OVERLAP_TILE_ROWS ? OUT_ROWS : RESNET_OVERLAP_TILE_ROWS;
        uint8_t *a[2] = {snrt_l1alloc(tile_rows*128), snrt_l1alloc(tile_rows*128)};
        uint8_t *b[2] = {snrt_l1alloc(32*128), snrt_l1alloc(32*128)};
#endif
        uint32_t *out[2] = {snrt_l1alloc(tile_rows*32*4), snrt_l1alloc(tile_rows*32*4)};
        printf("RESNET_B8_OVERLAP M=%d logical_K=%d padded_K=%d N=%d tile_rows=%d batch_rows=8 numeric_check=%d\n",
               OUT_ROWS,VMVM_LOGICAL_K,FEAT_COLS,OUT_COLS,tile_rows,RESNET_OVERLAP_CHECK);
        if (!a[0] || !a[1] || !out[0] || !out[1]
#if RESIDENT_INPUTS
            || !kernels
#else
            || !b[0] || !b[1]
#endif
        ) {
            printf("FAIL allocation\n"); failed=1;
        } else {
            #if !RESIDENT_INPUTS
            // Inactive tail columns are zero in both ping-pong kernel buffers.
            for (int i=0; i<32*128; ++i) b[0][i]=b[1][i]=0;
            #endif
            CSR(0x7d5,1);
            uint64_t compute_cycles=0;
            uint32_t start=cycles();
#if RESIDENT_INPUTS
            int ob=0, ab=0;
            snrt_dma_start_1d(a[0],data_A,tile_rows*FEAT_COLS);
            if (OUT_ROWS != 1) snrt_dma_start_1d(kernels,data_B,OUT_COLS*FEAT_COLS);
            snrt_dma_wait_all();
            for(int rb=0;rb<OUT_ROWS;rb+=tile_rows) {
                int rows=OUT_ROWS-rb;if(rows>tile_rows)rows=tile_rows;
                if(rb+rows<OUT_ROWS) {
                    int nr=OUT_ROWS-rb-rows;if(nr>tile_rows)nr=tile_rows;
                    snrt_dma_start_1d(a[ab^1],&data_A[rb+rows][0],nr*FEAT_COLS);
                }
                for(int cb=0;cb<OUT_COLS;cb+=32) {
                    uint8_t *kernel=kernels+(OUT_ROWS==1 ? 0 : cb*FEAT_COLS);
                    if (OUT_ROWS==1) {
                        int cols=KERN_ROWS-cb;if(cols>32)cols=32;
                        if(cols<32)for(int i=cols*FEAT_COLS;i<32*FEAT_COLS;++i)kernels[i]=0;
                        snrt_dma_start_1d(kernels,&data_B[cb][0],cols*FEAT_COLS);
                        snrt_dma_wait_all();
                    }
                    for(int k=0;k<FEAT_COLS;k+=128) {
                        const uint8_t *next_kernel=k+128<FEAT_COLS && rows>=10 ? kernel+k+128 : 0;
                        uint32_t cs=cycles();
                        compute_chunk(a[ab]+k,kernel+k,next_kernel,out[ob],rows,k!=0,k!=0 && rows>=10);
                        compute_cycles+=(uint32_t)(cycles()-cs);
                    }
                    for(int half=0;half<2 && cb+half*16<OUT_COLS;++half) {
                        int col=cb+half*16;
                        int cols=OUT_COLS-col;if(cols>16)cols=16;
                        snrt_dma_start_2d(result_l2+col*OUT_ROWS+rb*cols,
                            out[ob]+half*rows*16,cols*4,cols*4,16*4,rows);
                    }
                    ob^=1;
                }
                snrt_dma_wait_all();
                ab^=1;
            }
#else
            int ob=0;
            for (int rb=0; rb<OUT_ROWS; rb+=tile_rows) {
                int rows=OUT_ROWS-rb;
                if(rows>tile_rows) rows=tile_rows;
                for (int cb=0; cb<OUT_COLS; cb+=32) {
                    // Zero padded tail columns after a preceding full block.
                    if (KERN_ROWS-cb < 32) {
                        for(int i=(KERN_ROWS-cb)*128;i<32*128;++i) b[0][i]=b[1][i]=0;
                    }
                    stage(a[0],b[0],rb,rows,cb,0);
                    snrt_dma_wait_all();
                    int bank=0, prefetched=0;
                    for(int k=0;k<FEAT_COLS;k+=128) {
                        int next=k+128<FEAT_COLS;
                        if(next) stage(a[bank^1],b[bank^1],rb,rows,cb,k+128);
                        const uint8_t *next_kernel=next && rows>=10 ? b[bank^1] : 0;
                        uint32_t cs=cycles();
                        compute_chunk(a[bank],b[bank],next_kernel,out[ob],rows,k!=0,prefetched);
                        compute_cycles+=(uint32_t)(cycles()-cs);
                        snrt_dma_wait_all();
                        prefetched=next_kernel!=0;
                        bank^=1;
                    }
                    for(int half=0;half<2 && cb+half*16<OUT_COLS;++half) {
                        int col=cb+half*16;
                        int cols=OUT_COLS-col; if(cols>16)cols=16;
                        snrt_dma_start_2d(result_l2+col*OUT_ROWS+rb*cols,
                            out[ob]+half*rows*16,cols*4,cols*4,16*4,rows);
                    }
                    ob^=1;
                }
            }
#endif
            snrt_dma_wait_all();
            uint32_t elapsed=cycles()-start;
            printf("VMVM_BENCH resnet_b8_overlap total_cycles=%u compute_cycles=%llu\n",
                   elapsed,(unsigned long long)compute_cycles);
#if RESNET_OVERLAP_CHECK
            int checked=0;
            for(int base=0;base<OUT_ROWS*OUT_COLS;base+=tile_rows*32) {
                int words=OUT_ROWS*OUT_COLS-base;
                if(words>tile_rows*32)words=tile_rows*32;
                snrt_dma_start_1d(out[0],result_l2+base,words*4);
                snrt_dma_start_1d(out[1],serialized_C+base,words*4);
                snrt_dma_wait_all();
                for(int i=0;i<words;++i) {
                    if(out[0][i]!=out[1][i]) {
                        if(failed<8)printf("Mismatch idx=%d got=%u expected=%u\n",base+i,out[0][i],out[1][i]);
                        ++failed;
                    }
                    ++checked;
                }
            }
            printf("SOFTWARE_CHECK checked=%d mismatches=%d\n",checked,failed);
            printf(failed ? "FAIL\n" : "PASS\n");
#else
            printf("RTL_TIMING_ONLY numeric_check=external_software\n");
#endif
            CSR(0x7d3,0);CSR(0x7d4,0);CSR(0x7d5,0);
        }
    }
    snrt_cluster_hw_barrier();
    return failed ? 1 : 0;
}
