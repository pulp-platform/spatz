#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: vector-quantised weights (residual FP16 codebooks) + FP16 activations, FP32 output.
#   xT        [N/2][M] words : the packed X^T operand, word (p,m) = {X[m][2p], X[m][2p+1]} (offline)
#   codebooks [NCB][CBN][CB_D] FP16
#   idx       [NCB][N][K/CB_D]  uint16 code indices
#   scale     [N] FP16
#   golden    [M][K] FP32
# The decode is linear, so it runs in FP16 on Spatz: w = (cb0[i0] + cb1[i1]) * scale, with FP16 rounding
# after every operation; the golden emulates it bit-exactly (Spatz FP16 arithmetic matches numpy float16).

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    M, N, K, CBD, CBN, NCB = (p[k] for k in ("M", "N", "K", "CB_D", "CBN", "NCB"))
    assert M % 16 == 0 and min(M, N, K) >= 16 and K % CBD == 0 and N % 2 == 0
    assert CBD == 16, "the device decode uses one LMUL=1 e16 register (16 elements) per code"
    assert NCB in (1, 2) and CBN <= 65536
    G = K // CBD

    rng = np.random.default_rng(1337)
    cb = rng.uniform(-0.5, 0.5, (NCB, CBN, CBD)).astype(np.float16)
    idx = rng.integers(0, CBN, (NCB, N, G)).astype(np.uint16)
    scale = rng.uniform(0.5, 1.5, N).astype(np.float16)
    x = rng.uniform(-1, 1, (M, N)).astype(np.float16)

    w = cb[0][idx[0]]  # [N][G][CBD] FP16
    if NCB == 2:
        w = (w + cb[1][idx[1]]).astype(np.float16)
    w = (w * scale[:, None, None]).astype(np.float16).reshape(N, K)
    z = (x.astype(np.float64) @ w.astype(np.float64)).astype(np.float32)

    xT = od.pack_pairs(od.halves(x.T).reshape(N, M))  # [N/2][M] words

    od.check_footprint({"xT": 4 * (N // 2) * M, "w": 4 * (N // 2) * K, "z": 4 * M * K,
                        "codebooks": 2 * NCB * CBN * CBD, "idx": 2 * NCB * N * G,
                        "scale": 2 * N})

    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K, "CB_D": CBD, "CBN": CBN, "NCB": NCB,
               "GROUPS": G}
    arrays = [od.carray("vq_xt_dram", xT.flatten()),
              od.carray("vq_cb_dram", od.halves(cb), ctype="uint16_t"),
              od.carray("vq_idx_dram", idx.flatten(), ctype="uint16_t"),
              od.carray("vq_scale_dram", od.halves(scale), ctype="uint16_t"),
              od.carray("vq_golden", od.words(z))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
