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
    N_CHUNK = GEMM_CHUNK_N,
    K_CHUNK = GEMM_CHUNK_K,
    TILE_DIM = 16,
    TILE_FLOATS = TILE_DIM * K_CHUNK,
    TILE_BYTES = TILE_FLOATS * sizeof(float),
    A_TILES = M_CHUNK / TILE_DIM,
    B_TILES = N_CHUNK / TILE_DIM,
    A_BYTES = A_TILES * TILE_BYTES,
    B_BYTES = B_TILES * TILE_BYTES,
    C_BYTES = M_CHUNK * N_CHUNK * sizeof(float),
    A_OFFSET = 0,
    B_OFFSET = A_BYTES,
    C_OFFSET = B_OFFSET + B_BYTES,
    PARTIAL_FLOATS = M_CHUNK * N_CHUNK,
    BUFFER_BYTES = C_OFFSET + C_BYTES,
    BUFFER_COUNT = 2,
    RESIDENT_A_OFFSET = 0,
    RESIDENT_B_OFFSET = RESIDENT_A_OFFSET + A_BYTES,
    RESIDENT_C_OFFSET = RESIDENT_B_OFFSET + 2 * B_BYTES,
    RESIDENT_BYTES = RESIDENT_C_OFFSET + C_BYTES,
};

_Static_assert(BUFFER_BYTES <= TCDM_BYTES,
               "selected GEMM chunk does not fit in TCDM");
_Static_assert(BUFFER_COUNT * BUFFER_BYTES <= TCDM_BYTES,
               "GEMM buffer allocation exceeds TCDM");
_Static_assert(RESIDENT_BYTES <= TCDM_BYTES,
               "resident-A, B ping-pong, and C layout exceeds TCDM");

static const float zero_tile_dram[TILE_FLOATS]
    __attribute__((section(".dram"), aligned(64))) = {0};

static uint8_t *l1_base;
static float *partials;
static float *c_out;
static uint32_t allocation_failed;

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

static inline void finish_spatz(void) {
    uint32_t spatz_fcsr;
    asm volatile("vtdiscard\n"
                 "csrr %0, fcsr\n"
                 "fence rw, rw\n"
                 : "=&r"(spatz_fcsr)
                 :
                 : "memory");
}

static snrt_dma_txid_t fill_panel(float *dst, const float *src,
                                  uint32_t panel_tiles,
                                  uint32_t valid_tiles,
                                  uint32_t chunk_k,
                                  uint32_t src_tile_k) {
    // A panel is physically contiguous only when its source tile stride is
    // identical to the local K chunk. For example, Kp=192/K_CHUNK=128 must
    // skip over the remaining 64 K rows between packed source tiles.
    if (valid_tiles == panel_tiles && chunk_k == K_CHUNK &&
        src_tile_k == K_CHUNK)
        return snrt_dma_start_1d(dst, src, panel_tiles * TILE_BYTES);

    // D2 first clears every destination tile, then copies only the valid
    // K-slices. XDMA executes descriptors in issue order.
    const snrt_dma_txid_t clear_tid = snrt_dma_start_2d(
        dst, zero_tile_dram, TILE_BYTES, TILE_BYTES, 0, panel_tiles);
    if (valid_tiles == 0)
        return clear_tid;

    const size_t copy_bytes = chunk_k * TILE_DIM * sizeof(float);
    const size_t src_stride = src_tile_k * TILE_DIM * sizeof(float);
    return snrt_dma_start_2d(dst, src, copy_bytes, TILE_BYTES, src_stride,
                             valid_tiles);
}

static snrt_dma_txid_t fill_a(float *a_dst, uint32_t mb, uint32_t kb) {
    const uint32_t k0 = kb * K_CHUNK;
    const uint32_t chunk_k = min_u32(K_CHUNK, gemm_l.Kp - k0);
    const uint32_t mt_count = gemm_l.Mp / TILE_DIM;
    const uint32_t first_mt = mb * A_TILES;
    const uint32_t valid_tiles = first_mt < mt_count
                                     ? min_u32(A_TILES, mt_count - first_mt)
                                     : 0;
    const float *src = valid_tiles == 0
                           ? zero_tile_dram
                           : gemm_Apack_dram +
                                 (first_mt * gemm_l.Kp + k0) * TILE_DIM;
    return fill_panel(a_dst, src, A_TILES, valid_tiles, chunk_k, gemm_l.Kp);
}

static snrt_dma_txid_t fill_b(float *b_dst, uint32_t nb, uint32_t kb) {
    const uint32_t k0 = kb * K_CHUNK;
    const uint32_t chunk_k = min_u32(K_CHUNK, gemm_l.Kp - k0);
    const uint32_t nt_count = gemm_l.Np / TILE_DIM;
    const uint32_t first_nt = nb * B_TILES;
    const uint32_t valid_tiles = first_nt < nt_count
                                     ? min_u32(B_TILES, nt_count - first_nt)
                                     : 0;
    const float *src = valid_tiles == 0
                           ? zero_tile_dram
                           : gemm_Bpack_dram +
                                 (first_nt * gemm_l.Kp + k0) * TILE_DIM;
    return fill_panel(b_dst, src, B_TILES, valid_tiles, chunk_k, gemm_l.Kp);
}

static snrt_dma_txid_t fill_buffer(uint32_t buffer, uint32_t mb, uint32_t nb,
                                   uint32_t kb) {
    uint8_t *base = l1_base + buffer * BUFFER_BYTES;
    fill_a((float *)(base + A_OFFSET), mb, kb);
    return fill_b((float *)(base + B_OFFSET), nb, kb);
}

static inline float partial_at(const float *partial, uint32_t row,
                               uint32_t col) {
#ifdef GEMM_PACKED_STORE
    const uint32_t tile = (row / TILE_DIM) * B_TILES + col / TILE_DIM;
    return partial[tile * TILE_DIM * TILE_DIM +
                   (row % TILE_DIM) * TILE_DIM + col % TILE_DIM];
#else
    return partial[row * N_CHUNK + col];
#endif
}

int main(void) {
    const uint32_t cid = snrt_cluster_core_idx();
    if (cid != 0)
        return 0;

    const uint32_t row_pairs = M_CHUNK / (2 * TILE_DIM);
    const uint32_t mb_count = (gemm_l.Mp + M_CHUNK - 1) / M_CHUNK;
    const uint32_t nb_count = (gemm_l.Np + N_CHUNK - 1) / N_CHUNK;
    const uint32_t kb_count = (gemm_l.Kp + K_CHUNK - 1) / K_CHUNK;
    const uint32_t jobs = mb_count * nb_count;
    const uint32_t iterations = jobs * kb_count;

    const uint32_t l1_bytes = BUFFER_COUNT == 2
                                  ? 2 * BUFFER_BYTES
                                  : RESIDENT_BYTES;
    l1_base = (uint8_t *)snrt_l1alloc(l1_bytes);
    partials = (float *)snrt_l3alloc(iterations * PARTIAL_FLOATS *
                                     sizeof(float));
    c_out = (float *)snrt_l3alloc(gemm_l.Mp * gemm_l.Np * sizeof(float));
    allocation_failed = l1_base == 0 || partials == 0 || c_out == 0;
    if (allocation_failed) {
        printf("general GEMM allocation failure\n");
        return 2;
    }

    uint32_t measured_cycles = 0;
    if (BUFFER_COUNT == 2) {
        snrt_dma_txid_t input_tid[2] = {0, 0};
        uint32_t current_buffer = 0;
        input_tid[0] = fill_buffer(0, 0, 0, 0);
        snrt_dma_wait(input_tid[0]);
        for (uint32_t iter = 0; iter < iterations; ++iter) {
            const uint32_t next_iter = iter + 1;
            const uint32_t has_next = next_iter < iterations;
            const uint32_t next_buffer = current_buffer ^ 1;

            if (has_next) {
                const uint32_t next_job = next_iter / kb_count;
                input_tid[next_buffer] = fill_buffer(
                    next_buffer, next_job / nb_count, next_job % nb_count,
                    next_iter % kb_count);
            }
            uint8_t *base = l1_base + current_buffer * BUFFER_BYTES;
            const uint32_t t0 = gemm_cycle();
            for (uint32_t pair = 0; pair < row_pairs; ++pair) {
                const uint32_t core_row = pair * 2 * TILE_DIM;
                gemm_block_fp32(
                    (void *)(base + A_OFFSET +
                             core_row * K_CHUNK * sizeof(float)),
                    (void *)(base + B_OFFSET),
                    (void *)(base + C_OFFSET +
                             core_row * N_CHUNK * sizeof(float)),
                    (int)K_CHUNK, (int)N_CHUNK, 2 * TILE_DIM, 0);
                // Complete this pair's VTSE stream before reusing the tiles.
                finish_spatz();
            }
            measured_cycles += gemm_cycle() - t0;

            float *src = (float *)(l1_base +
                                   current_buffer * BUFFER_BYTES + C_OFFSET);
            snrt_dma_start_1d(partials + iter * PARTIAL_FLOATS, src, C_BYTES);
            if (has_next)
                snrt_dma_wait(input_tid[next_buffer]);
            if (has_next)
                current_buffer = next_buffer;
        }
    } else {
        // Retain A while traversing N and prefetch B into the alternate bank.
        // C uses one buffer so the 112 KiB layout leaves the per-core stacks
        // above TCDM allocation untouched.
        float *a = (float *)(l1_base + RESIDENT_A_OFFSET);
        float *b[2] = {
            (float *)(l1_base + RESIDENT_B_OFFSET),
            (float *)(l1_base + RESIDENT_B_OFFSET + B_BYTES),
        };
        float *c = (float *)(l1_base + RESIDENT_C_OFFSET);

        for (uint32_t kb = 0; kb < kb_count; ++kb) {
            for (uint32_t mb = 0; mb < mb_count; ++mb) {
                uint32_t ping = 0;
                snrt_dma_wait_all();
                fill_a(a, mb, kb);
                fill_b(b[0], 0, kb);
                // Make the complete A/B panel visible before compute starts.
                snrt_dma_wait_all();
                for (uint32_t nb = 0; nb < nb_count; ++nb) {
                    const uint32_t has_next_n = nb + 1 < nb_count;
                    if (has_next_n)
                        fill_b(b[ping ^ 1], nb + 1, kb);
                    const uint32_t t0 = gemm_cycle();
                    for (uint32_t pair = 0; pair < row_pairs; ++pair) {
                        const uint32_t core_row = pair * 2 * TILE_DIM;
                        gemm_block_fp32(
                            (void *)((uint8_t *)a +
                                     core_row * K_CHUNK * sizeof(float)),
                            (void *)b[ping],
                            (void *)((uint8_t *)c +
                                     core_row * N_CHUNK * sizeof(float)),
                            (int)K_CHUNK, (int)N_CHUNK,
                            2 * TILE_DIM, 0);
                        finish_spatz();
                    }
                    measured_cycles += gemm_cycle() - t0;

                    const uint32_t job = mb * nb_count + nb;
                    const uint32_t partial = job * kb_count + kb;
                    snrt_dma_start_1d(partials + partial * PARTIAL_FLOATS,
                                      c, C_BYTES);
                    // C must be copied before the next job overwrites it.
                    snrt_dma_wait_all();
                    ping ^= 1;
                }
            }
        }
    }

    snrt_dma_wait_all();

    const uint32_t output_elements = gemm_l.M * gemm_l.N;
    for (uint32_t idx = 0; idx < output_elements; ++idx) {
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
    const uint32_t cycles = measured_cycles;

    enum { CE = 8 };
    const uint64_t useful_fmas =
        (uint64_t)gemm_l.M * gemm_l.N * gemm_l.K;
    const uint64_t peak_fmas =
        (uint64_t)cycles * CE * CE;
    const uint32_t utilization_bp =
        peak_fmas != 0 ? (uint32_t)((useful_fmas * 10000u) / peak_fmas) : 0;

    uint32_t local_errors = 0;
    float local_max_abs = 0.0f;
    const float tolerance = 1.0e-3f;
    for (uint32_t row = 0; row < gemm_l.M; ++row) {
        uint32_t row_errors = 0;
        for (uint32_t col = 0; col < gemm_l.N; ++col) {
            const uint32_t idx = row * gemm_l.N + col;
            const float output = c_out[row * gemm_l.Np + col];
            const float expected = gemm_Cref_dram[idx];
            if (float_bits(output) == float_bits(expected))
                continue;
            float diff = output - expected;
            float abs = diff < 0.0f ? -diff : diff;
            float ref = expected;
            ref = ref < 0.0f ? -ref : ref;
            if (abs > local_max_abs)
                local_max_abs = abs;
            if (abs > tolerance && abs > tolerance * ref) {
                ++local_errors;
                ++row_errors;
            }
        }
        if (row_errors)
            printf("core %u row %u: %u errors\n", cid, row, row_errors);
    }

    printf("\n=== General FP32 GEMM %ux%ux%u "
           "(M64xN%uxK%u chunks, %u buffer%s, 1 core) ===\n",
           gemm_l.M, gemm_l.N, gemm_l.K, N_CHUNK, K_CHUNK, BUFFER_COUNT,
           BUFFER_COUNT == 1 ? "" : "s");
    printf("Cycles:    %u\n", cycles);
    printf("Utilization: %u.%02u%%\n", utilization_bp / 100,
           utilization_bp % 100);
    printf("MaxAbsErr: 0x%08x\n", *(uint32_t *)&local_max_abs);
    printf("Errors:    %u\n", local_errors);
    printf("Status:    %s\n", local_errors ? "FAIL" : "PASS");
    return local_errors ? 1 : 0;
}
