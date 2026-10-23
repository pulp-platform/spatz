#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: widening FP16 -> FP32 GeMM (O-POPE SDOTP). Z = Y + X*W, X/W FP16, Y/Z FP32.
# Memory layouts (see include/opope_app.h, opope_gemm_fp16): two FP16 per 32-bit word along N,
#   xT [N/2][M] words, w [N/2][K] words, y [M][K] FP32.
# The golden is computed in float64 on the FP16-quantised inputs (FP16 products are exact in FP32).

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def pack_pairs(h16):
    """uint16 [A][B] (pairs along axis 0) -> uint32 [A/2][B]: lo = even row, hi = odd row"""
    a = h16.astype(np.uint32)
    return a[0::2, :] | (a[1::2, :] << 16)


def main():
    tag, p = od.parse_cfg()
    M, N, K = p["M"], p["N"], p["K"]
    assert M % 16 == 0 and N % 2 == 0 and min(M, N, K) >= 16

    rng = np.random.default_rng(1337)
    x = rng.uniform(0, 1, (M, N)).astype(np.float16)
    w = rng.uniform(0, 1, (N, K)).astype(np.float16)
    y = rng.uniform(0, 1, (M, K)).astype(np.float32)
    z = (y.astype(np.float64) + x.astype(np.float64) @ w.astype(np.float64)).astype(np.float32)

    xT = pack_pairs(x.T.view(np.uint16))  # x.T [N][M] -> [N/2][M]
    wp = pack_pairs(w.view(np.uint16))    # [N][K] -> [N/2][K]

    od.check_footprint({"xT": 4 * (N // 2) * M, "w": 4 * (N // 2) * K, "z": 4 * M * K})
    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K}
    arrays = [od.carray("wm_xt_dram", xT.flatten()), od.carray("wm_w_dram", wp.flatten()),
              od.carray("wm_y_dram", od.words(y)), od.carray("wm_golden", od.words(z))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
