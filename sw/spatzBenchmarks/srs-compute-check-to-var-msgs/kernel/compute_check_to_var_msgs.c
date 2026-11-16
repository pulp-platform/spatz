#include "compute_check_to_var_msgs.h"
#include <stddef.h>
#include <snrt.h>
#define ELENB 8

void compute_check_to_var_msgs(int8_t *this_check_to_var,
                               int8_t const *this_var_to_check,
                               int8_t const *min_var_to_check_dup,
                               int8_t const *second_min_var_to_check_dup,
                               uint8_t const *min_var_to_check_index_dup,
                               uint8_t const *sign_prod_var_to_check_dup,
                               uint32_t shift,
                               uint32_t var_node,
                               uint32_t const lifting_size)
{
    const uint32_t off = lifting_size - shift;
    const uint32_t r   = off & (ELENB - 1);
    const uint32_t a   = off - r;

    const int8_t  *min_ro = min_var_to_check_dup        + a;
    const int8_t  *sec_ro = second_min_var_to_check_dup + a;
    const uint8_t *idx_ro = min_var_to_check_index_dup  + a;
    const uint8_t *sgn_ro = sign_prod_var_to_check_dup  + a;

    uint32_t vlmax, vl;
    asm volatile("vsetvli %0, zero, e8, m8, ta, mu" : "=r"(vlmax));
    const uint32_t step = vlmax - ELENB;

    uint32_t remaining = lifting_size;
    uint32_t n  = remaining < step ? remaining : step;
    uint32_t nl = n + r;

    asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(nl));
    asm volatile("vle8.v v8, (%0)" :: "r"(idx_ro) : "memory");
    asm volatile("vmseq.vx v0, v8, %0" :: "r"(var_node));
    asm volatile("vle8.v v16, (%0)" :: "r"(min_ro) : "memory");
    asm volatile("vle8.v v24, (%0)" :: "r"(sec_ro) : "memory");
    asm volatile("vmerge.vvm v8, v16, v24, v0");

    asm volatile("vle8.v v16, (%0)" :: "r"(sgn_ro) : "memory");
    asm volatile("vmsne.vx v0, v16, zero");
    asm volatile("vrsub.vx v8, v8, zero, v0.t");

    asm volatile("vsetvli %0, %1, e8, m8, ta, mu" : "=r"(vl) : "r"(n));
    asm volatile("vslidedown.vx v16, v8, %0" :: "r"(r));

    asm volatile("vle8.v v24, (%0)" :: "r"(this_var_to_check) : "memory");
    asm volatile("vmslt.vx v0, v24, zero");
    asm volatile("vrsub.vx v16, v16, zero, v0.t");
    asm volatile("vse8.v v16, (%0)" :: "r"(this_check_to_var) : "memory");
}