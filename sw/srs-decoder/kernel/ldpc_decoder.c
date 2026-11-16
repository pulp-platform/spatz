#include "ldpc_decoder_ctx.h"
#include <snrt.h>
#include <string.h>
#include <stdio.h>
#include "compute_var_to_check_msgs.h"
#include "compute_check_to_var_msgs.h"
#include "compute_soft_bits.h"
#include "analyze_var_to_check_msgs.h"
#include "scale_llr.h"
#include "load_soft_bits.h"
#include <benchmark.h>

void update_variable_to_check(ldpc_ctx_t *ctx, unsigned m);
void update_check_to_variable_messages(ldpc_ctx_t *ctx, unsigned m);
void update_soft_bits(ldpc_ctx_t *ctx, unsigned m);

void dup_to_2z(uint8_t *dst, const uint8_t *src, uint32_t Z) {
    uint32_t remaining = Z, vl;
    uint8_t *dst2 = dst + Z;
    do {
        asm volatile("vsetvli %0, %1, e8, m4, ta, mu" : "=r"(vl) : "r"(remaining));
        asm volatile("vle8.v v4, (%0)" :: "r"(src)  : "memory");
        asm volatile("vse8.v v4, (%0)" :: "r"(dst)  : "memory");
        asm volatile("vse8.v v4, (%0)" :: "r"(dst2) : "memory");
        src  += vl;
        dst  += vl;
        dst2 += vl;
        remaining -= vl;
    } while (remaining > 0);
}

void reset_accumulators(int8_t* min, int8_t* second, uint8_t* index, uint8_t* sign, uint32_t lifting_size){
  uint32_t remaining = lifting_size;
  uint32_t vl;
  do {
    asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(remaining));
    asm volatile("vmv.v.x v16, %0" :: "r"(0));
    asm volatile("vse8.v v16, (%0)" :: "r"(index));
    asm volatile("vse8.v v16, (%0)" :: "r"(sign));
    asm volatile("vmv.v.x v0, %0" :: "r"(120));
    asm volatile("vse8.v v0, (%0)" :: "r"(min));
    asm volatile("vse8.v v0, (%0)" :: "r"(second));
    index += vl;
    sign += vl;
    min += vl;
    second += vl;
    remaining    -= vl;
  } while (remaining > 0);
}

uint32_t ldpc_decode(ldpc_ctx_t *ctx, unsigned nof_layers){
    memset(ctx->c2v, 0, (size_t)ctx->total_edges * ctx->lifting_size);
    memset(ctx->c2v_init, 0, ctx->bg_M);
    for (uint32_t i = 0; i < ctx->max_iterations; i++){
        for (uint32_t m = 0; m < nof_layers; m++){
            update_variable_to_check(ctx, m);
            update_check_to_variable_messages(ctx, m);
            update_soft_bits(ctx, m);
        }
        //hard decision and crc_calc
    }
    return ctx->max_iterations;
}

void update_variable_to_check(ldpc_ctx_t *ctx, unsigned m)
{
    const uint16_t Z  = ctx->lifting_size;
    const unsigned hr = ctx->bg_N_high_rate;

    int8_t *v2c_base = ctx->v2c;
    int8_t *soft_base = ctx->soft;
    int8_t *c2v_base  = ctx->c2v;
    uint16_t offset_m = ctx->offset[m];
    int8_t *c2v = c2v_base + offset_m * Z;
    uint16_t row_base = m * ctx->max_deg;
    uint16_t *bg1_adjacency_l1_local = ctx->bg1_adjacency_l1;
 
    for (unsigned k = 0; k < ctx->max_deg; ++k) {
        uint16_t node = bg1_adjacency_l1_local[row_base + k];
        if (node == NO_EDGE) break;

        unsigned v2c_node = (node < hr) ? node : hr;
        int8_t *v2c  = v2c_base  + (size_t)v2c_node * 2 * Z;
        int8_t *soft = soft_base + (size_t)node * Z;
        compute_var_to_check_msgs(v2c, soft, c2v, Z);
        c2v += Z;
    }
}

void update_soft_bits(ldpc_ctx_t *ctx, unsigned m)
{
    const uint16_t Z = ctx->lifting_size;
    const unsigned hr = ctx->bg_N_high_rate;
    int8_t *v2c_base = ctx->v2c;
    int8_t *soft_base = ctx->soft;
    int8_t *c2v_base  = ctx->c2v;
    uint16_t offset_m = ctx->offset[m];
    int8_t *c2v = c2v_base + offset_m * Z;
    uint16_t max_deg = ctx->max_deg;
    uint16_t row_base = m * max_deg;
    uint16_t *bg1_adjacency_l1_local = ctx->bg1_adjacency_l1; 

    for (uint32_t k = 0; k < max_deg; k++) {
        uint16_t node = bg1_adjacency_l1_local[row_base + k];
        if (node == NO_EDGE) break;

        unsigned base = (node < hr) ? node : hr;
        int8_t *v2c = v2c_base + (size_t)base * 2 * Z;
        int8_t *soft = soft_base + (size_t)node * Z;
        compute_soft_bits(soft, v2c, c2v, Z);
        c2v += Z;
    }
}

void update_check_to_variable_messages(ldpc_ctx_t *ctx, unsigned m)
{
  const uint16_t Z   = ctx->lifting_size;
  const unsigned hr  = ctx ->bg_N_high_rate;
  uint16_t *adj_row   = ctx->bg1_adjacency_l1 + (size_t)m*ctx->max_deg;
  uint16_t *shift_row = ctx->shift + (size_t)m*ctx->max_deg;
  int8_t *v2c_base   = ctx->v2c;
  int8_t *min        = ctx->min;
  int8_t *second     = ctx->second;
  uint8_t *index     = ctx->index;
  uint8_t *sign      = ctx->sign;
  uint16_t max_deg = ctx->max_deg;

  reset_accumulators(min, second, index, sign, Z);

  unsigned var_node = 0;
  for (unsigned k = 0; k < max_deg; k++) {
    uint16_t node = adj_row[k];
    if (node == NO_EDGE) break;

    unsigned base = (node < hr) ? node : hr;
    uint16_t shift = shift_row[k];
    int8_t *v2c_rot = v2c_base + (size_t)base * 2 * Z + shift;
    analyze_var_to_check_msgs(min, second, index, sign, v2c_rot, var_node, Z);
    var_node++;
  }

  int8_t *c2v = ctx->c2v + (size_t)ctx->offset[m] * Z;
  int8_t *min_dup = ctx->min_dup;
  int8_t *second_dup = ctx->second_dup;
  uint8_t *index_dup = ctx->index_dup;
  uint8_t *sign_dup = ctx->sign_dup;

  scale_llr(min_dup, min, Z);
  scale_llr(second_dup, second, Z);
  dup_to_2z(index_dup, index, Z);
  dup_to_2z(sign_dup, sign, Z);

  var_node = 0;
  for (uint32_t k = 0; k < ctx->max_deg; k++){
    uint16_t node = adj_row[k];
    if (node == NO_EDGE) break;

    unsigned base = (node < hr) ? node : hr;
    uint16_t shift = shift_row[k];
    int8_t *v2c_lin = v2c_base + (size_t)base * 2 * Z;
    
    compute_check_to_var_msgs(c2v, v2c_lin, min_dup, second_dup, index_dup, sign_dup, shift, var_node, Z);

    c2v += Z;
    var_node++;
  }
  ctx->c2v_init[m] = 1;
}