#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator for embedding-bag -> MLP (DLRM) and GCN-layer pipelines, FP16 inputs / FP32 output.
#   table  [R][D] FP16, stored per row as [even features (D/2) | odd features (D/2)]: the pooled row
#          can then be written with two strided stores straight into the packed W operand
#   idx    [B][L]    u32 row indices looked up by each of the B bags
#   w1     [D/2][Dout] words {W1[2p][o], W1[2p+1][o]}  (packed X^T operand of the MLP GeMM, prepared offline)
#   bias   [Dout] FP32 (initialises Y)
#   golden Z[Dout][B] FP32, feature-major, after ReLU
# The golden emulates the FP16 sequential sum-pooling (and the FP16 mean scale of the GCN) exactly.

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[3] / 'script'))
import vme_data as od  # noqa: E402


def main():
    tag, p = "matched", {'mode': 'dlrm', 'R': 144, 'D': 64, 'B': 127, 'L': 8, 'Dout': 128}
    mode, R, D, B, L, Dout = (p[k] for k in ("mode", "R", "D", "B", "L", "Dout"))
    assert mode in ("dlrm", "gcn")
    assert D == 64
    assert Dout % 16 == 0 and B >= 16

    rng = np.random.default_rng(1337)
    table = rng.uniform(-1, 1, (R, D)).astype(np.float16)
    if mode == "gcn":
        assert R == B
        idx = np.zeros((B, L), dtype=np.int64)
        idx[:, 0] = np.arange(B)  # self loop
        for v in range(B):
            idx[v, 1:] = rng.choice(R, L - 1, replace=False)  # neighbours
    else:
        idx = rng.integers(0, R, (B, L))
    w1 = rng.uniform(-0.5, 0.5, (D, Dout)).astype(np.float16)
    bias = rng.uniform(-0.5, 0.5, Dout).astype(np.float32)

    # FP16 sequential sum-pooling (same order as the device), then the FP16 mean scale (GCN)
    pooled = np.zeros((B, D), dtype=np.float16)
    for l in range(L):
        pooled = (pooled + table[idx[:, l]]).astype(np.float16)
    if mode == "gcn":
        pooled = (pooled * np.float16(1.0 / L)).astype(np.float16)
    z = np.maximum(w1.astype(np.float64).T @ pooled.astype(np.float64).T + bias[:, None], 0)
    z = z.astype(np.float32)

    # Match O-POPE 606: natural table rows and four u8 indices per word.
    # Reinterpreting a natural FP16 row as e32 yields {odd, even}; A uses the
    # same pair order so vtfmm computes even*even + odd*odd.
    ii = idx.astype(np.uint32)
    idxw = (ii[:, 0::4] | (ii[:, 1::4] << 8) |
            (ii[:, 2::4] << 16) | (ii[:, 3::4] << 24)).astype(np.uint32)
    aw = w1.T.view(np.uint16).astype(np.uint32)
    w1p = (aw[:, 0::2] | (aw[:, 1::2] << 16))
    w1p = w1p.reshape(Dout // 16, 16, D // 2).transpose(0, 2, 1).copy()

    # VME tiles require an N multiple of 16.  Keep the latest 127-bag
    # workload and append one inert column after all RNG-dependent data has
    # been generated, so the first 127 columns and golden are bit-identical.
    BP = (B + 15) & ~15
    od.check_footprint({"table": 2 * R * D, "idx": 4 * B * (L // 4), "pool": 4 * (D // 2) * BP,
                        "w1": 4 * (D // 2) * Dout, "z": 4 * Dout * BP})

    defines = {"MODE_GCN": int(mode == "gcn"), "R_SIZE": R, "D_SIZE": D, "B_SIZE": BP,
               "VALID_B_SIZE": B, "L_SIZE": L, "DOUT_SIZE": Dout}
    arrays = [od.carray("gm_table_dram", od.halves(table), ctype="uint16_t"),
              od.carray("gm_idx_dram", idxw.flatten()),
              od.carray("gm_w1_dram", w1p.flatten()),
              od.carray("gm_zinit_dram", od.words(np.repeat(bias[:, None], BP, axis=1))),
              od.carray("gm_golden", od.words(z))]
    if mode == "gcn":
        defines["SCALE_F16_BITS"] = "0x%04x" % int(np.float16(1.0 / L).view(np.uint16))
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    out = pathlib.Path(parser.parse_args().output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('#include <stdint.h>\n' + ''.join(f'#define {k} {v}\n' for k,v in defines.items()) + '\n'.join(arrays))


if __name__ == "__main__":
    main()
