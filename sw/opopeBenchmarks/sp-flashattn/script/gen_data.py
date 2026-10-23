#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator: single-head attention O = softmax(Q K^T / sqrt(d)) V, processed on the device
# per key tile. Layouts (FP32), chosen so that no transpose is needed:
#   qT      [d][Tq]            Q transposed (W operand of GeMM1)
#   kT      [T/Bc][d][Bc]      K tiles, each stored transposed (X operand of GeMM1)
#   v       [T/Bc][Bc][dv]     V tiles, natural layout (X operand of GeMM2, "transposed" V^T)
#   golden  [dv][Tq]           O^T

import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "common"))
import opope_data as od  # noqa: E402


def main():
    tag, p = od.parse_cfg()
    Tq, T, d, dv, Bc = (p[k] for k in ("Tq", "T", "d", "dv", "Bc"))
    assert Tq <= 64 and Tq >= 16 and Tq % 2 == 0  # one LMUL=4 vector per row of queries
    assert Bc % 16 == 0 and dv % 16 == 0 and d >= 16 and T % Bc == 0
    NT = T // Bc

    rng = np.random.default_rng(1337)
    q = rng.uniform(-1, 1, (Tq, d)).astype(np.float32)
    k = rng.uniform(-1, 1, (T, d)).astype(np.float32)
    v = rng.uniform(-1, 1, (T, dv)).astype(np.float32)

    s = (q.astype(np.float64) @ k.astype(np.float64).T) / np.sqrt(d)
    s = s - s.max(axis=1, keepdims=True)
    pr = np.exp(s)
    pr = pr / pr.sum(axis=1, keepdims=True)
    o = (pr @ v.astype(np.float64)).astype(np.float32)  # [Tq][dv]

    kt_tiles = np.stack([k[t * Bc:(t + 1) * Bc].T for t in range(NT)])  # [NT][d][Bc]

    od.check_footprint({"qT": 4 * d * Tq, "kT": 4 * T * d, "v": 4 * T * dv, "st": 4 * Bc * Tq,
                        "oT": 4 * dv * Tq, "m": 4 * Tq, "l": 4 * Tq, "alpha": 4 * Tq})

    defines = {"TQ": Tq, "T_SIZE": T, "D_SIZE": d, "DV_SIZE": dv, "BC": Bc, "NT": NT,
               "SCALE_F": "%.9ff" % (1.0 / np.sqrt(d)), "INV_T_F": "%.9ff" % (1.0 / T)}
    arrays = [od.carray("fa_qt_dram", od.words(q.T.copy())),
              od.carray("fa_kt_dram", od.words(kt_tiles)),
              od.carray("fa_v_dram", od.words(v)),
              od.carray("fa_golden", od.words(o.T.copy()))]
    od.write_header(pathlib.Path(__file__).resolve().parents[1], tag, defines, arrays)


if __name__ == "__main__":
    main()
