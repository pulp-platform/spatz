#pragma once
#include <stdint.h>

void dq_matmul_MT8x2(const uint32_t *apack, const uint32_t *idx,
                     const uint32_t *cb0, const uint32_t *cb1, float *output,
                     uint32_t M, uint32_t N, uint32_t *decoded_workspace);
