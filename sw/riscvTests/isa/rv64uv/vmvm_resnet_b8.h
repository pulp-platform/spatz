// SPDX-License-Identifier: SHL-0.51
// Shared B8 partial sums: eight positions, sixteen e32 result registers.
_Static_assert(OUT_COLS % 32 == 0, "B8 convolution groups require 32 channels");

// A read-only FCSR access waits for all outstanding Spatz instructions through
// the Snitch interlock. A RAW dependency alone allows partial-word chaining.
#define B8_WAIT_SPATZ() do { \
    uint32_t ignored; \
    asm volatile("csrr %0, fcsr" : "=r"(ignored) :: "memory"); \
} while (0)
#define B8_DRAIN(p) asm volatile("lw zero, 0(%0)" :: "r"(p) : "memory")
#define B8_STORE_PAIR(L,H,P) do { \
    ST32(L,out+(P)*16); ST32(H,out+rows*16+(P)*16); \
} while (0)

#if !RESNET_CHUNK_OVERLAP
#define CHUNK_PREPARE_CARRY(TAIL) ((void)0)
#define CHUNK_FREE_FEATURE_REGISTER() ((void)0)
#define CHUNK_TRY_FINISH(TAIL) 0
#endif

// Current results occupy v16-v31. Old partials occupy v0-v13, while v14/v15
// alternate features. After the final feature is consumed, v14/v15 hold the
// last two old partials. Previous outputs are stored before their registers
// are reused. The peeled bootstrap group uses its own register map below.
// Prepare addresses before launch. The two-entry DIMC queue holds the first
// two instructions while e32 transfers issue; queue the final two together
// after restoring e8 so a VLSU stall cannot separate them. Feature registers
// may be reused for old partials only after the first instruction reads them.
#define B8_ROW(L,H,F,NF,OL,OH,LOOK,STORE,PL,PH,TAIL,P) do { \
    uint32_t *old_low=out+r*16, *old_high=out+rows*16+r*16; \
    const uint8_t *next_feature=features+(r+1)*INPUT_STRIDE; \
    asm volatile("" :: "r"(old_low), "r"(old_high), "r"(next_feature)); \
    E8(); CSR(0x7d4,0); SF(L,0,F,3); \
    if (LOOK) { LD8(NF,next_feature); } \
    if ((TAIL) && (P)==0) { \
        if (next->valid) snrt_dma_wait_all(); \
        if (last_chunk) prepare_output(work); \
    } \
    CSR(0x7d4,1); SF(L,1,F,7); \
    E32(); \
    if ((STORE) && ((STORE)!=2 || (PL)!=30)) { \
        ST32(PL,old_low-((STORE)==2?6:7)*16); ST32(PH,old_high-((STORE)==2?6:7)*16); \
    } \
    LD32(OL,old_low); LD32(OH,old_high); \
    E8(); SF(H,2,F,3); SF(H,3,F,7); \
    ++r; \
} while (0)

// Kernel staging reuses old-partial registers after the IPU has read them.
// Only the final B8 group stages a kernel. The scoreboard protects these WAR
// dependencies; VADD always uses e32, and raw kernel transfers use e8.
#define B8_ADD_PAIR(L,H,OL,OH,P,TAIL) do { \
    ADD(H,OH); ADD(L,OL); \
    if ((TAIL) && next_kernel) { \
        E8(); prefetch_pair(P,next_kernel); E32(); \
    } \
} while (0)
// The first ordinary group establishes carried pointers. Later groups consume
// their prefetched feature and configuration without repeating startup checks.
// Final complete groups retain kernel prefetch; a following short tail instead
// consumes the feature prepared by the last ordinary group.
#define B8_FIRST_ROW(TAIL,READY,STORE) do { \
    uint32_t *old_low=(READY)?b8_low:out+r*16; \
    uint32_t *old_high=(READY)?b8_high:out+rows*16+r*16; \
    const uint8_t *lookahead=(READY)?b8_lookahead:features+(r+1)*INPUT_STRIDE; \
    asm volatile("" :: "r"(old_low), "r"(old_high), "r"(lookahead)); \
    if (!(READY)) { E8(); CSR(0x7d4,0); } \
    if (READY) { B8_WAIT_SPATZ(); } \
    SF(16,0,14,3); LD8(15,lookahead); \
    if (TAIL) { \
        if (next->valid) snrt_dma_wait_all(); \
        if (last_chunk) prepare_output(work); \
    } \
    CSR(0x7d4,1); SF(16,1,14,7); \
    E32(); \
    if ((STORE)) { ST32(18,old_low-((STORE)==2?6:7)*16); ST32(19,old_high-((STORE)==2?6:7)*16); } \
    LD32(0,old_low); LD32(1,old_high); \
    E8(); SF(17,2,14,3); SF(17,3,14,7); \
    ++r; \
} while (0)

#define B8_GROUP(TAIL,READY,STORE) do { \
    int group=r; \
    if (!(READY)) { E8(); LD8(14,features+r*INPUT_STRIDE); } \
    B8_FIRST_ROW(TAIL,READY,STORE); \
    B8_ROW(18,19,15,14, 2, 3,1,(STORE),20,21,TAIL,1); \
    B8_ROW(20,21,14,15, 4, 5,1,(STORE),22,23,TAIL,2); \
    B8_ROW(22,23,15,14, 6, 7,1,(STORE),24,25,TAIL,3); \
    B8_ROW(24,25,14,15, 8, 9,1,(STORE),26,27,TAIL,4); \
    B8_ROW(26,27,15,14,10,11,1,(STORE),28,29,TAIL,5); \
    B8_ROW(28,29,14,15,12,13,1,(STORE),30,31,TAIL,6); \
    B8_ROW(30,31,15,14,14,15,0,0,30,31,TAIL,7); \
    uint32_t *store_low=out+group*16, *store_high=store_low+rows*16; \
    if (!(TAIL)) { \
        b8_low=out+r*16; b8_high=b8_low+rows*16; \
        b8_lookahead=features+(r+1)*INPUT_STRIDE; \
        b8_more=r+8<full_rows; \
        asm volatile("" : "+r"(b8_low), "+r"(b8_high), "+r"(b8_lookahead), \
                          "+r"(b8_more), "+r"(r) : "r"(store_low), "r"(store_high)); \
        CSR(0x7d4,0); \
    } \
    CHUNK_PREPARE_CARRY(TAIL); \
    E32(); B8_WAIT_SPATZ(); \
    if (CHUNK_TRY_FINISH(TAIL)) { } \
    else if (!(TAIL)) { \
        ADD(31,15); ADD(30,14); \
        E8(); LD8(14,b8_lookahead-INPUT_STRIDE); E32(); \
        ADD(17,1); ADD(16,0); ADD(29,13); ADD(28,12); \
        ST32(16,store_low); ST32(17,store_high); \
        ADD(27,11); ADD(26,10); ADD(25,9); ADD(24,8); \
        ADD(23,7); ADD(22,6); ADD(21,5); ADD(20,4); \
        ADD(19,3); ADD(18,2); \
        /* Each accepted VADD retains E32 while the next config becomes E8. */ \
        E8(); \
    } else { \
        B8_ADD_PAIR(30,31,14,15,7,TAIL); \
        CHUNK_FREE_FEATURE_REGISTER(); \
        B8_ADD_PAIR(28,29,12,13,6,TAIL); \
        B8_ADD_PAIR(26,27,10,11,5,TAIL); \
        B8_ADD_PAIR(24,25, 8, 9,4,TAIL); \
        B8_ADD_PAIR(22,23, 6, 7,3,TAIL); \
        B8_ADD_PAIR(20,21, 4, 5,2,TAIL); \
        B8_ADD_PAIR(18,19, 2, 3,1,TAIL); \
        B8_ADD_PAIR(16,17, 0, 1,0,TAIL); \
        ST32(16,store_low); ST32(17,store_high); \
        if (!RESNET_CHUNK_OVERLAP || !next_kernel) { B8_DRAIN(store_low); } \
    } \
} while (0)

static void compute_chunk_b8(const work_t *work, work_t *next, int prefetched) {
#if OUT_ROWS % 8 != 0 || TILE_ROWS % 8 != 0 || RESNET_RUN_ROWS % 8 != 0
    if (work->rows < 8) { compute_chunk(work,next,prefetched); return; }
#endif
    const uint8_t *features=work->features, *kernel=work->kernel;
    uint32_t *out=work->out;
    int rows=work->rows, last_chunk=WORK_K(work)+128==FEAT_COLS;
    int b8_more=0;
    uint32_t *b8_low=0, *b8_high=0;
    const uint8_t *b8_lookahead=0;
    bootstrap(kernel,features,prefetched,work,next);
#if OUT_ROWS % 8 == 0 && TILE_ROWS % 8 == 0 && RESNET_RUN_ROWS % 8 == 0
    const uint8_t *next_kernel=next->valid && rows>=16 ? next->kernel : 0;
#else
    const uint8_t *next_kernel=next->valid && rows>=10 ? next->kernel : 0;
#endif

    // Bootstrap leaves position zero in v0/v1. Keep it until positions 1-7
    // finish in v16-v29. Old partials use v2-v15 and finally v30/v31.
    E32(); LD32(2,out); LD32(3,out+rows*16);
    E8(); LD8(30,features+INPUT_STRIDE);
    int r=1;
    B8_ROW(16,17,30,31, 4, 5,1,0,0,1,0,0);
    B8_ROW(18,19,31,30, 6, 7,1,0,0,1,0,0);
    B8_ROW(20,21,30,31, 8, 9,1,0,0,1,0,0);
    B8_ROW(22,23,31,30,10,11,1,0,0,1,0,0);
    B8_ROW(24,25,30,31,12,13,1,0,0,1,0,0);
    B8_ROW(26,27,31,30,14,15,1,0,0,1,0,0);
    B8_ROW(28,29,30,31,30,31,0,0,0,1,0,0);
    if (rows==8 && last_chunk) prepare_output(work);
#if RESNET_CHUNK_OVERLAP
    if (rows>8) {
        // Prepare the first ordinary group while the peeled group finishes.
        b8_low=out+r*16; b8_high=b8_low+rows*16;
        b8_lookahead=features+(r+1)*INPUT_STRIDE;
        asm volatile("" :: "r"(b8_low), "r"(b8_high), "r"(b8_lookahead));
        CSR(0x7d4,0);
        E32(); B8_WAIT_SPATZ();
        // Release v14 for the next feature, and store only the pairs that
        // the next DIMC/old-psum loads overwrite first. Carry the other six
        // pairs into the following B8 rows (STORE=2 uses the peeled map).
        ADD(27,15); ADD(26,14);
        E8(); LD8(14,features+r*INPUT_STRIDE); E32();
        ADD(1,3); ADD(0,2); ADD(17,5); ADD(16,4);
        B8_STORE_PAIR(0,1,0); B8_STORE_PAIR(16,17,1);
        ADD(29,31); ADD(28,30); ADD(25,13); ADD(24,12);
        ADD(23,11); ADD(22,10); ADD(21,9); ADD(20,8);
        ADD(19,7); ADD(18,6);
        E8();
    } else
#endif
    {
    E32(); B8_WAIT_SPATZ();
    ADD(29,31); ADD(28,30); ADD(27,15); ADD(26,14);
    ADD(25,13); ADD(24,12); ADD(23,11); ADD(22,10);
    ADD(21,9); ADD(20,8); ADD(19,7); ADD(18,6);
    ADD(17,5); ADD(16,4); ADD(1,3); ADD(0,2);
    // Retire the peeled group before reusing v0/v1 for ordinary old partials.
    B8_STORE_PAIR(0,1,0); B8_STORE_PAIR(16,17,1);
    B8_STORE_PAIR(18,19,2); B8_STORE_PAIR(20,21,3);
    B8_STORE_PAIR(22,23,4); B8_STORE_PAIR(24,25,5);
    B8_STORE_PAIR(26,27,6); B8_STORE_PAIR(28,29,7);
    B8_DRAIN(out);

    }

#if OUT_ROWS % 8 == 0 && TILE_ROWS % 8 == 0 && RESNET_RUN_ROWS % 8 == 0
    int full_rows=rows;
#else
    int full_rows=rows/8*8;
#endif
    // Peel startup so READY and STORE are compile-time constants in the loop.
    // A final incomplete group is never padded or included in a B8 phase.
    b8_more=r+8<full_rows;
    if (b8_more) {
        B8_GROUP(0,RESNET_CHUNK_OVERLAP,RESNET_CHUNK_OVERLAP?2:0);
        while (b8_more) { B8_GROUP(0,1,1); }
        if (full_rows==rows) { B8_GROUP(1,1,1); }
        else { B8_GROUP(0,1,1); }
    } else if (r<full_rows) {
        if (full_rows==rows) { B8_GROUP(1,RESNET_CHUNK_OVERLAP,RESNET_CHUNK_OVERLAP?2:0); }
        else { B8_GROUP(0,RESNET_CHUNK_OVERLAP,RESNET_CHUNK_OVERLAP?2:0); }
    }
    // Each regular group stored its first pair. Flush its remaining seven
    // pairs before a K transition or publishing the completed output DMA.
    if (full_rows>8 && (!RESNET_CHUNK_OVERLAP || !next_kernel || full_rows!=rows)) {
        E32();
        B8_STORE_PAIR(18,19,full_rows-7); B8_STORE_PAIR(20,21,full_rows-6);
        B8_STORE_PAIR(22,23,full_rows-5); B8_STORE_PAIR(24,25,full_rows-4);
        B8_STORE_PAIR(26,27,full_rows-3); B8_STORE_PAIR(28,29,full_rows-2);
        B8_STORE_PAIR(30,31,full_rows-1);
        B8_DRAIN(out+rows*16+full_rows*16-1);
    }
#if OUT_ROWS % 8 != 0 || TILE_ROWS % 8 != 0 || RESNET_RUN_ROWS % 8 != 0
    // Drain the complete group before reusing its result registers for the
    // final 1-7 positions. No extra rows or feature values are read.
    if (r<rows) {
        int tail=rows-r, group=r;
        // Ordinary full groups prefetch this row during their E32 additions.
        // The bootstrap-only case has no such prefetch.
        E8();
        if (full_rows==8) { LD8(14,features+r*INPUT_STRIDE); }
        if (tail>0) { B8_ROW(16,17,14,15,0,1,tail>1,0,0,1,1,0); }
        if (tail>1) { B8_ROW(18,19,15,14,2,3,tail>2,0,0,1,1,1); }
        if (tail>2) { B8_ROW(20,21,14,15,4,5,tail>3,0,0,1,1,2); }
        if (tail>3) { B8_ROW(22,23,15,14,6,7,tail>4,0,0,1,1,3); }
        if (tail>4) { B8_ROW(24,25,14,15,8,9,tail>5,0,0,1,1,4); }
        if (tail>5) { B8_ROW(26,27,15,14,10,11,tail>6,0,0,1,1,5); }
        if (tail>6) { B8_ROW(28,29,14,15,12,13,tail>7,0,0,1,1,6); }
#if RESNET_CHUNK_OVERLAP && RESNET_RUN_ROWS % 8 != 0
        if (next_kernel) {
            const uint8_t *next_feature=next->features;
            uint32_t *store_low=out+group*16, *store_high=store_low+rows*16;
            asm volatile("" :: "r"(next_kernel), "r"(next_feature),
                               "r"(store_low), "r"(store_high));
            // Unused old-partial registers can stage the next kernel while
            // the short group finishes. v31 is free in these exact tails.
            E8();
            for (int pair=tail;pair<8;++pair) prefetch_pair(pair,next_kernel);
            LD8(31,next_feature);
            E32(); B8_WAIT_SPATZ();
            chunk_finish_tail(next_kernel,next_feature,store_low,store_high);
            pipeline.feature_ready=2;
            return;
        }
#endif
        E32(); B8_WAIT_SPATZ();
        if (tail>6) { B8_ADD_PAIR(28,29,12,13,6,1); }
        if (tail>5) { B8_ADD_PAIR(26,27,10,11,5,1); }
        if (tail>4) { B8_ADD_PAIR(24,25,8,9,4,1); }
        if (tail>3) { B8_ADD_PAIR(22,23,6,7,3,1); }
        if (tail>2) { B8_ADD_PAIR(20,21,4,5,2,1); }
        if (tail>1) { B8_ADD_PAIR(18,19,2,3,1,1); }
        if (tail>0) { B8_ADD_PAIR(16,17,0,1,0,1); }
        if (tail>0) { B8_STORE_PAIR(16,17,group+0); }
        if (tail>1) { B8_STORE_PAIR(18,19,group+1); }
        if (tail>2) { B8_STORE_PAIR(20,21,group+2); }
        if (tail>3) { B8_STORE_PAIR(22,23,group+3); }
        if (tail>4) { B8_STORE_PAIR(24,25,group+4); }
        if (tail>5) { B8_STORE_PAIR(26,27,group+5); }
        if (tail>6) { B8_STORE_PAIR(28,29,group+6); }
        B8_DRAIN(out+rows*32-1);
        // These registers held no old partials in the short group. Complete
        // the next kernel staging before advertising it as prefetched.
        if (next_kernel) {
            E8();
            for (int pair=tail;pair<8;++pair) prefetch_pair(pair,next_kernel);
        }
    }
#endif
}

#undef B8_FIRST_ROW
#undef B8_GROUP
#undef B8_ADD_PAIR
#undef B8_ROW
#undef B8_STORE_PAIR
#undef B8_DRAIN
#undef B8_WAIT_SPATZ
