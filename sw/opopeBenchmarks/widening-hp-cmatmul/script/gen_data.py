#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: complex GeMM C = A*B with FP16 complex inputs (one 32-bit word {re (lo), im (hi)} per
# element) and FP32 complex output (interleaved Re, Im).
#   a_t    [N][M] words : A TRANSPOSED (the packed X^T operand: the word {Ar, Ai} is a pair along the
#                         contraction dimension, so no packing is needed)
#   b      [N][K] words : B row-major
#   golden [M][K] complex FP32, interleaved (= the O-POPE output [M][2K], column 2k = Cr, 2k+1 = Ci)

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def cwords(z):
    """complex FP16-quantised array -> uint32 words {re (lo), im (hi)}"""
    re = od.halves(z.real).reshape(z.shape).astype(np.uint32)
    im = od.halves(z.imag).reshape(z.shape).astype(np.uint32)
    return (re | (im << 16)).flatten()


def main():
    tag, p = od.parse_cfg()
    M, N, K = p["M"], p["N"], p["K"]
    assert min(M, N, K) >= 16 and M % 16 == 0

    rng = np.random.default_rng(1337)
    A = (rng.uniform(-1, 1, (M, N)) + 1j * rng.uniform(-1, 1, (M, N)))
    B = (rng.uniform(-1, 1, (N, K)) + 1j * rng.uniform(-1, 1, (N, K)))
    A = A.real.astype(np.float16) + 1j * A.imag.astype(np.float16)  # quantise Re/Im to FP16
    B = B.real.astype(np.float16) + 1j * B.imag.astype(np.float16)
    C = A.astype(np.complex128) @ B.astype(np.complex128)
    cgold = od.words(np.stack([C.real, C.imag], axis=-1))  # [M][K][2]

    od.check_footprint({"a_t": 4 * N * M, "b": 4 * N * K, "w": 4 * N * 2 * K, "c": 4 * M * 2 * K})

    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K}
    arrays = [od.carray("cm_at_dram", cwords(A.T.copy())), od.carray("cm_b_dram", cwords(B)),
              od.carray("cm_golden", cgold)]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
