#include <printf.h>
#include <snrt.h>

#include DATAHEADER
#ifdef GEMM_KERNEL_SOURCE
#include GEMM_KERNEL_SOURCE
#else
#include KERNELHEADER
#endif

enum {
    TCDM_BYTES = 256 * 1024,
    M_CHUNK = 64,
    N_CHUNK = 64,
    K_CHUNK = 64,
    TILE_DIM = 16,
    TILE_FLOATS = TILE_DIM * K_CHUNK,
    TILE_BYTES = TILE_FLOATS * sizeof(float),
    A_TILES = M_CHUNK / TILE_DIM,
    B_TILES = N_CHUNK / TILE_DIM,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_TILE_BYTES = TILE_DIM * TILE_DIM * sizeof(float),
    C_TILE_ROW_BYTES = B_TILES * C_TILE_BYTES,
    C_BYTES = M_CHUNK * N_CHUNK * sizeof(float),
    A_OFFSET = 0,
    B_OFFSET = A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    PARTIAL_FLOATS = M_CHUNK * N_CHUNK,
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    BUFFER_STRIDE = TCDM_BYTES / 2,
    BUFFER_COUNT = 2,
    L1_BYTES = BUFFER_STRIDE + BUFFER_BYTES,
};

_Static_assert(BUFFER_BYTES <= BUFFER_STRIDE,
               "one M64xN64xK64 buffer must fit in half of TCDM");
_Static_assert(L1_BYTES <= TCDM_BYTES,
               "ping-pong GEMM buffers exceed TCDM");
_Static_assert(C_TILE_ROW_BYTES * A_TILES == C_BYTES,
               "packed C tile geometry must cover one output panel");

static const float zero_tile_dram[TILE_FLOATS]
    __attribute__((section(".dram"), aligned(64))) = {0};

static uint8_t *l1_base;
static float *partials;
static float *c_out;
static uint32_t allocation_failed;
static volatile uint32_t core_cycles[2];
static volatile uint32_t verify_errors[2];
static volatile float verify_max_abs[2];

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static inline uint32_t float_bits(float value) {
    union {
        float f;
        uint32_t u;
    } bits = {.f = value};
    return bits.u;
}

static inline void gemm_barrier(uint32_t active_cores) {
    if (active_cores > 1)
        snrt_cluster_hw_barrier();
}

static inline uint32_t chunk_extent(uint32_t total, uint32_t block,
                                    uint32_t chunk) {
    const uint32_t offset = block * chunk;
    return offset < total ? min_u32(chunk, total - offset) : 0;
}

static snrt_dma_txid_t fill_panel(float *dst, const float *src,
                                  uint32_t panel_tiles,
                                  uint32_t valid_tiles,
                                  uint32_t valid_k,
                                  uint32_t src_tile_k) {
    // A 1D copy across tiles is legal only when the source tile has the same
    // physical K stride as the local panel.
    if (valid_tiles == panel_tiles && valid_k == K_CHUNK &&
        src_tile_k == K_CHUNK)
        return snrt_dma_start_1d(dst, src, panel_tiles * TILE_BYTES);

    const snrt_dma_txid_t clear_tid = snrt_dma_start_2d(
        dst, zero_tile_dram, TILE_BYTES, TILE_BYTES, 0, panel_tiles);
    if (valid_tiles == 0 || valid_k == 0)
        return clear_tid;

    const size_t copy_bytes = valid_k * TILE_DIM * sizeof(float);
    const size_t src_stride = src_tile_k * TILE_DIM * sizeof(float);
    return snrt_dma_start_2d(dst, src, copy_bytes, TILE_BYTES, src_stride,
                             valid_tiles);
}

static snrt_dma_txid_t fill_a(float *dst, uint32_t mb, uint32_t kb) {
    const uint32_t k0 = kb * K_CHUNK;
    const uint32_t valid_k = chunk_extent(gemm_l.K, kb, K_CHUNK);
    const uint32_t mt_count = (gemm_l.M + TILE_DIM - 1) / TILE_DIM;
    const uint32_t first_mt = mb * A_TILES;
    const uint32_t valid_tiles = first_mt < mt_count
                                     ? min_u32(A_TILES, mt_count - first_mt)
                                     : 0;
    const float *src = valid_tiles == 0
                           ? zero_tile_dram
                           : gemm_Apack_dram +
                                 (first_mt * gemm_l.Kp + k0) * TILE_DIM;
    return fill_panel(dst, src, A_TILES, valid_tiles, valid_k, gemm_l.Kp);
}

static snrt_dma_txid_t fill_b(float *dst, uint32_t nb, uint32_t kb) {
    const uint32_t k0 = kb * K_CHUNK;
    const uint32_t valid_k = chunk_extent(gemm_l.K, kb, K_CHUNK);
    const uint32_t nt_count = (gemm_l.N + TILE_DIM - 1) / TILE_DIM;
    const uint32_t first_nt = nb * B_TILES;
    const uint32_t valid_tiles = first_nt < nt_count
                                     ? min_u32(B_TILES, nt_count - first_nt)
                                     : 0;
    const float *src = valid_tiles == 0
                           ? zero_tile_dram
                           : gemm_Bpack_dram +
                                 (first_nt * gemm_l.Kp + k0) * TILE_DIM;
    return fill_panel(dst, src, B_TILES, valid_tiles, valid_k, gemm_l.Kp);
}

static snrt_dma_txid_t fill_c(float *dst, uint32_t mb, uint32_t nb,
                              uint32_t kb) {
    // Every K panel produces an independent packed partial. Only kb=0 starts
    // from the bias; later panels must start from zero before L3 reduction.
    const snrt_dma_txid_t clear_tid = snrt_dma_start_2d(
        dst, zero_tile_dram, C_TILE_BYTES, C_TILE_BYTES, 0,
        A_TILES * B_TILES);
    if (kb != 0)
        return clear_tid;

    const uint32_t mt_count = (gemm_l.M + TILE_DIM - 1) / TILE_DIM;
    const uint32_t nt_count = (gemm_l.N + TILE_DIM - 1) / TILE_DIM;
    const uint32_t first_mt = mb * A_TILES;
    const uint32_t first_nt = nb * B_TILES;
    const uint32_t valid_mt = first_mt < mt_count
                                  ? min_u32(A_TILES, mt_count - first_mt)
                                  : 0;
    const uint32_t valid_nt = first_nt < nt_count
                                  ? min_u32(B_TILES, nt_count - first_nt)
                                  : 0;
    if (valid_mt == 0 || valid_nt == 0)
        return clear_tid;

    const float *src = gemm_Biaspack_dram +
                       (first_mt * nt_count + first_nt) *
                           TILE_DIM * TILE_DIM;
    const size_t copy_bytes = valid_nt * C_TILE_BYTES;
    const size_t src_stride = nt_count * C_TILE_BYTES;
    return snrt_dma_start_2d(dst, src, copy_bytes,
                             C_TILE_ROW_BYTES, src_stride, valid_mt);
}

static snrt_dma_txid_t fill_buffer(uint32_t buffer, uint32_t mb, uint32_t nb,
                                   uint32_t kb) {
    uint8_t *base = l1_base + buffer * BUFFER_STRIDE;
    fill_a((float *)(base + A_OFFSET), mb, kb);
    fill_b((float *)(base + B_OFFSET), nb, kb);
    return fill_c((float *)(base + C_OFFSET), mb, nb, kb);
}

static inline float partial_at(const float *partial, uint32_t row,
                               uint32_t col) {
    const uint32_t tile = (row / TILE_DIM) * B_TILES + col / TILE_DIM;
    return partial[tile * TILE_DIM * TILE_DIM +
                   (row % TILE_DIM) * TILE_DIM + col % TILE_DIM];
}

static float reference_partial(uint32_t row, uint32_t col, uint32_t kb) {
    const uint32_t k_begin = kb * K_CHUNK;
    const uint32_t k_end = min_u32(k_begin + K_CHUNK, gemm_l.K);
    const uint32_t mt = row / TILE_DIM;
    const uint32_t nt = col / TILE_DIM;
    const uint32_t row_in_tile = row % TILE_DIM;
    const uint32_t col_in_tile = col % TILE_DIM;
    float sum = 0.0f;

    if (kb == 0) {
        const int32_t value =
            (int32_t)((row * 3u + col * 5u) & 15u) - 8;
        sum = (float)value * 0.03125f;
    }

    for (uint32_t k = k_begin; k < k_end; ++k) {
        const float a = gemm_Apack_dram[
            (mt * gemm_l.Kp + k) * TILE_DIM + row_in_tile];
        const float b = gemm_Bpack_dram[
            (nt * gemm_l.Kp + k) * TILE_DIM + col_in_tile];
        sum += a * b;
    }
    return sum;
}

static inline void finish_spatz(void) {
    uint32_t fcsr;
    asm volatile("vtdiscard\n"
                 "csrr %0, fcsr\n"
                 "fence rw, rw\n"
                 : "=&r"(fcsr)
                 :
                 : "memory");
}

int main(void) {
    const uint32_t cid = snrt_cluster_core_idx();
    const uint32_t ncores = snrt_cluster_core_num();
#ifdef SPATZ_GEMM_SINGLE_CORE
    const uint32_t active_cores = 1;
#else
    const uint32_t active_cores =
        gemm_l.M <= 2 * TILE_DIM ? 1 : min_u32(ncores, 2);
#endif
    const uint32_t post_cores = min_u32(ncores, 2);
    const uint32_t row_pairs = M_CHUNK / (2 * TILE_DIM);
    const uint32_t mb_count = (gemm_l.M + M_CHUNK - 1) / M_CHUNK;
    const uint32_t nb_count = (gemm_l.N + N_CHUNK - 1) / N_CHUNK;
    const uint32_t kb_count = (gemm_l.K + K_CHUNK - 1) / K_CHUNK;
    const uint32_t jobs = mb_count * nb_count;
    const uint32_t iterations = jobs * kb_count;

    if (cid == 0) {
        l1_base = (uint8_t *)snrt_l1alloc(L1_BYTES);
        partials = (float *)snrt_l3alloc(iterations * PARTIAL_FLOATS *
                                         sizeof(float));
        c_out = (float *)snrt_l3alloc(gemm_l.Mp * gemm_l.Np * sizeof(float));
        allocation_failed = l1_base == 0 || partials == 0 || c_out == 0 ||
                            ncores < active_cores;
    }
    snrt_cluster_hw_barrier();
    if (allocation_failed) {
        if (cid == 0)
            printf("general GEMM allocation/core-count failure\n");
        return 2;
    }

    snrt_dma_txid_t input_tid[BUFFER_COUNT] = {0, 0};
    uint32_t current_buffer = 0;
    uint32_t kernel_cycles = 0;

    if (cid == 0) {
        input_tid[0] = fill_buffer(0, 0, 0, 0);
        snrt_dma_wait(input_tid[0]);
    }
    gemm_barrier(active_cores);

    // Prime the alternate buffer.  Iteration 0 computes from buffer 0 while
    // this descriptor chain fills buffer 1.
    if (cid == 0 && iterations > 1) {
        const uint32_t next_job = 1 / kb_count;
        input_tid[1] = fill_buffer(1, next_job / nb_count,
                                   next_job % nb_count, 1 % kb_count);
    }

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        const uint32_t job = iter / kb_count;
        const uint32_t mb = job / nb_count;
        const uint32_t nb = job % nb_count;
        const uint32_t kb = iter % kb_count;
        const uint32_t valid_m = chunk_extent(gemm_l.M, mb, M_CHUNK);
        const uint32_t valid_n = chunk_extent(gemm_l.N, nb, N_CHUNK);
        const uint32_t valid_k = chunk_extent(gemm_l.K, kb, K_CHUNK);

        const uint32_t next_iter = iter + 1;
        const uint32_t has_next = next_iter < iterations;
        const uint32_t refill_iter = iter + 2;
        const uint32_t has_refill = refill_iter < iterations;
        const uint32_t next_buffer = current_buffer ^ 1;

        if (cid < active_cores) {
            uint8_t *base = l1_base + current_buffer * BUFFER_STRIDE;
            float *a = (float *)(base + A_OFFSET);
            float *b = (float *)(base + B_OFFSET);
            float *c = (float *)(base + C_OFFSET);
            const uint32_t t0 = gemm_cycle();
            uint32_t did_compute = 0;

            if (valid_m == M_CHUNK && valid_n == N_CHUNK &&
                valid_k == K_CHUNK) {
                for (uint32_t pair = cid; pair < row_pairs;
                     pair += active_cores) {
                    const uint32_t ti_lo = 2 * pair;
                    gemm_fp32(c, a, b, ti_lo, ti_lo + 2);
                    did_compute = 1;
                }
            } else {
                for (uint32_t pair = cid; pair < row_pairs;
                     pair += active_cores) {
                    const uint32_t core_row = pair * 2 * TILE_DIM;
                    const uint32_t core_m = core_row < valid_m
                                                ? min_u32(2 * TILE_DIM,
                                                          valid_m - core_row)
                                                : 0;
                    if (core_m != 0) {
                        gemm_block_fp32(
                            (void *)((uint8_t *)a +
                                     core_row * K_CHUNK * sizeof(float)),
                            (void *)b,
                            (void *)((uint8_t *)c +
                                     core_row * N_CHUNK * sizeof(float)),
                            (int)valid_k, (int)valid_n, (int)core_m, 0);
                        did_compute = 1;
                    }
                }
            }

            if (did_compute) {
                finish_spatz();
                kernel_cycles += gemm_cycle() - t0;
            }
        }
        gemm_barrier(active_cores);

        if (cid == 0) {
            // Do not switch to the alternate buffer until all of its A/B
            // descriptors have completed.
            if (has_next)
                snrt_dma_wait(input_tid[next_buffer]);

            float *src = (float *)(l1_base +
                                   current_buffer * BUFFER_STRIDE + C_OFFSET);
            snrt_dma_start_1d(partials + iter * PARTIAL_FLOATS, src, C_BYTES);

            // Reuse the just-computed buffer for iteration i+2.  The DMA
            // executes descriptors in issue order, so C(i) is read before the
            // following A/B refill overwrites this 128 KiB half. This complete
            // chain overlaps compute(i+1) in the other half.
            if (has_refill) {
                const uint32_t refill_job = refill_iter / kb_count;
                input_tid[current_buffer] = fill_buffer(
                    current_buffer, refill_job / nb_count,
                    refill_job % nb_count, refill_iter % kb_count);
            }
        }
        gemm_barrier(active_cores);
        if (has_next)
            current_buffer = next_buffer;
    }

    if (cid < active_cores)
        core_cycles[cid] = kernel_cycles;

    if (cid == 0)
        snrt_dma_wait_all();
    snrt_cluster_hw_barrier();

    if (cid < post_cores) {
        const uint32_t output_elements = gemm_l.M * gemm_l.N;
        for (uint32_t idx = cid; idx < output_elements; idx += post_cores) {
            const uint32_t row = idx / gemm_l.N;
            const uint32_t col = idx % gemm_l.N;
            const uint32_t mb = row / M_CHUNK;
            const uint32_t nb = col / N_CHUNK;
            const uint32_t job = mb * nb_count + nb;
            const uint32_t local_row = row % M_CHUNK;
            const uint32_t local_col = col % N_CHUNK;
            float sum = 0.0f;
            for (uint32_t kb = 0; kb < kb_count; ++kb) {
                const float *partial = partials +
                    (job * kb_count + kb) * PARTIAL_FLOATS;
                sum += partial_at(partial, local_row, local_col);
            }
            c_out[row * gemm_l.Np + col] = sum;
        }
    }
    snrt_cluster_hw_barrier();

    uint32_t cycles = core_cycles[0];
    for (uint32_t core = 1; core < active_cores; ++core) {
        if (core_cycles[core] > cycles)
            cycles = core_cycles[core];
    }

    enum { CE = 8 };
    const uint64_t useful_fmas =
        (uint64_t)gemm_l.M * gemm_l.N * gemm_l.K;
    const uint64_t peak_fmas =
        (uint64_t)cycles * CE * CE * active_cores;
    const uint32_t utilization_bp =
        peak_fmas != 0 ? (uint32_t)((useful_fmas * 10000u) / peak_fmas) : 0;

    uint32_t local_errors = 0;
    uint32_t printed_partial_debug = 0;
    float local_max_abs = 0.0f;
    const float tolerance = 1.0e-3f;
    for (uint32_t row = cid; cid < post_cores && row < gemm_l.M;
         row += post_cores) {
        uint32_t row_errors = 0;
        uint32_t error_mask_lo = 0;
        uint32_t error_mask_hi = 0;
        for (uint32_t col = 0; col < gemm_l.N; ++col) {
            const uint32_t idx = row * gemm_l.N + col;
            const float output = c_out[row * gemm_l.Np + col];
            const float expected = gemm_Cref_dram[idx];
            if (float_bits(output) == float_bits(expected))
                continue;
            const float diff = output - expected;
            const float abs = diff < 0.0f ? -diff : diff;
            float ref = expected;
            ref = ref < 0.0f ? -ref : ref;
            if (abs > local_max_abs)
                local_max_abs = abs;
            if (abs > tolerance && abs > tolerance * ref) {
                ++local_errors;
                ++row_errors;
                if (col < 32)
                    error_mask_lo |= 1u << col;
                else if (col < 64)
                    error_mask_hi |= 1u << (col - 32);

                if (!printed_partial_debug) {
                    const uint32_t mb = row / M_CHUNK;
                    const uint32_t nb = col / N_CHUNK;
                    const uint32_t job = mb * nb_count + nb;
                    const uint32_t local_row = row % M_CHUNK;
                    const uint32_t local_col = col % N_CHUNK;
                    printf("first mismatch row=%u col=%u output=0x%08x ref=0x%08x\n",
                           row, col, *(uint32_t *)&c_out[row * gemm_l.Np + col],
                           *(uint32_t *)&gemm_Cref_dram[idx]);
                    for (uint32_t kb = 0; kb < kb_count; ++kb) {
                        const float actual = partial_at(
                            partials + (job * kb_count + kb) * PARTIAL_FLOATS,
                            local_row, local_col);
                        const float expected = reference_partial(row, col, kb);
                        printf("  kb=%u actual=0x%08x expected=0x%08x\n", kb,
                               *(uint32_t *)&actual, *(uint32_t *)&expected);
                    }
                    printed_partial_debug = 1;
                }
            }
        }
        if (row_errors)
            printf("core %u row %u: %u errors, col_mask[63:0]=%08x_%08x\n",
                   cid,
                   row, row_errors, error_mask_hi, error_mask_lo);
    }

    if (cid < post_cores) {
        verify_errors[cid] = local_errors;
        verify_max_abs[cid] = local_max_abs;
    }
    snrt_cluster_hw_barrier();

    if (cid != 0)
        return 0;

    uint32_t errors = 0;
    float max_abs = 0.0f;
    for (uint32_t core = 0; core < post_cores; ++core) {
        errors += verify_errors[core];
        if (verify_max_abs[core] > max_abs)
            max_abs = verify_max_abs[core];
    }

    printf("\n=== General FP32 GEMM %ux%ux%u "
           "(M64xN64xK64 panels, 2 buffers, %u core%s) ===\n",
           gemm_l.M, gemm_l.N, gemm_l.K, active_cores,
           active_cores == 1 ? "" : "s");
    printf("Cycles:    %u\n", cycles);
    printf("Utilization: %u.%02u%%\n", utilization_bp / 100,
           utilization_bp % 100);
    printf("MaxAbsErr: 0x%08x\n", *(uint32_t *)&max_abs);
    printf("Errors:    %u\n", errors);
    printf("Status:    %s\n", errors ? "FAIL" : "PASS");
    return errors ? 1 : 0;
}
