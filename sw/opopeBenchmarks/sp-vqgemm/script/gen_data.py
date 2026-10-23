#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: vector-quantised weights (residual codebooks) + activations. Layouts (FP32):
#   xT        [N][M]            activations, stored transposed (the O-POPE X operand)
#   codebooks [NCB][CBN][CB_D]
#   idx       [NCB][N][K/CB_D]  uint16 code indices
#   scale     [N]
#   golden    [M][K]

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    M, N, K, CBD, CBN, NCB = (p[k] for k in ("M", "N", "K", "CB_D", "CBN", "NCB"))
    assert M % 16 == 0 and min(M, N, K) >= 16 and K % CBD == 0 and CBD <= 128
    assert NCB in (1, 2) and CBN <= 65536
    G = K // CBD

    rng = np.random.default_rng(1337)
    cb = rng.uniform(-0.5, 0.5, (NCB, CBN, CBD)).astype(np.float32)
    idx = rng.integers(0, CBN, (NCB, N, G)).astype(np.uint16)
    scale = rng.uniform(0.5, 1.5, N).astype(np.float32)
    x = rng.uniform(-1, 1, (M, N)).astype(np.float32)

    w = np.zeros((N, G, CBD), dtype=np.float32)
    for c in range(NCB):
        w = w + cb[c][idx[c]]  # [N][G][CBD] in float32, same order as the device
    w = (w * scale[:, None, None]).reshape(N, K).astype(np.float32)
    z = (x.astype(np.float64) @ w.astype(np.float64)).astype(np.float32)

    od.check_footprint({"xT": 4 * N * M, "w": 4 * N * K, "z": 4 * M * K,
                        "codebooks": 4 * NCB * CBN * CBD, "idx": 2 * NCB * N * G,
                        "scale": 4 * N})

    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K, "CB_D": CBD, "CBN": CBN, "NCB": NCB,
               "GROUPS": G}
    arrays = [od.carray("vq_xt_dram", od.words(x.T.copy())),
              od.carray("vq_cb_dram", od.words(cb)),
              od.carray("vq_idx_dram", idx.flatten(), ctype="uint16_t"),
              od.carray("vq_scale_dram", od.words(scale)),
              od.carray("vq_golden", od.words(z))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
