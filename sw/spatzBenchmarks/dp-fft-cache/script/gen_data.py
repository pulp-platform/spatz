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

# arg1: input size, arg2: num_cores

import numpy as np
import copy
import argparse
import pathlib
import hjson

np.random.seed(42)

FFT2_SAMPLE_DYN = 13
FFT_TWIDDLE_DYN = 15


def serialize_cmplx(vector, NFFT, dtype):
    # Split the real and imaginary parts
    vector_re = np.real(vector)
    vector_im = np.imag(vector)
    # Serialize the vectors
    serial_vec = np.empty(2 * NFFT, dtype=dtype)
    serial_vec[0::2] = vector_re
    serial_vec[1::2] = vector_im
    return serial_vec

############
#   DATA   #
############


def setupInput(samples, Nfft, dyn):
    with np.nditer(samples, op_flags=['readwrite']) as it:
        for samp in it:
            samp[...]['re'] = np.random.randn(1)
            samp[...]['im'] = np.random.randn(1)

############
# TWIDDLES #
############


def setupTwiddlesLUT(Twiddles_vec, Nfft):
    Theta = (2 * np.pi) / Nfft
    with np.nditer(Twiddles_vec, op_flags=['readwrite']) as it:
        for idx, twi in enumerate(it):
            Phi = 2 * np.pi - Theta * (idx)
            twi[...]['re'] = np.cos(Phi)
            twi[...]['im'] = np.sin(Phi)


def setupTwiddlesLUT_dif_vec(Twiddles_vec, Nfft):
    # Nfft power of 2
    stages = int(np.log2(Nfft))
    Theta = (2 * np.pi) / Nfft
    # Twiddle factors ([[twi0_re, twi0_im], [twi1_re, twi1_im]])
    twi = [[np.cos((2 * np.pi) - i * Theta), np.sin((2 * np.pi) - i * Theta)]
           for i in range(int(Nfft / 2))]
    # Write the Twiddle factors
    for s in range(stages):
        for t in range(int(Nfft / 2)):
            Twiddles_vec[int(s * Nfft / 2 + t)
                         ]['re'] = twi[int((2**(s) * t) % int(Nfft / 2))][0]
            Twiddles_vec[int(s * Nfft / 2 + t)
                         ]['im'] = twi[int((2**(s) * t) % int(Nfft / 2))][1]
    return Twiddles_vec

    with np.nditer(Twiddles_vec, op_flags=['readwrite']) as it:
        for idx, twi in enumerate(it):
            Phi = Theta * idx
            twi[...]['re'] = np.cos(Phi)
            twi[...]['im'] = np.sin(Phi)

###############
# BITREVERSAL #
###############


def gen_bitrev_idx(nfft):
    fmt = '{:0' + str(int(np.log2(nfft))) + 'b}'
    bitrev = np.zeros(nfft)
    for n in np.arange(nfft):
        bitrev[n] = int(fmt.format(n)[::-1], 2)
    return bitrev


def gen_store_idx(nfft):
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
                    b[h * (nffth >> 1) + i * stride + j] = old_b[h * nffth + j + 2 * i * stride]
                    b[h * (nffth >> 1) + i * stride + j + nffth] = old_b[h * nffth + j + 2 * i * stride + stride]
        delta = [[i for i in b].index(n) for n in old_b]
        ibuf += [[i for i in b]]
        dbuf += [delta]
        old_b = [n for n in copy.deepcopy(b)]
        b = np.zeros(nfft)
    idx_list = sum(ibuf, [])
    delta_list = sum(dbuf, [])
    return [idx_list, delta_list]


##########
# SCRIPT #
##########


def main():

    parser = argparse.ArgumentParser(description='Generate data for kernels')
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help='Select param config file kernel'
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action='store_true',
        help='Set verbose'
    )

    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    NFFT = param['npoints']
    CORES = param['ncores']
    NFFTh = NFFT // CORES
    N_TWID_V = int(np.log2(NFFTh) * NFFTh / 2)

    dtype = np.float64
    idx_dtype = np.uint64
    # Complex data type with int16 for real and img parts
    dtype_cplx = np.dtype([('re', dtype), ('im', dtype)])

    # Vector of samples
    samples = np.empty(NFFT, dtype=dtype_cplx)
    twiddle = np.empty(NFFTh, dtype=dtype_cplx)
    twiddle_v = np.empty(N_TWID_V, dtype=dtype_cplx)
    gold_out = np.empty(NFFT, dtype=dtype_cplx)

    # Initialize the twiddle factors
    setupTwiddlesLUT(twiddle, NFFTh)
    twiddle_v = setupTwiddlesLUT_dif_vec(twiddle_v, NFFTh)

    # Initialize the input samples
    setupInput(samples, NFFT, FFT2_SAMPLE_DYN)

    # Calculate the golden FFT
    # print(samples)
    # print(samples['re'] + 1j * samples['im'])
    gold_out = np.fft.fft(samples['re'] + 1j * samples['im'])
    # print(gold_out)

    # Serialize the complex array
    samples_s = serialize_cmplx(
        samples['re'] + 1j * samples['im'], NFFT, dtype)
    twiddle_v_s = serialize_cmplx(
        twiddle_v['re'] + 1j * twiddle_v['im'], N_TWID_V, dtype)
    gold_out_s = serialize_cmplx(gold_out, NFFT, dtype)

    # Create the sequential vectors - Real, and Imaginary
    samples_reim = np.empty(2 * NFFT, dtype=dtype)
    samples_reim[0:NFFT] = samples_s[0::2]
    samples_reim[NFFT:2 * NFFT] = samples_s[1::2]

    twiddle_vec_reim = np.empty(2 * N_TWID_V, dtype=dtype)
    twiddle_vec_reim[0:N_TWID_V] = twiddle_v_s[0::2]
    twiddle_vec_reim[N_TWID_V:2 * N_TWID_V] = twiddle_v_s[1::2]

    # Generate indices for intermediate stores (if masks are not supported)
    [store_idx, store_delta] = gen_store_idx(NFFTh)
    # Get the last store index vector
    last_si = store_idx[-NFFTh:]
    # Convert to byte array
    store_delta = [n * np.dtype(idx_dtype).itemsize for n in store_delta]
    # We need only half of this vector
    buf = []
    for i in range(len(store_delta) // NFFTh):
        buf += store_delta[i * NFFTh:i * NFFTh + NFFTh // 2]
    store_delta = buf

    # Generate the bitrev pattern
    buf = gen_bitrev_idx(NFFTh)
    bitrev = [[i for i in buf].index(n) for n in last_si]
    bitrev = np.array([n * np.dtype(idx_dtype).itemsize for n in bitrev])
    # We need only half of this vector
    bitrev = bitrev[:len(bitrev) // 2]
    # If two cores, the bitrev idx vector is different and we need an additional twi layer
    if CORES == 2:
        # Bitrev
        buf = copy.deepcopy(bitrev)
        bitrev = [2 * i for i in buf]
        # Twi
        N_T_BUF = int(np.log2(NFFTh * 2) * NFFTh)
        twiddle = np.empty(NFFTh * 2, dtype=dtype_cplx)
        twiddle_v = np.empty(N_T_BUF, dtype=dtype_cplx)
        setupTwiddlesLUT(twiddle, 2 * NFFTh)
        twiddle_v = setupTwiddlesLUT_dif_vec(twiddle_v, 2 * NFFTh)
        twiddle_v_s = serialize_cmplx(
            twiddle_v['re'] + 1j * twiddle_v['im'], N_T_BUF, dtype)
        tbuf = np.empty(2 * N_T_BUF, dtype=dtype)
        tbuf[0:N_T_BUF] = twiddle_v_s[0::2]
        tbuf[N_T_BUF:2 * N_T_BUF] = twiddle_v_s[1::2]
        # Attach 1bf img part
        twiddle_vec_reim = np.concatenate(
            (tbuf[N_T_BUF:N_T_BUF + NFFTh], twiddle_vec_reim))
        # Attach 1bf real part
        twiddle_vec_reim = np.concatenate((tbuf[:NFFTh], twiddle_vec_reim))

    # Generate buffer for intermediate butterflies
    buffer_dram = np.zeros(2 * NFFT)

    # License
    emit_str = (
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    # store_delta = [0, 2, 4, 6, 8, 10, 12, 14]
    # bitrev = [0, 4, 2, 6, 1, 5, 3, 7]
    # Create the file
    emit_str += 'static uint32_t NFFT = {};\n'.format(NFFT)
    emit_str += 'static uint32_t NTWI = {};\n\n'.format(N_TWID_V)

    emit_str += 'static double samples_dram[{}]'.format(2 * NFFT) + ' __attribute__((section(".data"))) = {' + ', '.join(
        map(str, samples_reim.astype(dtype).tolist())) + '};\n'
    emit_str += 'static double buffer_dram[{}]'.format(2 * NFFT) + ' __attribute__((section(".data"))) = {' + ', '.join(
        map(str, buffer_dram.astype(dtype).tolist())) + '};\n'
    if CORES == 1:
        emit_str += 'static double twiddle_dram[{}]'.format(2 * N_TWID_V) + ' __attribute__((section(".data"))) = {' + ', '.join(
            map(str, twiddle_vec_reim.astype(dtype).tolist())) + '};\n'
    else:
        emit_str += 'static double twiddle_dram[{}]'.format(2 * (N_TWID_V + NFFTh)) + ' __attribute__((section(".data"))) = {' + ', '.join(
            map(str, twiddle_vec_reim.astype(dtype).tolist())) + '};\n'
    emit_str += 'static uint16_t store_idx_dram[{}]'.format(int(np.log2(NFFTh / 2) * NFFTh / 2)) + ' __attribute__((section(".data"))) = {' + ', '.join(
        map(str, np.array(store_delta).astype(idx_dtype).tolist())) + '};\n'
    emit_str += 'static uint16_t bitrev_dram[{}]'.format(int(
        NFFTh / 2)) + ' __attribute__((section(".data"))) = {' + ', '.join(map(str, bitrev)) + '};\n'
    emit_str += 'static double gold_out_dram[{}]'.format(
        2 * NFFT) + ' __attribute__((section(".data"))) = {' + ', '.join(map(str, gold_out_s.astype(dtype).tolist())) + '};\n'

    file_path = pathlib.Path(__file__).parent.parent / 'data'
    file = file_path / ('data_' + str(NFFT) + "_" + str(CORES) + ".h")
    with file.open('w') as f:
        f.write(emit_str)


if __name__ == '__main__':
    main()
