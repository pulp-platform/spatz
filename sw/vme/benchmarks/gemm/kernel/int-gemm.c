
#include <stdint.h>
#include <string.h>
#include <printf.h>

#include "vector_macros.h"
#include "vector_matrix_macros.h"
#include "int-gemm.h"

#ifndef VTMMU_INSN
#define VTMMU_INSN "vtmmu.tvv"
#endif

#ifndef VTMMS_INSN
#define VTMMS_INSN "vtmms.tvv"
#endif

#ifndef VTMMU_ALT_INSN
#define VTMMU_ALT_INSN "vtmmu.alt.tvv"
#endif

#ifndef VTMMS_ALT_INSN
#define VTMMS_ALT_INSN "vtmms.alt.tvv"
#endif

#ifndef VTMM_ALT_USES_ALT_INSN
#define VTMM_ALT_USES_ALT_INSN 1
#endif

#define STORE_4TILES_I32(c00, c01, c10, c11) do {                              \
    for (int _r = 0; _r < (int)TM; _r++) {                                     \
        uintptr_t _t0  = TSS_ROW( 0,_r), _t4  = TSS_ROW( 4,_r);               \
        uintptr_t _t8  = TSS_ROW( 8,_r), _t12 = TSS_ROW(12,_r);               \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t0),  "r"((c00)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t4),  "r"((c01)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t8),  "r"((c10)+_r*N) : "memory"); \
        asm volatile("vtse32 %0, (%1)" :: "r"(_t12), "r"((c11)+_r*N) : "memory"); \
    }                                                                           \
} while (0)

#define LOAD_4TILES_I32(c00, c01, c10, c11) do {                               \
    for (int _r = 0; _r < (int)TM; _r++) {                                     \
        uintptr_t _t0  = TSS_ROW( 0,_r), _t4  = TSS_ROW( 4,_r);               \
        uintptr_t _t8  = TSS_ROW( 8,_r), _t12 = TSS_ROW(12,_r);               \
        asm volatile("vtle32 %0, (%1)" :: "r"(_t0),  "r"((c00)+_r*N) : "memory"); \
        asm volatile("vtle32 %0, (%1)" :: "r"(_t4),  "r"((c01)+_r*N) : "memory"); \
        asm volatile("vtle32 %0, (%1)" :: "r"(_t8),  "r"((c10)+_r*N) : "memory"); \
        asm volatile("vtle32 %0, (%1)" :: "r"(_t12), "r"((c11)+_r*N) : "memory"); \
    }                                                                           \
} while (0)

#define ZERO_4TILES() do {                                                      \
    asm volatile("vtzero mt0"  ::: "memory");                                  \
    asm volatile("vtzero mt4"  ::: "memory");                                  \
    asm volatile("vtzero mt8"  ::: "memory");                                  \
    asm volatile("vtzero mt12" ::: "memory");                                  \
} while (0)

#define VLSE8(dst, base, stride_bytes) \
    asm volatile("vlse8.v " dst ", (%0), %1" :: "r"(base), "r"(stride_bytes) : dst, "memory")

#define VLE8(dst, base) \
    asm volatile("vle8.v " dst ", (%0)" :: "r"(base) : dst, "memory")

#define VTMM_GROUP(INSN) do {                                                   \
    asm volatile(INSN " mt0,  v8, v16"  ::: "memory");                         \
    asm volatile(INSN " mt4,  v8, v17"  ::: "memory");                         \
    asm volatile(INSN " mt8,  v9, v16"  ::: "memory");                         \
    asm volatile(INSN " mt12, v9, v17"  ::: "memory");                         \
} while (0)

#define INNER_LOOP_INT8(A8, B8, INSN) do {                             \
    const uintptr_t _a_stride = (uintptr_t)(K * sizeof(uint8_t));               \
    for (int _k = 0; _k < (int)K; _k += 4) {                                    \
        const uint8_t *_a0 = (const uint8_t *)(A8) + ti*(int)TM*(int)K + _k;    \
        const uint8_t *_a1 = (const uint8_t *)(A8) + (ti+1)*(int)TM*(int)K + _k;\
        const uint8_t *_b0 = (const uint8_t *)(B8) + _k*(int)N + tj*(int)TN;    \
        const uint8_t *_b1 = (const uint8_t *)(B8) + _k*(int)N + (tj+1)*(int)TN;\
        VLSE8("v8",  _a0,     _a_stride);                                      \
        VLSE8("v10", _a0 + 1, _a_stride);                                      \
        VLSE8("v12", _a0 + 2, _a_stride);                                      \
        VLSE8("v14", _a0 + 3, _a_stride);                                      \
        VLSE8("v9",  _a1,     _a_stride);                                      \
        VLSE8("v11", _a1 + 1, _a_stride);                                      \
        VLSE8("v13", _a1 + 2, _a_stride);                                      \
        VLSE8("v15", _a1 + 3, _a_stride);                                      \
        VLE8("v16", _b0);                                                      \
        VLE8("v18", _b0 + (int)N);                                             \
        VLE8("v20", _b0 + 2*(int)N);                                           \
        VLE8("v22", _b0 + 3*(int)N);                                           \
        VLE8("v17", _b1);                                                      \
        VLE8("v19", _b1 + (int)N);                                             \
        VLE8("v21", _b1 + 2*(int)N);                                           \
        VLE8("v23", _b1 + 3*(int)N);                                           \
        VTMM_GROUP(INSN);                                                       \
    }                                                                           \
} while (0)

void gemm_vtmmu(int32_t *C,
                const uint8_t *A_u8,
                const uint8_t *B_u8,
                uint32_t M,
                uint32_t N,
                uint32_t K,
                uint32_t TM,
                uint32_t TN,
                int altfmt,
                int use_vtle)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma"
                 :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);

    if (altfmt) set_altfmt();
    else        clear_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            int32_t *c00 = C + ti*(int)TM*(int)N + tj*(int)TN;
            int32_t *c01 = C + ti*(int)TM*(int)N + (tj+1)*(int)TN;
            int32_t *c10 = C + (ti+1)*(int)TM*(int)N + tj*(int)TN;
            int32_t *c11 = C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN;

            if (use_vtle) LOAD_4TILES_I32(c00, c01, c10, c11);
            else          ZERO_4TILES();

            INNER_LOOP_INT8(A_u8, B_u8, "vtmmu.tvv");
            
            STORE_4TILES_I32(c00, c01, c10, c11);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}

void gemm_vtmms(int32_t *C,
                const int8_t *A_s8,
                const int8_t *B_i8,
                uint32_t M,
                uint32_t N,
                uint32_t K,
                uint32_t TM,
                uint32_t TN,
                int altfmt,
                int use_vtle)
{
    asm volatile("vsetvli zero, %0, e8, m1, ta, ma"
                 :: "r"((uintptr_t)TM) : "memory");
    vme_cfg_e8(TN, TM, 4);

    if (altfmt) set_altfmt();
    else        clear_altfmt();

    for (int ti = 0; ti < (int)(M / TM); ti += 2) {
        for (int tj = 0; tj < (int)(N / TN); tj += 2) {
            int32_t *c00 = C + ti*(int)TM*(int)N + tj*(int)TN;
            int32_t *c01 = C + ti*(int)TM*(int)N + (tj+1)*(int)TN;
            int32_t *c10 = C + (ti+1)*(int)TM*(int)N + tj*(int)TN;
            int32_t *c11 = C + (ti+1)*(int)TM*(int)N + (tj+1)*(int)TN;

            if (use_vtle) LOAD_4TILES_I32(c00, c01, c10, c11);
            else          ZERO_4TILES();

            INNER_LOOP_INT8(A_s8, B_i8, "vtmms.tvv");

            STORE_4TILES_I32(c00, c01, c10, c11);
        }
    }

    clear_altfmt();
    asm volatile("vtdiscard" ::: "memory");
}


#undef INNER_LOOP_INT8
#undef VTMM_GROUP
#undef VLE8
#undef VLSE8
#undef ZERO_4TILES
#undef LOAD_4TILES_I32
#undef STORE_4TILES_I32