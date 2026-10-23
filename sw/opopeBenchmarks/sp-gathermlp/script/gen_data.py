#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator for embedding-bag -> MLP (DLRM) and GCN-layer pipelines. Layouts (FP32):
#   table  T[R][D]     row-major: a gather is a unit-stride row load at a scalar-computed address
#   idx    [B][L]      u32 row indices looked up by each of the B bags
#   scale  [B]         per-bag multiplier applied after pooling (GCN: 1/L, DLRM: 1)
#   w1t    [D][Dout]   MLP weights, "in x out" (the O-POPE X operand, already transposed)
#   bias   [Dout]
#   golden Z[Dout][B]  feature-major MLP output after ReLU

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    mode, R, D, B, L, Dout = (p[k] for k in ("mode", "R", "D", "B", "L", "Dout"))
    assert mode in ("dlrm", "gcn")
    assert Dout % 16 == 0 and D % 16 == 0 and min(D, B) >= 16  # O-POPE: M multiple of 16 (Dout, D)
    assert D % 2 == 0 and B % 2 == 0

    rng = np.random.default_rng(1337)
    table = rng.uniform(-1, 1, (R, D)).astype(np.float32)  # [R][D]
    if mode == "gcn":
        assert R == B
        idx = np.zeros((B, L), dtype=np.int64)
        idx[:, 0] = np.arange(B)  # self loop
        for v in range(B):
            idx[v, 1:] = rng.choice(R, L - 1, replace=False)  # neighbours
        scale = np.full(B, 1.0 / L, dtype=np.float32)
    else:
        idx = rng.integers(0, R, (B, L))
        scale = np.ones(B, dtype=np.float32)
    w1t = rng.uniform(-0.5, 0.5, (D, Dout)).astype(np.float32)
    bias = rng.uniform(-0.5, 0.5, Dout).astype(np.float32)

    pooled = table[idx].astype(np.float64).sum(axis=1) * scale[:, None]  # [B][D]
    z = np.maximum(w1t.astype(np.float64).T @ pooled.T + bias[:, None], 0).astype(np.float32)

    idx32 = idx.astype(np.uint32)  # [B][L]

    od.check_footprint({"table": 4 * R * D, "idx": 4 * B * L, "scale": 4 * B,
                        "w1t": 4 * D * Dout, "bias": 4 * Dout, "pooled_rows": 4 * B * D,
                        "ident": 4 * B * B, "pool": 4 * D * B, "z": 4 * Dout * B})

    defines = {"MODE_GCN": int(mode == "gcn"), "R_SIZE": R, "D_SIZE": D, "B_SIZE": B, "L_SIZE": L,
               "DOUT_SIZE": Dout}
    arrays = [od.carray("gm_table_dram", od.words(table)),
              od.carray("gm_idx_dram", idx32.flatten()),
              od.carray("gm_scale_dram", od.words(scale)),
              od.carray("gm_w1t_dram", od.words(w1t)),
              od.carray("gm_bias_dram", od.words(bias)),
              od.carray("gm_golden", od.words(z))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
