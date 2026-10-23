#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: valid conv2d + bias + ReLU. Layouts (all FP32):
#   input   [Cin][H][W]            (CHW)
#   weights [Cin*KH*KW][Cout]      (the O-POPE W operand, prepared offline)
#   bias    [Cout]
#   golden  [Ho*Wo][Cout]          (spatial-major, only the valid output positions)

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    Cin, H, W, Cout, KH, KW = (p[k] for k in ("Cin", "H", "W", "Cout", "KH", "KW"))
    Ho, Wo = H - KH + 1, W - KW + 1
    M, N, K = Ho * W, Cin * KH * KW, Cout
    assert W % 16 == 0 and M % 16 == 0 and min(M, N, K) >= 16
    assert K % 2 == 0 and N % 2 == 0

    rng = np.random.default_rng(1337)
    x = rng.uniform(-1, 1, (Cin, H, W)).astype(np.float32)
    w = rng.uniform(-0.5, 0.5, (Cout, Cin, KH, KW)).astype(np.float32)
    b = rng.uniform(-0.5, 0.5, Cout).astype(np.float32)

    out = np.zeros((Ho, Wo, Cout), dtype=np.float64)
    for ky in range(KH):
        for kx in range(KW):
            patch = x[:, ky:ky + Ho, kx:kx + Wo].astype(np.float64)  # Cin,Ho,Wo
            out += np.einsum("chw,oc->hwo", patch, w[:, :, ky, kx].astype(np.float64))
    out = np.maximum(out + b.astype(np.float64), 0).astype(np.float32).reshape(Ho * Wo, Cout)

    # W operand: row n = (c*KH+ky)*KW+kx, column co
    wmat = w.transpose(1, 2, 3, 0).reshape(N, Cout)

    # L1 buffers: input (+4 floats: the shifted im2col copies read slightly past the plane),
    # xT [N][M], W [N][K], Z/Y [M][K], bias [K]
    od.check_footprint({"input": 4 * (Cin * H * W + 4), "xT": 4 * N * M, "w": 4 * N * K,
                        "z": 4 * M * K, "bias": 4 * K})

    defines = {"CIN": Cin, "H_SIZE": H, "W_SIZE": W, "COUT": Cout, "KH_SIZE": KH, "KW_SIZE": KW,
               "HO_SIZE": Ho, "WO_SIZE": Wo, "M_SIZE": M, "N_SIZE": N, "K_SIZE": K}
    arrays = [od.carray("cv_in_dram", od.words(x)), od.carray("cv_w_dram", od.words(wmat)),
              od.carray("cv_bias_dram", od.words(b)), od.carray("cv_golden", od.words(out))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
