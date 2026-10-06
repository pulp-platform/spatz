// SPDX-License-Identifier: SHL-0.51
// ResNet: overlap next work staging, DIMC compute, and completed-output DMA.
#include <stdint.h>
#include <stdio.h>
#include <printf.h>
#include "snrt.h"
#include "team.h"
#include "dimc_insn.h"
#ifndef RESNET_OVERLAP_HEADER
#error "Build with a DIMC test input header"
#endif
#include RESNET_OVERLAP_HEADER
#ifndef RESNET_OVERLAP_CHECK
#define RESNET_OVERLAP_CHECK 0
#endif
#ifndef RESNET_ABLATION_KERNEL_REUSE
#define RESNET_ABLATION_KERNEL_REUSE 1
#endif
#ifndef RESNET_ABLATION_FEATURE_REUSE
#define RESNET_ABLATION_FEATURE_REUSE 1
#endif
#ifndef RESNET_ABLATION_INIT_BYPASS
#define RESNET_ABLATION_INIT_BYPASS 1
#endif
#ifndef RESNET_ABLATION_ASYNC_DMA
#define RESNET_ABLATION_ASYNC_DMA 1
#endif
#ifndef RESNET_ABLATION_VECTOR_OVERLAP
#define RESNET_ABLATION_VECTOR_OVERLAP 1
#endif
#ifndef RESNET_ABLATION_DEBUG
#define RESNET_ABLATION_DEBUG 0
#endif
// One-row workloads use the shared overlap schedule.
#if OUT_ROWS == 1
#include "vmvm_resnet_overlap.c"
#else
_Static_assert(FEAT_COLS % 128 == 0 && OUT_COLS % 8 == 0, "K128 and N8 alignment required");
#ifndef RESNET_RUN_ROWS
#define RESNET_RUN_ROWS OUT_ROWS
#endif
_Static_assert(RESNET_RUN_ROWS > 0 && RESNET_RUN_ROWS <= OUT_ROWS, "Invalid row limit");
#define RESIDENT_INPUTS (OUT_COLS == 64 && FEAT_COLS <= 640)
#if RESIDENT_INPUTS
#define INPUT_STRIDE FEAT_COLS
#else
#define INPUT_STRIDE 128
#endif
#define TILE_ROWS (RESIDENT_INPUTS ? ((120832-64*FEAT_COLS)/(2*FEAT_COLS+256)/8)*8 : (OUT_ROWS<216 ? OUT_ROWS : 216))
#ifdef RESNET_B8_PREFIX
#undef RESNET_RUN_ROWS
// Include the resident layers' actual final-tile shape as well as a full tile.
// Other prefixes retain repeated B8 groups and the layer's exact short tail.
#define RESNET_RUN_ROWS (RESIDENT_INPUTS ? \
    (TILE_ROWS+(OUT_ROWS%TILE_ROWS ? OUT_ROWS%TILE_ROWS : 8)) : \
    (OUT_ROWS==784 ? 224 : (40+OUT_ROWS%8)))
#endif
// Carry completed outputs across K chunks for every convolution schedule.
#if defined(RESNET_PSUM_B8_GENERAL)
#define RESNET_CHUNK_OVERLAP 1
#else
#define RESNET_CHUNK_OVERLAP 0
#endif
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

#if RESNET_ABLATION_VECTOR_OVERLAP
#define ABLATION_SPATZ_DRAIN() ((void)0)
#else
#define ABLATION_SPATZ_DRAIN() do { \
    uint32_t ignored; \
    asm volatile("csrr %0, fcsr" : "=r"(ignored) :: "memory"); \
} while (0)
#endif
#if RESNET_ABLATION_KERNEL_REUSE
#define ABLATION_REARM_KERNEL() ((void)0)
#else
#define ABLATION_REARM_KERNEL() CSR(0x7d3,1)
#endif



typedef struct work {
    const uint8_t *features, *kernel;
    uint32_t *out;
#if RESNET_CHUNK_OVERLAP && RESIDENT_INPUTS
    // Resident inputs need no per-chunk DMA addresses. Compact descriptors
    // keep the complete plan in TCDM without shrinking the configured tiles.
    unsigned rb:14, rows:8, chunk:3, channel_block:1, ab:1, ob:1, valid:1;
#else
    int rb, cb, rows, k, bank, ab, ob, valid;
#if RESNET_CHUNK_OVERLAP
    const uint8_t *feature_src, *kernel_src;
#endif
#endif
} work_t;
#if RESNET_CHUNK_OVERLAP && RESIDENT_INPUTS
_Static_assert(sizeof(work_t)==16 && OUT_ROWS<(1<<14) && TILE_ROWS<256,
               "Resident work descriptor bounds");
#define WORK_K(w) ((w)->chunk*128)
#define WORK_CB(w) ((w)->channel_block*32)
#else
#define WORK_K(w) ((w)->k)
#define WORK_CB(w) ((w)->cb)
#endif

typedef struct {
    uint32_t *dst[2], *src[2];
    int bytes[2], valid;
} output_dma_t;

#if RESNET_CHUNK_OVERLAP
// Snitch places thread-local storage in TCDM. Avoid external pointer reads in
// the transition; this test's pipeline is used only by compute hart zero.
static __thread struct {
#else
static struct {
#endif
    uint8_t *a[2], *b[2], *kernels;
    uint32_t *out[2], *result;
    output_dma_t pending;
#if RESNET_CHUNK_OVERLAP
    int feature_ready;
    work_t *plan;
#endif
} pipeline;

static void launch_output(void) {
    output_dma_t *d=&pipeline.pending;
    if (!d->valid) return;
    for (int half=0;half<2;half++)
        if (d->bytes[half]) snrt_dma_start_1d(d->dst[half],d->src[half],d->bytes[half]);
    d->valid=0;
}

#if OUT_ROWS > 1
#if RESNET_CHUNK_OVERLAP
#define RESNET_WORK_COUNT (((RESNET_RUN_ROWS+TILE_ROWS-1)/TILE_ROWS) * \
                          (OUT_COLS/32) * (FEAT_COLS/128))
// Keep all transition addresses in TCDM. Build them while the initial input
// DMA runs, inside the measured layer interval. The final record is a sentinel.
static void prepare_plan(void) {
    work_t *entry=pipeline.plan;
    int ob=0;
#if RESIDENT_INPUTS
    int ab=0;
#else
    int bank=0;
#endif
    for (int rb=0;rb<RESNET_RUN_ROWS;rb+=TILE_ROWS) {
        int rows=RESNET_RUN_ROWS-rb; if (rows>TILE_ROWS) rows=TILE_ROWS;
        for (int cb=0;cb<OUT_COLS;cb+=32) {
            for (int k=0;k<FEAT_COLS;k+=128) {
#if RESIDENT_INPUTS
                *entry++=(work_t){.features=pipeline.a[ab]+k,
                    .kernel=pipeline.kernels+cb*FEAT_COLS+k,
                    .out=pipeline.out[ob], .rb=rb, .rows=rows, .chunk=k/128,
                    .channel_block=cb/32, .ab=ab, .ob=ob, .valid=1};
#else
                *entry++=(work_t){.features=pipeline.a[bank], .kernel=pipeline.b[bank],
                    .out=pipeline.out[ob], .rb=rb, .cb=cb, .rows=rows, .k=k,
                    .bank=bank, .ob=ob, .valid=1,
                    .feature_src=&data_A[rb][k], .kernel_src=&data_B[cb][k]};
                bank^=1;
#endif
            }
            ob^=1;
        }
#if RESIDENT_INPUTS
        ab^=1;
#endif
    }
    entry->valid=0;
}
static void launch_transfers(const work_t *work, work_t *next) {
    if (!WORK_K(work)) launch_output();
#if RESIDENT_INPUTS
    (void)next;
    // Kernels stay resident. At the first chunk of a tile, stage all features
    // for the following tile in the other bank while this tile computes.
    if (!WORK_K(work) && !WORK_CB(work)) {
        const work_t *tile=work+(OUT_COLS/32)*(FEAT_COLS/128);
        if (tile->valid) {
            asm volatile(
                ".insn r 0x2b, 0, 0, zero, %[src], zero\n"
                ".insn r 0x2b, 0, 1, zero, %[dst], zero\n"
                ".insn r 0x2b, 0, 2, zero, %[bytes], zero\n"
                :: [src] "r"(&data_A[tile->rb][0]), [dst] "r"(tile->features),
                   [bytes] "r"(tile->rows*FEAT_COLS) : "memory");
        }
    }
#else
    if (next->valid) {
        // Both transfers share strides and width. Capture each request
        // directly, avoiding two call/argument setup sequences. x2 encodes the
        // immediate 2D-copy mode in dmcpyi; its register value is not consumed.
        // Discard transaction IDs because the existing wait-all orders reuse.
        asm volatile(
            ".insn r 0x2b, 0, 0, zero, %[fsrc], zero\n"
            ".insn r 0x2b, 0, 1, zero, %[fdst], zero\n"
            ".insn r 0x2b, 0, 6, zero, %[stride], %[width]\n"
            ".insn r 0x2b, 0, 7, zero, %[rows], zero\n"
            ".insn r 0x2b, 0, 2, zero, %[width], x2\n"
            ".insn r 0x2b, 0, 0, zero, %[ksrc], zero\n"
            ".insn r 0x2b, 0, 1, zero, %[kdst], zero\n"
            ".insn r 0x2b, 0, 7, zero, %[channels], zero\n"
            ".insn r 0x2b, 0, 2, zero, %[width], x2\n"
            :: [fsrc] "r"(next->feature_src), [fdst] "r"(next->features),
               [ksrc] "r"(next->kernel_src), [kdst] "r"(next->kernel),
               [stride] "r"(FEAT_COLS), [width] "r"(128),
               [rows] "r"(next->rows), [channels] "r"(32)
            : "memory");
    }
#endif
}

#else
static void launch_transfers(const work_t *work, work_t *next) {
    if (!WORK_K(work)) launch_output();
    int rb=work->rb, cb=WORK_CB(work), k=WORK_K(work)+128;
    int ab=work->ab, ob=work->ob, bank=work->bank^1;
    if (k==FEAT_COLS) {
        k=0; cb+=32; ob^=1;
        if (cb>=OUT_COLS) { cb=0; rb+=work->rows; ab^=1; }
    }
    next->valid=rb<RESNET_RUN_ROWS;
    if (next->valid) {
        int rows=RESNET_RUN_ROWS-rb; if(rows>TILE_ROWS)rows=TILE_ROWS;
#if RESIDENT_INPUTS
        *next=(work_t){pipeline.a[ab]+k,pipeline.kernels+cb*FEAT_COLS+k,
                      pipeline.out[ob],rb,cb,rows,k,bank,ab,ob,1};
#else
        *next=(work_t){pipeline.a[bank],pipeline.b[bank],pipeline.out[ob],
                      rb,cb,rows,k,bank,ab,ob,1};
        int cols=OUT_COLS-cb; if(cols>32)cols=32;
        snrt_dma_start_2d(pipeline.a[bank],&data_A[rb][k],128,128,FEAT_COLS,rows);
        snrt_dma_start_2d(pipeline.b[bank],&data_B[cb][k],128,128,FEAT_COLS,cols);
#endif
    }
#if RESIDENT_INPUTS
    if (!WORK_K(work) && !WORK_CB(work) && work->rb+work->rows<RESNET_RUN_ROWS) {
        int nr=RESNET_RUN_ROWS-work->rb-work->rows; if(nr>TILE_ROWS)nr=TILE_ROWS;
        snrt_dma_start_1d(pipeline.a[work->ab^1],&data_A[work->rb+work->rows][0],nr*FEAT_COLS);
    }
#endif
}

#endif

static void prepare_output(const work_t *work) {
    output_dma_t *d=&pipeline.pending;
    for (int half=0;half<2;half++) {
        int col=WORK_CB(work)+half*16;
        int cols=OUT_COLS-col; if(cols>16)cols=16; if(cols<0)cols=0;
        d->dst[half]=pipeline.result+col*OUT_ROWS+work->rb*cols;
        d->src[half]=work->out+half*work->rows*16;
        d->bytes[half]=work->rows*cols*4;
    }
    d->valid=0;
}
#if RESNET_CHUNK_OVERLAP
#include "vmvm_resnet_chunk_overlap.h"
#endif

// v0-v15: next kernel first half, only after bootstrap output is stored.
// v16/v17 and v18/v19: alternate current/previous row results.
// v20/v21: old partial sums; v30/v31: alternating lookahead features.
// Kernel bootstrap temporarily uses all registers; no live result crosses it.
static __attribute__((always_inline)) inline void bootstrap(
    const uint8_t *kernel, const uint8_t *feature, int prefetched, const work_t *work, work_t *next) {
#if RESNET_CHUNK_OVERLAP
    if (pipeline.feature_ready==2) pipeline.feature_ready=0;
    else chunk_bootstrap_prepared(kernel,feature,prefetched,0,0,0);
    launch_transfers(work,next);
    return;
#else
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
    LD8(31,feature); SF(0,0,31,3); ABLATION_SPATZ_DRAIN();
    // The previous block's final vector stores have drained before this call.
    // DMA issue and next-tile staging now run after current DIMC work is issued.
    launch_transfers(work,next);
#if !RESNET_ABLATION_ASYNC_DMA
    snrt_dma_wait_all();
#endif
    CSR(0x7d4,RESNET_ABLATION_FEATURE_REUSE); ABLATION_REARM_KERNEL();
    SF(0,1,31,7); ABLATION_SPATZ_DRAIN();
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
    LD8(2,feature); SF(1,2,2,3); ABLATION_SPATZ_DRAIN();
    CSR(0x7d4,RESNET_ABLATION_FEATURE_REUSE); ABLATION_REARM_KERNEL();
    SF(1,3,2,7); ABLATION_SPATZ_DRAIN();
    CSR(0x7d3,0); CSR(0x7d4,0);
#endif
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

#if !RESNET_ABLATION_KERNEL_REUSE
// Serialized correctness reference.  Each instruction has a private
// 16-output slice, so forced initialization cannot clear a half produced by a
// preceding instruction.  A kernel-load transaction consumes v0-v15 even when
// only group zero is selected; all 16 kernels therefore have to be refreshed.
#define ABLATION_LOAD_KERNEL16(BASE) do { \
    LD8(0,(BASE)+0*INPUT_STRIDE); LD8(1,(BASE)+1*INPUT_STRIDE); \
    LD8(2,(BASE)+2*INPUT_STRIDE); LD8(3,(BASE)+3*INPUT_STRIDE); \
    LD8(4,(BASE)+4*INPUT_STRIDE); LD8(5,(BASE)+5*INPUT_STRIDE); \
    LD8(6,(BASE)+6*INPUT_STRIDE); LD8(7,(BASE)+7*INPUT_STRIDE); \
    LD8(8,(BASE)+8*INPUT_STRIDE); LD8(9,(BASE)+9*INPUT_STRIDE); \
    LD8(10,(BASE)+10*INPUT_STRIDE); LD8(11,(BASE)+11*INPUT_STRIDE); \
    LD8(12,(BASE)+12*INPUT_STRIDE); LD8(13,(BASE)+13*INPUT_STRIDE); \
    LD8(14,(BASE)+14*INPUT_STRIDE); LD8(15,(BASE)+15*INPUT_STRIDE); \
} while (0)
#define ABLATION_E32_8() asm volatile("vsetvli zero,%0,e32,m1,ta,ma" :: "r"(8) : "memory")
#define ABLATION_ONE16(KBASE,DST) do { \
    uint32_t ablation_csr_sync; \
    E8(); CSR(0x7d3,1); CSR(0x7d4,0); CSR(0x7d5,0); \
    ABLATION_LOAD_KERNEL16(KBASE); LD8(31,features+r*INPUT_STRIDE); \
    /* DIMC kernel registers are implicit operands, so the vector scoreboard */ \
    /* cannot infer their RAW dependency.  Cover the eight-load issue tail. */ \
    asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop" ::: "memory"); \
    asm volatile("csrr %0, 0x7d3" : "=r"(ablation_csr_sync) :: "memory"); \
    asm volatile("" :: "r"(ablation_csr_sync) : "memory"); \
    SF(16,0,31,3); ABLATION_SPATZ_DRAIN(); \
    LD8(31,features+r*INPUT_STRIDE); SF(17,1,31,7); ABLATION_SPATZ_DRAIN(); \
    ABLATION_E32_8(); \
    if (chunk) { LD32(18,DST); LD32(19,(DST)+8); ADD(16,18); ADD(17,19); } \
    ST32(16,DST); ST32(17,(DST)+8); \
    asm volatile("lw zero, 0(%0)" :: "r"(DST) : "memory"); \
} while (0)

static void compute_chunk_no_kernel_reuse(const work_t *work, work_t *next) {
    const uint8_t *features=work->features, *kernel=work->kernel;
    uint32_t *out=work->out;
    int rows=work->rows, chunk=WORK_K(work)!=0;
    launch_transfers(work,next);
    snrt_dma_wait_all();
#if RESNET_ABLATION_DEBUG
    printf("ABLATION_INPUT k=%d feature=%d kernel_ptr=%x feature_ptr=%x\n",
           kernel[0],features[0],(unsigned)kernel,(unsigned)features);
#endif
    for (int r=0;r<rows;r++) {
        ABLATION_ONE16(kernel+0*INPUT_STRIDE,out+r*16);
        ABLATION_ONE16(kernel+16*INPUT_STRIDE,out+rows*16+r*16);
#if RESNET_ABLATION_DEBUG
        if (r==0) printf("ABLATION_OUTPUT chunk=%d first=%x\n",WORK_K(work),(unsigned)out[0]);
#endif
    }
    if (WORK_K(work)+128==FEAT_COLS) prepare_output(work);
}
#undef ABLATION_ONE16
#undef ABLATION_E32_8
#undef ABLATION_LOAD_KERNEL16
#endif



// Eight tail rows stage v0-v15 after their bootstrap results have been stored.
// The DMA completion poll also protects reuse of the other output buffer.
#define ROW(LOW,HIGH,FEATURE,NEXTFEATURE,PREVLOW,PREVHIGH,TAIL,PAIR) do { \
    E8(); CSR(0x7d4,0); ABLATION_REARM_KERNEL(); SF(LOW,0,FEATURE,3); ABLATION_SPATZ_DRAIN(); \
    if (r+1<rows) { LD8(NEXTFEATURE,features+(r+1)*INPUT_STRIDE); } \
    if (TAIL) { \
        if ((PAIR)==0) { \
            if (next->valid) snrt_dma_wait_all(); \
            if (last_chunk) prepare_output(work); \
        } \
        if (next_kernel) prefetch_pair(PAIR,next_kernel); \
    } \
    CSR(0x7d4,RESNET_ABLATION_FEATURE_REUSE); ABLATION_REARM_KERNEL(); SF(LOW,1,FEATURE,7); ABLATION_SPATZ_DRAIN(); \
    CSR(0x7d4,RESNET_ABLATION_FEATURE_REUSE); ABLATION_REARM_KERNEL(); SF(HIGH,2,FEATURE,3); ABLATION_SPATZ_DRAIN(); \
    if (chunk) { E32(); LD32(20,out+r*16); LD32(21,out+rows*16+r*16); E8(); } \
    CSR(0x7d4,RESNET_ABLATION_FEATURE_REUSE); ABLATION_REARM_KERNEL(); SF(HIGH,3,FEATURE,7); ABLATION_SPATZ_DRAIN(); \
    E32(); \
    ST32(PREVLOW,out+(r-1)*16); ST32(PREVHIGH,out+rows*16+(r-1)*16); \
    if (chunk) { ADD(LOW,20); ADD(HIGH,21); } \
    ++r; \
} while (0)
#define PAIR(TAIL,P) do { \
    ROW(18,19,31,30,16,17,TAIL,P); \
    ROW(16,17,30,31,18,19,TAIL,(P)+1); \
} while (0)
#define ODD_PAIR(TAIL,P) do { \
    ROW(16,17,30,31,18,19,TAIL,P); \
    ROW(18,19,31,30,16,17,TAIL,(P)+1); \
} while (0)

static void compute_chunk(const work_t *work, work_t *next, int prefetched) {
    const uint8_t *features=work->features, *kernel=work->kernel;
    uint32_t *out=work->out;
    int rows=work->rows, chunk=WORK_K(work)!=0, last_chunk=WORK_K(work)+128==FEAT_COLS;
    bootstrap(kernel,features,prefetched,work,next);
    const uint8_t *next_kernel=next->valid && rows>=10 ? next->kernel : 0;
    E32();
    if (chunk) { LD32(20,out); LD32(21,out+rows*16); ADD(0,20); ADD(1,21); }
    if (rows==1) {
        if (last_chunk) prepare_output(work);
        ST32(0,out); ST32(1,out+16);
        return;
    }
    E8(); LD8(30,features+INPUT_STRIDE);
    int r=1;
    ROW(16,17,30,31,0,1,0,0);
    int tail=rows>=10 ? rows-8 : rows;
    for (;r+8<=tail;) { PAIR(0,0); PAIR(0,0); PAIR(0,0); PAIR(0,0); }
    for (;r+1<tail;) { PAIR(0,0); }
    if (r<tail) { ROW(18,19,31,30,16,17,0,0); }
    if (rows>=10) {
        // Conv5 has 49 rows: its final eight rows begin with the odd register bank.
        if (r&1) { ODD_PAIR(1,0); ODD_PAIR(1,2); ODD_PAIR(1,4); ODD_PAIR(1,6); }
        else { PAIR(1,0); PAIR(1,2); PAIR(1,4); PAIR(1,6); }
    } else if (last_chunk) prepare_output(work);
    E32();
    if ((rows-1)&1) { ST32(16,out+(rows-1)*16); ST32(17,out+rows*16+(rows-1)*16); }
    else { ST32(18,out+(rows-1)*16); ST32(19,out+rows*16+(rows-1)*16); }
}

#ifdef RESNET_PSUM_B8_GENERAL
#include "vmvm_resnet_b8.h"
#endif

static uint32_t run_layer(uint64_t *compute_cycles) {
    uint32_t start=cycles();
    int first_rows=RESNET_RUN_ROWS<TILE_ROWS ? RESNET_RUN_ROWS : TILE_ROWS;
#if RESIDENT_INPUTS
    snrt_dma_start_1d(pipeline.a[0],data_A,first_rows*FEAT_COLS);
    snrt_dma_start_1d(pipeline.kernels,data_B,OUT_COLS*FEAT_COLS);
#else
    snrt_dma_start_2d(pipeline.a[0],data_A,128,128,FEAT_COLS,first_rows);
    snrt_dma_start_2d(pipeline.b[0],data_B,128,128,FEAT_COLS,32);
#endif
#if RESNET_CHUNK_OVERLAP
    prepare_plan();
#endif
    snrt_dma_wait_all();
#if RESNET_CHUNK_OVERLAP
    work_t *current=pipeline.plan;
    int prefetched=0;
    while (current->valid) {
        work_t *next=current+1;
#else
    work_t work[2];
    work[0]=(work_t){pipeline.a[0],RESIDENT_INPUTS?pipeline.kernels:pipeline.b[0],
                    pipeline.out[0],0,0,first_rows,0,0,0,0,1};
    int slot=0, prefetched=0;
    while (work[slot].valid) {
        work_t *current=&work[slot], *next=&work[slot^1];
#endif
        uint32_t cs=cycles();
#if !RESNET_ABLATION_KERNEL_REUSE
        compute_chunk_no_kernel_reuse(current,next);
#elif defined(RESNET_PSUM_B8_GENERAL)
        if (WORK_K(current)) compute_chunk_b8(current,next,prefetched);
        else compute_chunk(current,next,prefetched);
#else
        compute_chunk(current,next,prefetched);
#endif
        *compute_cycles+=(uint32_t)(cycles()-cs);
        if (current->rows<10) snrt_dma_wait_all();
        if (WORK_K(current)+128==FEAT_COLS) {
#if RESNET_CHUNK_OVERLAP
            // Carried stores are issued by the next bootstrap, which drains
            // them before launch_output can publish this completed block.
            if (pipeline.feature_ready!=2)
#endif
            asm volatile("lw zero, 0(%0)" :: "r"(current->out+current->rows*32-1) : "memory");
            pipeline.pending.valid=1;
        }
        prefetched=current->rows>=10;
#if RESNET_CHUNK_OVERLAP
        ++current;
#else
        slot^=1;
#endif
    }
    launch_output(); snrt_dma_wait_all();
    return cycles()-start;
}

#endif

static volatile int failed;
int main(void) {
    snrt_cluster_hw_barrier();
    if (snrt_cluster_core_idx()==0) {
#if RESNET_CHUNK_OVERLAP
        // Explicitly initialize handoff flags before the first chunk. Runtime
        // TLS startup is not relied on to zero the complete metadata record.
        pipeline.pending.valid=0;
        pipeline.feature_ready=0;
#endif
#ifdef RESNET_ABLATION_BUILD
        // Address zero is TCDM in the standalone RTL map but is a null DMA
        // endpoint. Reserve one word for the experimental ablation targets.
        if (snrt_cluster_memory().start==0) (void)snrt_l1alloc(8);
#endif
        pipeline.result=snrt_l3alloc(OUT_ROWS*OUT_COLS*4);
#if OUT_ROWS==1
        pipeline.a[0]=snrt_l1alloc(FEAT_COLS);
        pipeline.b[0]=snrt_l1alloc(16*FEAT_COLS);
        pipeline.b[1]=snrt_l1alloc(16*FEAT_COLS);
        pipeline.out[0]=snrt_l1alloc(64); pipeline.out[1]=snrt_l1alloc(64);
#else
        pipeline.a[0]=snrt_l1alloc(TILE_ROWS*INPUT_STRIDE);
        pipeline.a[1]=snrt_l1alloc(TILE_ROWS*INPUT_STRIDE);
#if RESIDENT_INPUTS
        pipeline.kernels=snrt_l1alloc(OUT_COLS*FEAT_COLS);
#else
        pipeline.b[0]=snrt_l1alloc(32*128); pipeline.b[1]=snrt_l1alloc(32*128);
#endif
        pipeline.out[0]=snrt_l1alloc(TILE_ROWS*32*4);
        pipeline.out[1]=snrt_l1alloc(TILE_ROWS*32*4);
#if RESNET_CHUNK_OVERLAP
        pipeline.plan=snrt_l1alloc((RESNET_WORK_COUNT+1)*sizeof(work_t));
#endif
#endif
        if (!pipeline.result || !pipeline.a[0] || !pipeline.out[0] || !pipeline.out[1]
#if RESNET_CHUNK_OVERLAP
            || !pipeline.plan
#endif
#if OUT_ROWS==1
            || !pipeline.b[0] || !pipeline.b[1]
#else
            || !pipeline.a[1]
#if RESIDENT_INPUTS
            || !pipeline.kernels
#else
            || !pipeline.b[0] || !pipeline.b[1]
#endif
#endif
        ) {
            snrt_slice_t mem=snrt_cluster_memory();
            printf("FAIL: insufficient allocation space tcdm_start=%x tcdm_end=%x a0=%x a1=%x b0=%x b1=%x out0=%x out1=%x\n",
                   (unsigned)mem.start,(unsigned)mem.end,(unsigned)pipeline.a[0],(unsigned)pipeline.a[1],
                   (unsigned)pipeline.b[0],(unsigned)pipeline.b[1],(unsigned)pipeline.out[0],
                   (unsigned)pipeline.out[1]);
            failed=1;
            goto finished;
        }
        printf("RESNET_TRANSITION M=%d logical_K=%d padded_K=%d N=%d scheduled_rows=%d tile_rows=%d batch_rows=8 numeric_check=%d\n",
               OUT_ROWS,VMVM_LOGICAL_K,FEAT_COLS,OUT_COLS,RESNET_RUN_ROWS,TILE_ROWS,RESNET_OVERLAP_CHECK);
#if defined(RESNET_PSUM_B8_GENERAL)
        printf("PSUM_SCHEDULE batch_rows=8 completed_output_vregs=16 positions_per_group=8 sew=32\n");
#endif
        CSR(0x7d5,RESNET_ABLATION_INIT_BYPASS);
        uint64_t compute_cycles=0;
        uint32_t elapsed=run_layer(&compute_cycles);
        printf("VMVM_BENCH resnet_transition total_cycles=%u compute_cycles=%llu\n",elapsed,(unsigned long long)compute_cycles);
#if RESNET_OVERLAP_CHECK
        int checked=0;
        // Match the 16-column-block output/reference layout, including
        // the final eight-column FinalFC block and any validation-only row cap.
        for (int col=0;col<OUT_COLS;col+=16) {
            int cols=OUT_COLS-col; if(cols>16)cols=16;
            for (int rb=0;rb<RESNET_RUN_ROWS;rb+=TILE_ROWS) {
                int rows=RESNET_RUN_ROWS-rb; if(rows>TILE_ROWS)rows=TILE_ROWS;
                int base=col*OUT_ROWS+rb*cols, words=rows*cols;
                snrt_dma_start_1d(pipeline.out[0],pipeline.result+base,words*4);
                snrt_dma_start_1d(pipeline.out[1],serialized_C+base,words*4);
                snrt_dma_wait_all();
                for (int i=0;i<words;i++) {
                    if(pipeline.out[0][i]!=pipeline.out[1][i]) {
                        if(failed<8) printf("Mismatch idx=%d got=%u expected=%u\n",base+i,pipeline.out[0][i],pipeline.out[1][i]);
                        ++failed;
                    }
                    ++checked;
                }
            }
        }
        printf("SOFTWARE_CHECK checked=%d mismatches=%d\n",checked,failed);
        printf(failed ? "FAIL\n" : "PASS\n");
#else
        printf("RTL_TIMING_ONLY numeric_check=external_software\n");
#endif
        CSR(0x7d3,0); CSR(0x7d4,0); CSR(0x7d5,0);
finished:;
    }
    snrt_cluster_hw_barrier();
    return failed ? 1 : 0;
}

#endif // OUT_ROWS == 1
