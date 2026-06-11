#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gemm_vtmmu(int32_t *C,
                const uint8_t *A_u8,
                const uint8_t *B_u8,
                uint32_t M,
                uint32_t N,
                uint32_t K,
                uint32_t TM,
                uint32_t TN,
                int altfmt,
                int use_vtle);

void gemm_vtmms(int32_t *C,
                const int8_t *A_s8,
                const int8_t *B_s8,
                uint32_t M,
                uint32_t N,
                uint32_t K,
                uint32_t TM,
                uint32_t TN,
                int altfmt,
                int use_vtle);

#ifdef __cplusplus
}
#endif