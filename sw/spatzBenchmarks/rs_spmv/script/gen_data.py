#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import argparse
import pathlib

import hjson
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate row-sparse SpMV benchmark data"
    )
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help="HJSON configuration file",
    )
    parser.add_argument("--M", type=int, help="Number of rows in dense A")
    parser.add_argument("--K", type=int, help="Number of columns in dense A (= length of x)")
    parser.add_argument("--nz-rows", type=int, dest="nz_rows", help="Number of non-zero rows")
    parser.add_argument("--seed", type=int, help="Random seed")
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Print generation summary"
    )
    return parser.parse_args()


def override(cfg, args):
    out = dict(cfg)
    for key in ("M", "K", "seed"):
        value = getattr(args, key, None)
        if value is not None:
            out[key] = value
    if args.nz_rows is not None:
        out["nz_rows"] = args.nz_rows
    return out


def validate(cfg):
    for key in ("M", "K", "nz_rows"):
        if key not in cfg:
            raise ValueError(f"Missing required config field '{key}'")

    m = int(cfg["M"])
    k = int(cfg["K"])
    nz_rows = int(cfg["nz_rows"])
    seed = int(cfg.get("seed", 42))

    if m <= 0 or k <= 0:
        raise ValueError("M and K must be positive")
    if nz_rows < 0 or nz_rows > m:
        raise ValueError("nz_rows must be in [0, M]")

    return {"M": m, "K": k, "nz_rows": nz_rows, "seed": seed}


def generate(cfg):
    rng = np.random.default_rng(cfg["seed"])
    m = cfg["M"]
    k = cfg["K"]
    nz_rows = cfg["nz_rows"]

    # Pick nz_rows random sorted row indices from [0, M).
    nz_row_idx = np.sort(rng.choice(m, size=nz_rows, replace=False)).astype(np.uint32)

    # Generate dense_a: random values for non-zero rows, zeros for others.
    dense_a = np.zeros((m, k), dtype=np.float64)
    for row in nz_row_idx:
        dense_a[row, :] = rng.standard_normal(k).astype(np.float64)

    # Generate random x.
    x = rng.standard_normal(k).astype(np.float64)

    # Compute y = A * x.
    y = dense_a @ x

    checksum = float(np.sum(y.astype(np.float64)))

    return nz_row_idx, dense_a, x, y, checksum


def scalar_to_c_str(value):
    return repr(float(value))


def array_to_c_str(array, is_float=False):
    elems = []
    for value in np.asarray(array).reshape(-1):
        if is_float:
            elems.append(scalar_to_c_str(value))
        else:
            elems.append(str(int(value)))
    return "{\n\t" + ",\n\t".join(elems) + "\n}"


def emit_header(cfg, nz_row_idx, dense_a, x, y, checksum):
    data_dir = pathlib.Path(__file__).parent.parent / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    m = cfg["M"]
    k = cfg["K"]
    nz_rows = cfg["nz_rows"]

    file_path = data_dir / f"data_{m}_{k}_{nz_rows}_64.h"

    header = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
        '#include "layer.h"\n\n'
        f"const rs_spmv_layer rs_spmv_l = {{.M = {m}, .K = {k}, "
        f".nz_rows = {nz_rows}, .dtype = FP64}};\n\n"
        f"static double rs_spmv_dense_a_dram[{m * k}] "
        f'__attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(dense_a, is_float=True)};\n"
        f"static uint32_t rs_spmv_nz_row_idx_dram[{nz_rows}] "
        f'__attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(nz_row_idx)};\n"
        f"static double rs_spmv_x_dram[{k}] "
        f'__attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(x, is_float=True)};\n"
        f"static double rs_spmv_result[{m}] "
        f'__attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(y, is_float=True)};\n"
        f"static double rs_spmv_checksum __attribute__((aligned(8))) = "
        f"{repr(checksum)};\n"
    )

    file_path.write_text(header)
    return file_path


def main():
    args = parse_args()
    with args.cfg.open() as f:
        cfg = override(hjson.loads(f.read()), args)

    cfg = validate(cfg)
    nz_row_idx, dense_a, x, y, checksum = generate(cfg)
    file_path = emit_header(cfg, nz_row_idx, dense_a, x, y, checksum)

    if args.verbose:
        print(f"Wrote {file_path}")
        print(
            f"M={cfg['M']} K={cfg['K']} nz_rows={cfg['nz_rows']} "
            f"seed={cfg['seed']}"
        )
        print(f"Non-zero row indices: {nz_row_idx.tolist()}")
        print(f"Checksum: {checksum}")


if __name__ == "__main__":
    main()
