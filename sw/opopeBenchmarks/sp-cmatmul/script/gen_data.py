#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: complex GeMM C = A*B, operands/results stored interleaved (Re, Im) in DRAM.

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    M, N, K = p["M"], p["N"], p["K"]
    assert min(M, N, K) >= 16
    assert M % 16 == 0, "O-POPE needs M multiple of 16 (it writes garbage rows past Z otherwise)"
    assert N % 2 == 0 and K % 2 == 0  # even vl (strided accesses)

    rng = np.random.default_rng(1337)
    A = (rng.uniform(-1, 1, (M, N)) + 1j * rng.uniform(-1, 1, (M, N))).astype(np.complex64)
    B = (rng.uniform(-1, 1, (N, K)) + 1j * rng.uniform(-1, 1, (N, K))).astype(np.complex64)
    C = (A.astype(np.complex128) @ B.astype(np.complex128)).astype(np.complex64)

    def interleave(z):
        return od.words(np.stack([z.real, z.imag], axis=-1))

    raw = max(2 * M * N + 2 * N * K, 2 * M * K)
    planes = max(2 * M * N + 3 * N * K, 2 * M * K)
    od.check_footprint({"raw+C": 4 * raw, "planes": 4 * planes})

    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K}
    # A is stored transposed (N x M) so that no transpose is needed on the device
    arrays = [od.carray("cm_a_dram", interleave(A.T.copy())),
              od.carray("cm_b_dram", interleave(B)),
              od.carray("cm_golden", interleave(C))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
