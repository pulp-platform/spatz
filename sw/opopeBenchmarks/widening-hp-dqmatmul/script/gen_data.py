#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: block-codebook dequantised weights (no scale) (2 additive FP16 pair-block codebooks, see common/pairblock.py) + FP16 activations, FP32 output.
#   xT        [N/2][M] words : the packed X^T operand, word (p,m) = {X[m][2p], X[m][2p+1]} (offline)
#   cb        [2][CBN][CB_W] words : pair-block codewords {row 2p, row 2p+1} x CB_W columns, already packed
#   idx       [N/2][K/CB_W/2] words  7-bit fields {cb0, cb1 index of group 2h; cb0, cb1 index of group 2h+1}
#   golden    [M][K] FP32
# The decode is linear, so it runs in FP16 on Spatz: w = cb0[i0] + cb1[i1] with FP16 rounding (both rows of the pair at
# once); the golden emulates it bit-exactly (Spatz FP16 arithmetic matches numpy float16).

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402
import pairblock  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    M, N, K, CBW, CBN = (p[k] for k in ("M", "N", "K", "CB_W", "CBN"))
    assert M % 16 == 0 and min(M, N, K) >= 16
    rng = np.random.default_rng(1337)
    d = pairblock.make(rng, M, N, K, CBW, CBN, False)

    od.check_footprint({"xT": 4 * (N // 2) * M, "w": 4 * (N // 2) * K, "z": 4 * M * K,
                        "codebooks": 4 * 2 * CBN * CBW, "idx": 4 * d["P"] * (d["G"] // 2)})

    defines = {"M_SIZE": M, "N_SIZE": N, "K_SIZE": K, "CB_W": CBW, "CBN": CBN, "NCB": 2, "GROUPS": d["G"]}
    arrays = [od.carray("dq_xt_dram", d["xT"].flatten()),
              od.carray("dq_cb_dram", d["cbw"].flatten()),
              od.carray("dq_idx_dram", d["idxw"].flatten()),
              od.carray("dq_golden", od.words(d["z"]))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
