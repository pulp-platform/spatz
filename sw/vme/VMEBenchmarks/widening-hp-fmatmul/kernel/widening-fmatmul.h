// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

enum { VME_WIDENING_TE = 16 };

// Apack[ceil(K/2)][M], Bpack[ceil(K/2)][N], C[M][N].
// Returns zero on success and -1 for a shape unsupported by one tile.
int vme_widening_hp_fmatmul_1tile(float *c, const uint32_t *apack,
                                  const uint32_t *bpack, uint32_t M,
                                  uint32_t N, uint32_t K);

// Two tiles arranged as one M tile by two N tiles. The ping-pong variant
// overlaps mt0 stores with mt4 reduction; the reuse variant updates both
// tiles together and reuses each loaded A vector.
int vme_widening_hp_fmatmul_2tile_pingpong(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K);
int vme_widening_hp_fmatmul_2tile_reuse(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K);

// Four tiles arranged as a 2x2 output panel. The ping-pong variant computes
// and drains one 1x2 pair at a time; the reuse variant updates all four tiles
// together so both A and B vectors are reused.
int vme_widening_hp_fmatmul_4tile_pingpong(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K);
int vme_widening_hp_fmatmul_4tile_reuse(
    float *c, const uint32_t *apack, const uint32_t *bpack, uint32_t M,
    uint32_t N, uint32_t K);

// Shape-agnostic outer tiling. Each output panel is at most 32x32 and selects
// a 1x1, 1x2, 2x1, or 2x2 tile grid from its active M/N extents.
int vme_widening_hp_fmatmul_auto(float *c, const uint32_t *apack,
                                 const uint32_t *bpack, uint32_t M,
                                 uint32_t N, uint32_t K);
