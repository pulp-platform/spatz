#include "ldpc_decoder_ctx.h"
#include <stddef.h>

void build_vidx(ldpc_ctx_t *ctx, unsigned m)
{
    const uint16_t Z = ctx->lifting_size;
    const unsigned d = bg1_row_deg[m];

    for (unsigned seg = 0; seg < d; ++seg) {
        uint32_t  base = (uint32_t)bg1_adjacency[m][seg] * Z;
        uint16_t *dst  = ctx->vidx + (size_t)seg * Z;
        for (unsigned j = 0; j < Z; ++j)
            dst[j] = (uint16_t)(base + j);
    }
}