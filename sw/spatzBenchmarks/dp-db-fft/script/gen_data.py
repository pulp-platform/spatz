#!/usr/bin/env python3
# Copyright 2021 ETH Zurich and University of Bologna.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Generates data for the double-buffered sectioned FFT kernel.
#
# JSON config fields:
#   npoints : total FFT size N (power of 2)
#   ncores  : number of real hardware cores (currently 2)
#   nsec    : number of sections S (power of 2, >= ncores)
#             determines Phase 1 depth: log2(S) stages
#             determines Phase 2 sub-FFT size: N/S points
#             must satisfy: N/S >= 4 (at least one P2 butterfly stage)
#
# Twiddle DRAM layout emitted (all doubles, Re then Im split):
#   [P1_Re : NTWI_P1][P1_Im : NTWI_P1]
#   [P2_sec0_Re : NTWI_P2][P2_sec0_Im : NTWI_P2]
#   [P2_sec1_Re : NTWI_P2][P2_sec1_Im : NTWI_P2]
#   ...  (NSEC blocks total)
#
# Phase 1 twiddle layout detail (within P1_Re / P1_Im blocks):
#   Stage 0 contributes NFFT/2   twiddles (indices 0 .. NFFT/2 - 1)
#   Stage 1 contributes NFFT/4   twiddles (indices NFFT/2 .. 3*NFFT/4 - 1)
#   ...
#   Stage k contributes NFFT/2^(k+1) twiddles
#   Total NTWI_P1 = NFFT * (1 - 1/NSEC)
#
# Within stage k, the NFFT/2^(k+1) twiddles are split evenly across the
# S/2^k sections in that stage. Each section at stage k owns NFFT/S = NFFTh
# twiddles. Twiddles are identical across groups (same idx within group).
#
# Phase 2 twiddle layout (setupTwiddlesLUT_dif_vec on NFFTh-point sub-FFT):
#   log2(NFFTh) stages * NFFTh/2 twiddles each = NTWI_P2 twiddles
#   Serialised as [Re: NTWI_P2][Im: NTWI_P2] per section block.
#   All sections share the same twiddle values (duplicated for each section
#   to allow independent DMA loading per section without DRAM conflicts).

import numpy as np
import copy
import argparse
import pathlib
import hjson

np.random.seed(42)

FFT2_SAMPLE_DYN = 13
FFT_TWIDDLE_DYN = 15


# ---------------------------------------------------------------------------
# Helpers (unchanged from original)
# ---------------------------------------------------------------------------

def serialize_cmplx(vector, NFFT, dtype):
    vector_re = np.real(vector)
    vector_im = np.imag(vector)
    serial_vec = np.empty(2 * NFFT, dtype=dtype)
    serial_vec[0::2] = vector_re
    serial_vec[1::2] = vector_im
    return serial_vec


def setupInput(samples, Nfft, dyn):
    with np.nditer(samples, op_flags=['readwrite']) as it:
        for samp in it:
            samp[...]['re'] = np.random.randn(1)
            samp[...]['im'] = np.random.randn(1)


def setupTwiddlesLUT(Twiddles_vec, Nfft):
    Theta = (2 * np.pi) / Nfft
    with np.nditer(Twiddles_vec, op_flags=['readwrite']) as it:
        for idx, twi in enumerate(it):
            Phi = 2 * np.pi - Theta * idx
            twi[...]['re'] = np.cos(Phi)
            twi[...]['im'] = np.sin(Phi)


def setupTwiddlesLUT_dif_vec(Twiddles_vec, Nfft):
    """Fill Twiddles_vec with DIF twiddle factors for an Nfft-point FFT.

    Layout: log2(Nfft) stages, each with Nfft/2 entries.
    Stage s, position t: W_N^(2^s * t mod N/2)
    """
    stages = int(np.log2(Nfft))
    Theta = (2 * np.pi) / Nfft
    twi = [[np.cos((2 * np.pi) - i * Theta), np.sin((2 * np.pi) - i * Theta)]
           for i in range(int(Nfft / 2))]
    for s in range(stages):
        for t in range(int(Nfft / 2)):
            Twiddles_vec[int(s * Nfft / 2 + t)]['re'] = twi[int((2**s * t) % int(Nfft / 2))][0]
            Twiddles_vec[int(s * Nfft / 2 + t)]['im'] = twi[int((2**s * t) % int(Nfft / 2))][1]
    return Twiddles_vec


def gen_bitrev_idx(nfft):
    fmt = '{:0' + str(int(np.log2(nfft))) + 'b}'
    bitrev = np.zeros(nfft)
    for n in np.arange(nfft):
        bitrev[n] = int(fmt.format(n)[::-1], 2)
    return bitrev


def gen_store_idx(nfft):
    """Generate shuffle indices for Phase 2 intermediate butterfly stages."""
    ibuf = []
    dbuf = []
    a = [*range(nfft)]
    b = np.zeros(nfft)
    old_b = a
    nffth = nfft >> 1
    for bf in range(int(np.log2(nffth))):
        stride = nffth >> (bf + 1)
        for h in range(2):
            for i in range(int(nffth / (stride << 1))):
                for j in range(stride):
                    b[h * (nffth >> 1) + i * stride + j] = \
                        old_b[h * nffth + j + 2 * i * stride]
                    b[h * (nffth >> 1) + i * stride + j + nffth] = \
                        old_b[h * nffth + j + 2 * i * stride + stride]
        delta = [[i for i in b].index(n) for n in old_b]
        ibuf += [[i for i in b]]
        dbuf += [delta]
        old_b = [n for n in copy.deepcopy(b)]
        b = np.zeros(nfft)
    idx_list = sum(ibuf, [])
    delta_list = sum(dbuf, [])
    return [idx_list, delta_list]


# ---------------------------------------------------------------------------
# Phase 1 twiddle generation
#
# For a DIF FFT of size NFFT decomposed into NSEC sections:
#   - log2_S = log2(NSEC) Phase 1 stages
#   - Stage k operates on groups of size NFFT/2^k, butterfly distance NFFT/2^(k+1)
#   - Each section within stage k owns NFFTh = NFFT/NSEC twiddle elements
#   - Twiddles are drawn from a DIF twiddle table computed for NFFT points
#   - All groups at stage k share the same twiddle values (twiddles repeat
#     every NFFT/2^(k+1) elements in the full DIF table, and each section
#     covers exactly NFFTh = NFFT/NSEC of those)
#
# The P1 twiddle block is laid out as:
#   [Stage 0: NFFTh * (NSEC/1)  / (NSEC) = NFFTh twiddles ... wait]
#
# Actually more carefully:
#   Stage k has 2^k groups. Each group has NSEC/2^k sections.
#   Each section processes NFFTh elements and needs NFFTh twiddles.
#   But twiddles are the same across groups, so we store only one group's
#   worth per stage: (NSEC/2^k) * NFFTh = NFFT/2^(k+1) twiddles per stage.
#   This matches NTWI_P1 = sum_{k=0}^{log2_S-1} NFFT/2^(k+1) = NFFT*(1-1/NSEC).
#
# Within stage k, the twiddle slice for section idx (0-indexed within group)
# starts at: sum_{i=0}^{k-1} NFFT/2^(i+1)  +  idx * NFFTh
#
# The twiddle values at stage k come from the full NFFT-point DIF table:
#   full_table[stage_k_offset + idx * NFFTh : + NFFTh]
# where stage_k_offset = k * NFFT/2 in the full table.
# ---------------------------------------------------------------------------

def gen_p1_twiddles(NFFT, NSEC, dtype_cplx):
    """Generate Phase 1 twiddle array.

    Returns a complex structured array of shape (NTWI_P1,) with Re/Im fields,
    laid out as described above (one group's twiddles per stage, contiguous).
    """
    log2_S  = int(np.log2(NSEC))
    NFFTh   = NFFT // NSEC
    NTWI_P1 = int(NFFT * (1 - 1.0 / NSEC))

    # Build the full NFFT-point DIF twiddle table
    # setupTwiddlesLUT_dif_vec fills log2(NFFT) stages × NFFT/2 entries each
    N_FULL   = int(np.log2(NFFT)) * (NFFT // 2)
    full_twi = np.empty(N_FULL, dtype=dtype_cplx)
    full_twi = setupTwiddlesLUT_dif_vec(full_twi, NFFT)

    p1_twi = np.empty(NTWI_P1, dtype=dtype_cplx)
    out_idx = 0

    for k in range(log2_S):
        # At stage k there are 2^k groups; each group has NSEC/2^k sections.
        # Twiddles are identical across groups, so we store one group's worth:
        #   (NSEC / 2^k) / 2 * NFFTh  ... no, more simply:
        #   stage k butterfly distance = NFFT / 2^(k+1)
        #   one group spans NFFT / 2^k elements, upper+lower wings each NFFT/2^(k+1)
        #   twiddles needed per group = NFFT / 2^(k+1)
        stage_twi_count = NFFT >> (k + 1)   # = NFFT / 2^(k+1)
        # Offset into full NFFT-point DIF table for stage k
        full_stage_offset = k * (NFFT // 2)

        # Copy stage_twi_count twiddles from full table starting at stage_offset
        # These cover all sections in one group sequentially
        for i in range(stage_twi_count):
            p1_twi[out_idx + i]['re'] = full_twi[full_stage_offset + i]['re']
            p1_twi[out_idx + i]['im'] = full_twi[full_stage_offset + i]['im']
        out_idx += stage_twi_count

    assert out_idx == NTWI_P1, f"P1 twiddle count mismatch: {out_idx} != {NTWI_P1}"
    return p1_twi


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='Generate data for db-fft kernel')
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True,
                        help='Select param config file kernel')
    parser.add_argument("-v", "--verbose", action='store_true',
                        help='Set verbose')
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    NFFT  = param['npoints']   # Total FFT size, power of 2
    CORES = param['ncores']    # Number of real hardware cores (2)
    NSEC  = param['nsec']      # Number of sections S, power of 2, >= CORES

    assert (NFFT & (NFFT - 1)) == 0,  "npoints must be a power of 2"
    assert (NSEC & (NSEC - 1)) == 0,  "nsec must be a power of 2"
    assert NSEC >= CORES,              "nsec must be >= ncores"
    assert NFFT // NSEC >= 4,         "N/S must be >= 4 (need at least one P2 stage)"
    assert NSEC <= NFFT // 4,         "nsec too large relative to npoints"

    log2_NFFT = int(np.log2(NFFT))
    log2_S    = int(np.log2(NSEC))
    log2_sec  = log2_NFFT - log2_S    # log2(N/S), Phase 2 stages per section
    NFFTh     = NFFT // NSEC          # Elements per section
    NTWI_P1   = int(NFFT * (1 - 1.0 / NSEC))   # Total P1 twiddles (Re or Im half)
    NTWI_P2   = int(log2_sec * NFFTh / 2)       # P2 twiddles per section (Re or Im half)

    dtype      = np.float64
    idx_dtype  = np.uint64             # 8 bytes for 64-bit double indexing
    dtype_cplx = np.dtype([('re', dtype), ('im', dtype)])

    # -----------------------------------------------------------------------
    # Input samples
    # -----------------------------------------------------------------------
    samples = np.empty(NFFT, dtype=dtype_cplx)
    setupInput(samples, NFFT, FFT2_SAMPLE_DYN)

    # Golden output
    gold_out = np.fft.fft(samples['re'] + 1j * samples['im'])

    # Serialise samples: [Re: NFFT][Im: NFFT]
    samples_s    = serialize_cmplx(samples['re'] + 1j * samples['im'], NFFT, dtype)
    samples_reim = np.empty(2 * NFFT, dtype=dtype)
    samples_reim[0:NFFT]      = samples_s[0::2]
    samples_reim[NFFT:2*NFFT] = samples_s[1::2]

    # -----------------------------------------------------------------------
    # Phase 1 twiddles
    # -----------------------------------------------------------------------
    p1_twi = gen_p1_twiddles(NFFT, NSEC, dtype_cplx)

    # Serialise P1 twiddles: [Re: NTWI_P1][Im: NTWI_P1]
    p1_twi_s    = serialize_cmplx(p1_twi['re'] + 1j * p1_twi['im'], NTWI_P1, dtype)
    p1_twi_reim = np.empty(2 * NTWI_P1, dtype=dtype)
    p1_twi_reim[0:NTWI_P1]         = p1_twi_s[0::2]
    p1_twi_reim[NTWI_P1:2*NTWI_P1] = p1_twi_s[1::2]

    # -----------------------------------------------------------------------
    # Phase 2 twiddles (per section; all sections identical, duplicated NSEC times)
    # -----------------------------------------------------------------------
    p2_twi_v = np.empty(NTWI_P2, dtype=dtype_cplx)
    p2_twi_v = setupTwiddlesLUT_dif_vec(p2_twi_v, NFFTh)

    # Serialise one section's P2 twiddles: [Re: NTWI_P2][Im: NTWI_P2]
    p2_twi_s    = serialize_cmplx(p2_twi_v['re'] + 1j * p2_twi_v['im'], NTWI_P2, dtype)
    p2_twi_one  = np.empty(2 * NTWI_P2, dtype=dtype)
    p2_twi_one[0:NTWI_P2]          = p2_twi_s[0::2]
    p2_twi_one[NTWI_P2:2*NTWI_P2]  = p2_twi_s[1::2]

    # Duplicate for all NSEC sections so each can DMA independently
    p2_twi_all = np.tile(p2_twi_one, NSEC)

    # -----------------------------------------------------------------------
    # Combined twiddle DRAM array layout:
    #   [P1_Re: NTWI_P1][P1_Im: NTWI_P1]
    #   [P2_sec0_Re: NTWI_P2][P2_sec0_Im: NTWI_P2] ... (NSEC times)
    # -----------------------------------------------------------------------
    twiddle_dram = np.concatenate([p1_twi_reim, p2_twi_all])
    NTWI_TOT = NTWI_P1 + NSEC * NTWI_P2   # Re+Im halves each

    # -----------------------------------------------------------------------
    # Store indices for Phase 2 intermediate stages (vsuxei16 shuffle)
    # gen_store_idx operates on NFFTh-point sub-FFT
    # -----------------------------------------------------------------------
    [store_idx_list, store_delta] = gen_store_idx(NFFTh)

    # Convert element offsets to byte offsets (8 bytes per double)
    store_delta_bytes = [n * np.dtype(idx_dtype).itemsize for n in store_delta]

    # We use only half of each stage's index vector (upper wing; lower is derived)
    buf = []
    for i in range(len(store_delta_bytes) // NFFTh):
        buf += store_delta_bytes[i * NFFTh : i * NFFTh + NFFTh // 2]
    store_delta_bytes = buf
    # Number of entries: (log2_sec - 1) stages * NFFTh/2 per stage
    # (last stage uses strided store, no index needed)
    n_store_idx = int((log2_sec - 1) * NFFTh / 2)

    # -----------------------------------------------------------------------
    # Output buffer (zeroed, same size as samples)
    # -----------------------------------------------------------------------
    buffer_dram = np.zeros(2 * NFFT, dtype=dtype)

    # Gold output: serialised as [re0,im0,re1,im1,...] for verification
    gold_out_s = serialize_cmplx(gold_out, NFFT, dtype)

    # -----------------------------------------------------------------------
    # Emit header
    # -----------------------------------------------------------------------
    emit_str = (
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )

    # --- Constants ---
    emit_str += f'static uint32_t NFFT     = {NFFT};\n'
    emit_str += f'static uint32_t NSEC     = {NSEC};\n'
    emit_str += f'static uint32_t LOG2_NFFT = {log2_NFFT};\n'
    emit_str += f'static uint32_t LOG2_NSEC = {log2_S};\n'
    emit_str += f'static uint32_t NFFT_SEC = {NFFTh};\n'
    emit_str += f'static uint32_t LOG2_SEC = {log2_sec};\n'
    emit_str += f'static uint32_t NTWI_P1  = {NTWI_P1};\n'
    emit_str += f'static uint32_t NTWI_P2  = {NTWI_P2};\n'
    emit_str += f'static uint32_t NTWI_TOT = {NTWI_TOT};\n'
    emit_str += f'static uint32_t NCORES   = {CORES};\n'
    emit_str += '\n'

    # --- DRAM data arrays ---
    def arr(name, size, section, data, t):
        vals = ', '.join(map(str, data.astype(t).tolist()))
        return (f'static {c_type(t)} {name}[{size}]'
                f' __attribute__((section("{section}"))) = {{{vals}}};\n')

    def c_type(t):
        if t == np.float64:  return 'double'
        if t == np.uint64:   return 'uint64_t'
        if t == np.uint16:   return 'uint16_t'
        return str(t)

    emit_str += arr('samples_dram',   2 * NFFT,         '.data', samples_reim,           dtype)
    emit_str += arr('buffer_dram',    2 * NFFT,         '.data', buffer_dram,             dtype)
    emit_str += arr('twiddle_dram',   2 * (NTWI_P1 + NSEC * NTWI_P2), '.data', twiddle_dram, dtype)
    emit_str += arr('store_idx_dram', n_store_idx,       '.data',
                    np.array(store_delta_bytes),         np.uint64)
    emit_str += arr('gold_out_dram',  2 * NFFT,         '.data', gold_out_s,              dtype)

    # out_dram: output buffer, same size as samples, zeroed
    # Section s writes to out_dram + s with stride NSEC (via vsse64)
    # so layout is: [s0_elem0, s1_elem0, ..., sS-1_elem0, s0_elem1, ...]
    out_dram = np.zeros(2 * NFFT, dtype=dtype)
    emit_str += arr('out_dram',       2 * NFFT,         '.data', out_dram,                dtype)

    # -----------------------------------------------------------------------
    # Verbose: print layout summary
    # -----------------------------------------------------------------------
    if verbose:
        print(f"NFFT={NFFT}, NSEC={NSEC}, CORES={CORES}")
        print(f"NFFTh (per section) = {NFFTh}")
        print(f"log2_S (P1 stages)  = {log2_S}")
        print(f"log2_sec (P2 stages)= {log2_sec}")
        print(f"NTWI_P1             = {NTWI_P1} doubles (Re or Im)")
        print(f"NTWI_P2             = {NTWI_P2} doubles per section (Re or Im)")
        print(f"Total twiddle_dram  = {2*(NTWI_P1 + NSEC*NTWI_P2)} doubles")
        print(f"P1 slot L1 size     = {6 * NFFTh * 8} bytes  (6 * NFFTh doubles)")
        print(f"P2 slot L1 size     = {(2*NFFT + 2*NTWI_P2) * 8} bytes  "
              f"(2*NFFT + 2*NTWI_P2 doubles)")
        print(f"store_idx entries   = {n_store_idx}")

    # -----------------------------------------------------------------------
    # Write file
    # -----------------------------------------------------------------------
    file_path = pathlib.Path(__file__).parent.parent / 'data'
    file = file_path / f'data_{NFFT}_{NSEC}_{CORES}.h'
    with file.open('w') as f:
        f.write(emit_str)
    print(f"Written: {file}")


if __name__ == '__main__':
    main()
