#include "ldpc_decoder_ctx.h"
#include "compute_var_to_check_msgs.h"
#include <snrt.h>
#include <string.h>

void ldpc_init(ldpc_ctx_t *ctx, uint16_t Z, uint16_t max_iterations)
{
    const uint8_t ils = ldpc_ils_of_Z[Z];

    ctx->lifting_size   = Z;
    ctx->bg_M           = BG1_M;
    ctx->bg_K           = BG1_K;
    ctx->bg_N_short     = BG1_N_SHORT;
    ctx->bg_N_high_rate = BG1_N_HIGH_RATE;
    ctx->max_deg        = BG1_MAX_DEG;
    ctx->max_iterations = max_iterations;
    ctx->message_length = BG1_K * Z;
    ctx->min            = (int8_t  *)snrt_l1alloc((size_t)Z);
    ctx->second         = (int8_t  *)snrt_l1alloc((size_t)Z);
    ctx->index          = (uint8_t *)snrt_l1alloc((size_t)Z);
    ctx->sign           = (uint8_t *)snrt_l1alloc((size_t)Z);
    ctx->min_dup        = (int8_t  *)snrt_l1alloc((size_t)2 * Z);
    ctx->second_dup     = (int8_t  *)snrt_l1alloc((size_t)2 * Z);
    ctx->index_dup      = (uint8_t *)snrt_l1alloc((size_t)2 * Z);
    ctx->sign_dup       = (uint8_t *)snrt_l1alloc((size_t)2 * Z);
    ctx->c2v_init       = (uint8_t *)snrt_l1alloc((size_t)ctx->bg_M);
    ctx->shift          = (uint16_t *)snrt_l1alloc((size_t)ctx->bg_M*ctx->max_deg * sizeof(uint16_t));
    ctx->bg1_adjacency_l1  = (uint16_t *)snrt_l1alloc((size_t)ctx->bg_M*ctx->max_deg * sizeof(uint16_t));
    
    ctx->offset[0] = 0;
     for (unsigned m = 0; m < BG1_M; ++m) {
        unsigned d = bg1_row_deg[m];
        ctx->offset[m + 1] = ctx->offset[m] + d;
        for (unsigned k = 0; k < d; ++k)
            ctx->shift[m*ctx->max_deg + k] = (uint16_t)(bg1_V[ils][m][k] % Z);
    }
    ctx->total_edges = ctx->offset[BG1_M];
    snrt_dma_start_1d(ctx->bg1_adjacency_l1, bg1_adjacency, (size_t)ctx->bg_M*ctx->max_deg * sizeof(uint16_t));
    snrt_dma_wait_all();
    // L1
    ctx->soft = (int8_t  *)snrt_l1alloc((size_t)BG1_N_FULL  * Z);
    ctx->v2c  = (int8_t  *)snrt_l1alloc((size_t)(ctx->bg_N_high_rate + 1) * 2 * Z);
    ctx->c2v  = (int8_t  *)snrt_l1alloc((size_t)ctx->total_edges * Z);

    //ctx->vidx = (uint16_t*)snrt_l1alloc((size_t)ctx->max_deg * Z * sizeof(uint16_t));

    memset(ctx->c2v, 0, (size_t)ctx->total_edges * Z);
}