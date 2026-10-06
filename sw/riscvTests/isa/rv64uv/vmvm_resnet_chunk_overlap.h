// SPDX-License-Identifier: SHL-0.51
// Shared K-chunk handoff. v0-v15 hold the prefetched kernel, v31 the next
// feature. Previous results v16-v29 survive until the stores below read them.
// v30/v31 were already stored in the preceding B8 addition phase.
#define CHUNK_STORE_PAIR(L,H) \
    "vse32.v v" S(L) ", (%[low])\n" \
    "vse32.v v" S(H) ", (%[high])\n" \
    "addi %[low], %[low], 64\n" \
    "addi %[high], %[high], 64\n"
#if RESIDENT_INPUTS
// Resident kernel rows are 256/640 bytes apart. Increment a cursor so every
// address update fits addi, including rows whose base offset exceeds 2047.
#define CHUNK_KERNEL_FIRST "mv %[addr], %[kernel]\n" "vle8.v v16, (%[addr])\n"
#define CHUNK_KERNEL(V,OFF) \
    "addi %[addr], %[addr], " S(INPUT_STRIDE) "\n" \
    "vle8.v v" S(V) ", (%[addr])\n"
#else
#define CHUNK_KERNEL(V,OFF) \
    "addi %[addr], %[kernel], " S(OFF) "\n" \
    "vle8.v v" S(V) ", (%[addr])\n"
#define CHUNK_KERNEL_FIRST CHUNK_KERNEL(16,0)
#endif
#define CHUNK_E8 "li %[width], 128\n" "vsetvli zero, %[width], e8, m1, ta, ma\n"
#define CHUNK_E32 "li %[width], 16\n" "vsetvli zero, %[width], e32, m1, ta, ma\n"

#define CHUNK_BOOTSTRAP(STORES_FIRST,STORES_LAST) \
".word 0xb83f8077\n" \
"csrw 0x7d4, %[one]\n" \
".word 0xb8ff8077\n" \
"beqz %[low], 1f\n" \
CHUNK_E32 \
STORES_FIRST \
"1:\n" \
CHUNK_E8 \
CHUNK_KERNEL_FIRST CHUNK_KERNEL(17,128) CHUNK_KERNEL(18,256) CHUNK_KERNEL(19,384) \
CHUNK_KERNEL(20,512) CHUNK_KERNEL(21,640) CHUNK_KERNEL(22,768) CHUNK_KERNEL(23,896) \
"vle8.v v2, (%[feature])\n" \
/* Kernel-load CSR auto-clears after each pair of instructions. */ \
"csrw 0x7d3, %[one]\n" \
"csrw 0x7d4, zero\n" \
".word 0xb93100f7\n" \
"beqz %[low], 2f\n" \
CHUNK_E32 \
STORES_LAST \
"2:\n" \
CHUNK_E8 \
CHUNK_KERNEL(24,1024) CHUNK_KERNEL(25,1152) CHUNK_KERNEL(26,1280) CHUNK_KERNEL(27,1408) \
CHUNK_KERNEL(28,1536) CHUNK_KERNEL(29,1664) CHUNK_KERNEL(30,1792) CHUNK_KERNEL(31,1920) \
"csrw 0x7d4, %[one]\n" \
".word 0xb9f100f7\n" \
"csrw 0x7d3, zero\n" \
"csrw 0x7d4, zero\n"

#define CHUNK_BOOTSTRAP_BODY CHUNK_BOOTSTRAP( \
    CHUNK_STORE_PAIR(16,17) CHUNK_STORE_PAIR(18,19) CHUNK_STORE_PAIR(20,21) CHUNK_STORE_PAIR(22,23), \
    CHUNK_STORE_PAIR(24,25) CHUNK_STORE_PAIR(26,27) CHUNK_STORE_PAIR(28,29))

static __attribute__((always_inline)) inline void chunk_bootstrap_prepared(
    const uint8_t *kernel, const uint8_t *feature, int prefetched,
    uint32_t *low, uint32_t *high, int feature_ready) {
    uint32_t *drain=low;
    E8(); CSR(0x7d4,0); CSR(0x7d3,1);
    if (!prefetched) {
        LD8(0,kernel+0*INPUT_STRIDE); LD8(1,kernel+1*INPUT_STRIDE);
        LD8(2,kernel+2*INPUT_STRIDE); LD8(3,kernel+3*INPUT_STRIDE);
        LD8(4,kernel+4*INPUT_STRIDE); LD8(5,kernel+5*INPUT_STRIDE);
        LD8(6,kernel+6*INPUT_STRIDE); LD8(7,kernel+7*INPUT_STRIDE);
        LD8(8,kernel+8*INPUT_STRIDE); LD8(9,kernel+9*INPUT_STRIDE);
        LD8(10,kernel+10*INPUT_STRIDE); LD8(11,kernel+11*INPUT_STRIDE);
        LD8(12,kernel+12*INPUT_STRIDE); LD8(13,kernel+13*INPUT_STRIDE);
        LD8(14,kernel+14*INPUT_STRIDE); LD8(15,kernel+15*INPUT_STRIDE);
    }
    if (!feature_ready) LD8(31,feature);
    uint32_t addr, width;
    // Keep the handoff in one assembly block: scalar stack loads between
    // vector stores would force Snitch's global vector-store drain interlock.
    // VLSU and DIMC scoreboards retain each instruction's E8/E32 configuration
    // and protect result-store -> kernel-load WAR dependencies.
    asm volatile(
        CHUNK_BOOTSTRAP_BODY
        : [low] "+&r"(low), [high] "+&r"(high),
          [addr] "=&r"(addr), [width] "=&r"(width)
        : [kernel] "r"(kernel+16*INPUT_STRIDE), [feature] "r"(feature), [one] "r"(1)
        : "memory");
    // A completed output block may now be published to output DMA. This
    // barrier is after new DIMC work was queued, not before its first launch.
    if (drain) asm volatile("lw zero, 0(%0)" :: "r"(drain) : "memory");
}

// Only the last complete B8 group carries stores across a chunk boundary.
// A final layer chunk and an eight-row bootstrap-only tile flush normally.
#define CHUNK_PREPARE_CARRY(TAIL) \
    const uint8_t *chunk_next_feature=(TAIL) && next_kernel ? next->features : 0; \
    asm volatile("" :: "r"(chunk_next_feature), "r"(store_low), "r"(store_high))
#define CHUNK_FREE_FEATURE_REGISTER() do { \
    if (next_kernel) { \
        uint32_t width; \
        asm volatile( \
            "vse32.v v30, (%[low])\n" \
            "vse32.v v31, (%[high])\n" \
            CHUNK_E8 \
            "vle8.v v31, (%[feature])\n" \
            CHUNK_E32 \
            : [width] "=&r"(width) \
            : [low] "r"(out+(rows-1)*16), \
              [high] "r"(out+(2*rows-1)*16), [feature] "r"(chunk_next_feature) \
            : "memory"); \
    } \
} while (0)

// Keep the last additions, kernel prefetch, and next launch together. All
// operands are prepared before the B8 completion wait, so no scalar metadata
// reload or loop branch can interrupt this handoff.
#define CHUNK_LOW_KERNEL(V,OFF) \
    "addi %[addr], %[kernel_low], " S(OFF) "\n" \
    "vle8.v v" S(V) ", (%[addr])\n"
#if RESIDENT_INPUTS
#define CHUNK_LOW_BASE(kernel) ((kernel)+17*INPUT_STRIDE)
#define CHUNK_LOW_INIT "mv %[addr], %[kernel_low]\n"
#define CHUNK_STORE_ADDR "%[width]"
#define CHUNK_PREFETCH_PAIR(OL,OH,LOFF,HOFF) \
    "addi %[addr], %[addr], -3*" S(INPUT_STRIDE) "\n" \
    "vle8.v v" S(OL) ", (%[addr])\n" \
    "addi %[addr], %[addr], " S(INPUT_STRIDE) "\n" \
    "vle8.v v" S(OH) ", (%[addr])\n"
#else
#define CHUNK_LOW_BASE(kernel) (kernel)
#define CHUNK_LOW_INIT ""
#define CHUNK_STORE_ADDR "%[addr]"
#define CHUNK_PREFETCH_PAIR(OL,OH,LOFF,HOFF) CHUNK_LOW_KERNEL(OL,LOFF) CHUNK_LOW_KERNEL(OH,HOFF)
#endif
#define CHUNK_ADD_PREFETCH(L,H,OL,OH,LOFF,HOFF) \
    CHUNK_E32 \
    "vadd.vv v" S(H) ", v" S(H) ", v" S(OH) "\n" \
    "vadd.vv v" S(L) ", v" S(L) ", v" S(OL) "\n" \
    CHUNK_E8 CHUNK_PREFETCH_PAIR(OL,OH,LOFF,HOFF)

static __attribute__((always_inline)) inline void chunk_finish_b8(
    const uint8_t *kernel, const uint8_t *feature,
    uint32_t *low, uint32_t *high) {
    uint32_t *drain=low;
    uint32_t addr,width;
    asm volatile(
        CHUNK_LOW_INIT
        CHUNK_ADD_PREFETCH(30,31,14,15,1792,1920)
        CHUNK_E32
        "addi " CHUNK_STORE_ADDR ", %[low], 448\n"
        "vse32.v v30, (" CHUNK_STORE_ADDR ")\n"
        "addi " CHUNK_STORE_ADDR ", %[high], 448\n"
        "vse32.v v31, (" CHUNK_STORE_ADDR ")\n"
        CHUNK_E8
        "vle8.v v31, (%[feature])\n"
        CHUNK_ADD_PREFETCH(28,29,12,13,1536,1664)
        CHUNK_ADD_PREFETCH(26,27,10,11,1280,1408)
        CHUNK_ADD_PREFETCH(24,25,8,9,1024,1152)
        CHUNK_ADD_PREFETCH(22,23,6,7,768,896)
        CHUNK_ADD_PREFETCH(20,21,4,5,512,640)
        CHUNK_ADD_PREFETCH(18,19,2,3,256,384)
        CHUNK_ADD_PREFETCH(16,17,0,1,0,128)
        "csrw 0x7d4, zero\n"
        "csrw 0x7d3, %[one]\n"
        CHUNK_BOOTSTRAP_BODY
        : [low] "+&r"(low), [high] "+&r"(high),
          [addr] "=&r"(addr), [width] "=&r"(width)
        : [kernel] "r"(kernel+16*INPUT_STRIDE), [kernel_low] "r"(CHUNK_LOW_BASE(kernel)),
          [feature] "r"(feature), [one] "r"(1)
        : "memory");
    asm volatile("lw zero, 0(%0)" :: "r"(drain) : "memory");
}
#define CHUNK_TRY_FINISH(TAIL) ((TAIL) && next_kernel ? \
    (chunk_finish_b8(next_kernel,chunk_next_feature,store_low,store_high), \
     pipeline.feature_ready=2, 1) : 0)

#if RESNET_RUN_ROWS % 8 != 0
// Conv4/Conv5 end in four/one positions. Carry exactly those result pairs;
// never issue/store padded output rows. Their kernels use packed K128 rows.
_Static_assert(!RESIDENT_INPUTS && OUT_ROWS<=TILE_ROWS,
               "Short handoff requires a single spatial tile");
#if RESNET_RUN_ROWS % 8 == 4
#define CHUNK_TAIL_ADDS \
    CHUNK_ADD_PREFETCH(22,23,6,7,768,896) \
    CHUNK_ADD_PREFETCH(20,21,4,5,512,640) \
    CHUNK_ADD_PREFETCH(18,19,2,3,256,384) \
    CHUNK_ADD_PREFETCH(16,17,0,1,0,128)
#define CHUNK_TAIL_STORES CHUNK_STORE_PAIR(16,17) CHUNK_STORE_PAIR(18,19) \
    CHUNK_STORE_PAIR(20,21) CHUNK_STORE_PAIR(22,23)
#elif RESNET_RUN_ROWS % 8 == 1
#define CHUNK_TAIL_ADDS CHUNK_ADD_PREFETCH(16,17,0,1,0,128)
#define CHUNK_TAIL_STORES CHUNK_STORE_PAIR(16,17)
#else
#error "Add an exact register map before enabling another short-tail shape"
#endif
static __attribute__((always_inline)) inline void chunk_finish_tail(
    const uint8_t *kernel, const uint8_t *feature,
    uint32_t *low, uint32_t *high) {
    uint32_t *drain=low;
    uint32_t addr,width;
    asm volatile(
        CHUNK_TAIL_ADDS
        "csrw 0x7d4, zero\n"
        "csrw 0x7d3, %[one]\n"
        CHUNK_BOOTSTRAP(CHUNK_TAIL_STORES, "")
        : [low] "+&r"(low), [high] "+&r"(high),
          [addr] "=&r"(addr), [width] "=&r"(width)
        : [kernel] "r"(kernel+16*INPUT_STRIDE), [kernel_low] "r"(kernel),
          [feature] "r"(feature), [one] "r"(1)
        : "memory");
    asm volatile("lw zero, 0(%0)" :: "r"(drain) : "memory");
}
#endif
