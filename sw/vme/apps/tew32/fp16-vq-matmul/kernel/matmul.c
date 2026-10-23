#include "matmul.h"
#define MATRIX_CONFIG \
    "msetmtype %[mt], %[vt]\n" "msettn zero, %[tn]\n"
#define MATRIX_INPUTS \
    [mt] "r"((TE << 10) | (2u << 5) | 2u), [vt] "r"(0xC8), [tn] "r"(TE)

__attribute__((noinline, aligned(64)))
void matmul_fp16_fp32_vq_1x2(const __fp16 *Atpack, const vq_group *Bmeta,
                           const __fp16 *cb0, const __fp16 *cb1, float *C) {
    _Static_assert(K_CHUNK % 8 == 0, "K must be a multiple of eight");
    const uint32_t *a = (const uint32_t *)Atpack;
    enum { K_GROUPS = K_CHUNK / 8 };

    for (unsigned mb = 0; mb < 4; ++mb) {
        const uint32_t *ab = a + mb * TE * (K_CHUNK / 2);
        for (unsigned nb = 0; nb < 4; nb += 2) {
            const vq_group *meta0 = Bmeta + nb * K_GROUPS;
            const vq_group *meta1 = meta0 + K_GROUPS;

            // Preload A and the two raw codebook vectors for mt0.
            asm volatile(
                MATRIX_CONFIG
                "vtzero mt0\n"
                "vtzero mt4\n"
                "vsetvli zero, %[vl32], e32, m4, ta, ma\n"
                "vle32.v v0, (%[ap])\n"
                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                "vle16.v v8, (%[i0])\n"
                "vle16.v v12, (%[j0])\n"
                "vluxei16.v v16, (%[cb0]), v8\n"
                "vluxei16.v v20, (%[cb1]), v12\n"
                :
                : [ap] "r"(ab), [i0] "r"(meta0[0].off0),
                  [j0] "r"(meta0[0].off1),
                  [cb0] "r"(cb0), [cb1] "r"(cb1),
                  [vl32] "r"(64), [vl16] "r"(128), MATRIX_INPUTS
                : "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11",
                  "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
                  "v20", "v21", "v22", "v23", "memory");

            #pragma clang loop unroll(disable)
            for (unsigned kg = 0; kg + 1 < K_GROUPS; ++kg) {
                const vq_group *next0 = meta0 + kg + 1;
                const vq_group *curr1 = meta1 + kg;
                const vq_group *curr0 = meta0 + kg;
                asm volatile(
                    // Finish mt0 preprocessing while preloading mt4 indices.
                    "vle16.v v8, (%[i1])\n"
                    "vle16.v v12, (%[j1])\n"
                    "vfadd.vv v16, v16, v20\n"
                    "vle16.v v28, (%[s0])\n"
                    "vluxei16.v v24, (%[cb0]), v8\n"
                    "vfmul.vv v16, v28, v16\n"
                    "vluxei16.v v28, (%[cb1]), v12\n"
                    MATRIX_CONFIG
                    "vtfmm.tvv mt0, v0, v16\n"
                    "vtfmm.tvv mt0, v1, v17\n"
                    "vtfmm.tvv mt0, v2, v18\n"
                    "vtfmm.tvv mt0, v3, v19\n"
                    // Finish mt4 preprocessing while gathering the next mt0.
                    "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                    "vle16.v v8, (%[ni0])\n"
                    "vle16.v v12, (%[nj0])\n"
                    "vfadd.vv v24, v24, v28\n"
                    "vle16.v v20, (%[s1])\n"
                    "vluxei16.v v16, (%[cb0]), v8\n"
                    "vfmul.vv v24, v20, v24\n"
                    "vluxei16.v v20, (%[cb1]), v12\n"
                    MATRIX_CONFIG
                    "vtfmm.tvv mt4, v0, v24\n"
                    "vtfmm.tvv mt4, v1, v25\n"
                    "vtfmm.tvv mt4, v2, v26\n"
                    "vtfmm.tvv mt4, v3, v27\n"
                    // The next raw mt0 vectors are resident; preload only A.
                    "vsetvli zero, %[vl32], e32, m4, ta, ma\n"
                    "vle32.v v0, (%[nap])\n"
                    "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                    :
                    : [i1] "r"(curr1->off0), [j1] "r"(curr1->off1),
                      [s1] "r"(curr1->scale), [ni0] "r"(next0->off0),
                      [nj0] "r"(next0->off1), [s0] "r"(curr0->scale),
                      [nap] "r"(ab + (kg + 1) * 64),
                      [cb0] "r"(cb0), [cb1] "r"(cb1),
                      [vl32] "r"(64), [vl16] "r"(128), MATRIX_INPUTS
                    : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                      "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                      "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                      "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                      "memory");
            }

            // Drain the final raw mt0 group and decode/compute mt4.
            const vq_group *last0 = meta0 + K_GROUPS - 1;
            const vq_group *last1 = meta1 + K_GROUPS - 1;
            asm volatile(
                "vle16.v v8, (%[i1])\n"
                "vle16.v v12, (%[j1])\n"
                "vfadd.vv v16, v16, v20\n"
                "vle16.v v28, (%[s0])\n"
                "vluxei16.v v24, (%[cb0]), v8\n"
                "vfmul.vv v16, v28, v16\n"
                "vluxei16.v v28, (%[cb1]), v12\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt0, v0, v16\n"
                "vtfmm.tvv mt0, v1, v17\n"
                "vtfmm.tvv mt0, v2, v18\n"
                "vtfmm.tvv mt0, v3, v19\n"
                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                "vfadd.vv v24, v24, v28\n"
                "vle16.v v20, (%[s1])\n"
                "vfmul.vv v24, v20, v24\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt4, v0, v24\n"
                "vtfmm.tvv mt4, v1, v25\n"
                "vtfmm.tvv mt4, v2, v26\n"
                "vtfmm.tvv mt4, v3, v27\n"
                :
                : [i1] "r"(last1->off0), [j1] "r"(last1->off1),
                  [s0] "r"(last0->scale), [s1] "r"(last1->scale),
                  [cb0] "r"(cb0), [cb1] "r"(cb1),
                  [vl16] "r"(128), MATRIX_INPUTS
                : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                  "memory");

            for (unsigned tile = 0; tile < 2; ++tile) {
                uintptr_t tss = tile * 0x20000000u;
                uintptr_t out = (uintptr_t)(C + mb * TE * 64 + (nb + tile) * TE);
                for (unsigned row = 0; row < TE; ++row)
                    asm volatile(
                        "vtse32 %0, (%1)\n"
                        "addi %0, %0, 1\n"
                        "addi %1, %1, 256\n"
                        : "+&r"(tss), "+&r"(out) : : "memory");
            }
        }
    }
}

__attribute__((noinline, aligned(64)))
void matmul_fp16_fp32_vq_1x4(const __fp16 *Atpack, const vq_group *Bmeta,
                           const __fp16 *cb0, const __fp16 *cb1, float *C) {
    _Static_assert(K_CHUNK % 8 == 0, "K must be a multiple of eight");
    const uint32_t *a = (const uint32_t *)Atpack;
    enum { K_GROUPS = K_CHUNK / 8 };

    for (unsigned mb = 0; mb < 4; ++mb) {
        const uint32_t *ab = a + mb * TE * (K_CHUNK / 2);
        const vq_group *meta0 = Bmeta;
        const vq_group *meta1 = meta0 + K_GROUPS;
        const vq_group *meta2 = meta1 + K_GROUPS;
        const vq_group *meta3 = meta2 + K_GROUPS;

        asm volatile(
            MATRIX_CONFIG
            "vtzero mt0\n"
            "vtzero mt4\n"
            "vtzero mt8\n"
            "vtzero mt12\n"
            "vsetvli zero, %[vl32], e32, m4, ta, ma\n"
            "vle32.v v0, (%[ap])\n"
            "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
            "vle16.v v8, (%[i0])\n"
            "vle16.v v12, (%[j0])\n"
            "vluxei16.v v16, (%[cb0]), v8\n"
            "vluxei16.v v20, (%[cb1]), v12\n"
            :
            : [ap] "r"(ab), [i0] "r"(meta0[0].off0),
              [j0] "r"(meta0[0].off1), [cb0] "r"(cb0), [cb1] "r"(cb1),
              [vl32] "r"(64), [vl16] "r"(128), MATRIX_INPUTS
            : "v0", "v1", "v2", "v3", "v8", "v9", "v10", "v11",
              "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
              "v20", "v21", "v22", "v23", "memory");

        #pragma clang loop unroll(disable)
        for (unsigned kg = 0; kg + 1 < K_GROUPS; ++kg) {
            const vq_group *curr0 = meta0 + kg;
            const vq_group *curr1 = meta1 + kg;
            const vq_group *curr2 = meta2 + kg;
            const vq_group *curr3 = meta3 + kg;
            const vq_group *next0 = curr0 + 1;
            asm volatile(
                "vle16.v v8, (%[i1])\n"
                "vle16.v v12, (%[j1])\n"
                "vfadd.vv v16, v16, v20\n"
                "vle16.v v28, (%[s0])\n"
                "vluxei16.v v24, (%[cb0]), v8\n"
                "vfmul.vv v16, v28, v16\n"
                "vluxei16.v v28, (%[cb1]), v12\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt0, v0, v16\n"
                "vtfmm.tvv mt0, v1, v17\n"
                "vtfmm.tvv mt0, v2, v18\n"
                "vtfmm.tvv mt0, v3, v19\n"

                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                "vle16.v v8, (%[i2])\n"
                "vle16.v v12, (%[j2])\n"
                "vfadd.vv v24, v24, v28\n"
                "vle16.v v20, (%[s1])\n"
                "vluxei16.v v16, (%[cb0]), v8\n"
                "vfmul.vv v24, v20, v24\n"
                "vluxei16.v v20, (%[cb1]), v12\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt4, v0, v24\n"
                "vtfmm.tvv mt4, v1, v25\n"
                "vtfmm.tvv mt4, v2, v26\n"
                "vtfmm.tvv mt4, v3, v27\n"

                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                "vle16.v v8, (%[i3])\n"
                "vle16.v v12, (%[j3])\n"
                "vfadd.vv v16, v16, v20\n"
                "vle16.v v28, (%[s2])\n"
                "vluxei16.v v24, (%[cb0]), v8\n"
                "vfmul.vv v16, v28, v16\n"
                "vluxei16.v v28, (%[cb1]), v12\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt8, v0, v16\n"
                "vtfmm.tvv mt8, v1, v17\n"
                "vtfmm.tvv mt8, v2, v18\n"
                "vtfmm.tvv mt8, v3, v19\n"

                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                "vle16.v v8, (%[ni0])\n"
                "vle16.v v12, (%[nj0])\n"
                "vfadd.vv v24, v24, v28\n"
                "vle16.v v20, (%[s3])\n"
                "vluxei16.v v16, (%[cb0]), v8\n"
                "vfmul.vv v24, v20, v24\n"
                "vluxei16.v v20, (%[cb1]), v12\n"
                MATRIX_CONFIG
                "vtfmm.tvv mt12, v0, v24\n"
                "vtfmm.tvv mt12, v1, v25\n"
                "vtfmm.tvv mt12, v2, v26\n"
                "vtfmm.tvv mt12, v3, v27\n"
                "vsetvli zero, %[vl32], e32, m4, ta, ma\n"
                "vle32.v v0, (%[nap])\n"
                "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
                :
                : [i1] "r"(curr1->off0), [j1] "r"(curr1->off1),
                  [i2] "r"(curr2->off0), [j2] "r"(curr2->off1),
                  [i3] "r"(curr3->off0), [j3] "r"(curr3->off1),
                  [ni0] "r"(next0->off0), [nj0] "r"(next0->off1),
                  [s0] "r"(curr0->scale), [s1] "r"(curr1->scale),
                  [s2] "r"(curr2->scale), [s3] "r"(curr3->scale),
                  [nap] "r"(ab + (kg + 1) * 64),
                  [cb0] "r"(cb0), [cb1] "r"(cb1),
                  [vl32] "r"(64), [vl16] "r"(128), MATRIX_INPUTS
                : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
                  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
                  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
                  "memory");
        }

        const vq_group *last0 = meta0 + K_GROUPS - 1;
        const vq_group *last1 = meta1 + K_GROUPS - 1;
        const vq_group *last2 = meta2 + K_GROUPS - 1;
        const vq_group *last3 = meta3 + K_GROUPS - 1;
        asm volatile(
            "vle16.v v8, (%[i1])\n"
            "vle16.v v12, (%[j1])\n"
            "vfadd.vv v16, v16, v20\n"
            "vle16.v v28, (%[s0])\n"
            "vluxei16.v v24, (%[cb0]), v8\n"
            "vfmul.vv v16, v28, v16\n"
            "vluxei16.v v28, (%[cb1]), v12\n"
            MATRIX_CONFIG
            "vtfmm.tvv mt0, v0, v16\n"
            "vtfmm.tvv mt0, v1, v17\n"
            "vtfmm.tvv mt0, v2, v18\n"
            "vtfmm.tvv mt0, v3, v19\n"

            "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
            "vle16.v v8, (%[i2])\n"
            "vle16.v v12, (%[j2])\n"
            "vfadd.vv v24, v24, v28\n"
            "vle16.v v20, (%[s1])\n"
            "vluxei16.v v16, (%[cb0]), v8\n"
            "vfmul.vv v24, v20, v24\n"
            "vluxei16.v v20, (%[cb1]), v12\n"
            MATRIX_CONFIG
            "vtfmm.tvv mt4, v0, v24\n"
            "vtfmm.tvv mt4, v1, v25\n"
            "vtfmm.tvv mt4, v2, v26\n"
            "vtfmm.tvv mt4, v3, v27\n"

            "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
            "vle16.v v8, (%[i3])\n"
            "vle16.v v12, (%[j3])\n"
            "vfadd.vv v16, v16, v20\n"
            "vle16.v v28, (%[s2])\n"
            "vluxei16.v v24, (%[cb0]), v8\n"
            "vfmul.vv v16, v28, v16\n"
            "vluxei16.v v28, (%[cb1]), v12\n"
            MATRIX_CONFIG
            "vtfmm.tvv mt8, v0, v16\n"
            "vtfmm.tvv mt8, v1, v17\n"
            "vtfmm.tvv mt8, v2, v18\n"
            "vtfmm.tvv mt8, v3, v19\n"

            "vsetvli zero, %[vl16], e16, m4, ta, ma\n"
            "vfadd.vv v24, v24, v28\n"
            "vle16.v v20, (%[s3])\n"
            "vfmul.vv v24, v20, v24\n"
            MATRIX_CONFIG
            "vtfmm.tvv mt12, v0, v24\n"
            "vtfmm.tvv mt12, v1, v25\n"
            "vtfmm.tvv mt12, v2, v26\n"
            "vtfmm.tvv mt12, v3, v27\n"
            :
            : [i1] "r"(last1->off0), [j1] "r"(last1->off1),
              [i2] "r"(last2->off0), [j2] "r"(last2->off1),
              [i3] "r"(last3->off0), [j3] "r"(last3->off1),
              [s0] "r"(last0->scale), [s1] "r"(last1->scale),
              [s2] "r"(last2->scale), [s3] "r"(last3->scale),
              [cb0] "r"(cb0), [cb1] "r"(cb1),
              [vl16] "r"(128), MATRIX_INPUTS
            : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
              "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
              "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
              "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
              "memory");

        for (unsigned tile = 0; tile < 4; ++tile) {
            uintptr_t tss = tile * 0x20000000u;
            uintptr_t out = (uintptr_t)(C + mb * TE * 64 + tile * TE);
            for (unsigned row = 0; row < TE; ++row)
                asm volatile(
                    "vtse32 %0, (%1)\n"
                    "addi %0, %0, 1\n"
                    "addi %1, %1, 256\n"
                    : "+&r"(tss), "+&r"(out) : : "memory");
        }
    }
}

