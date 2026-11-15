#pragma once

#include <stdint.h>

void vq_gemm_MT6x2(const uint32_t *apack, uint32_t *workspace,
                   const uint32_t *idx, const uint32_t *cb0,
                   const uint32_t *cb1, float *output,
                   uint32_t M, uint32_t N, uint32_t K);
