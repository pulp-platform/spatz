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
        description="Generate row-sparse SpMM benchmark data"
    )
    parser.add_argument(
        "-c", "--cfg", type=pathlib.Path, required=True, help="HJSON config file"
    )
    parser.add_argument("--M", type=int, help="Total number of rows in A")
    parser.add_argument("--K", type=int, help="Inner dimension (columns of A / rows of B)")
    parser.add_argument("--N", type=int, help="Number of columns in B / C")
    parser.add_argument("--nz-rows", dest="nz_rows", type=int,
                        help="Number of non-zero rows in A")
    parser.add_argument("--seed", type=int, help="Random seed")
    parser.add_argument("-v", "--verbose", action="store_true")
    return parser.parse_args()


def override(cfg, args):
    out = dict(cfg)
    for key in ("M", "K", "N", "seed"):
        value = getattr(args, key, None)
        if value is not None:
            out[key] = value
    if args.nz_rows is not None:
        out["nz_rows"] = args.nz_rows
    return out


def validate(cfg):
    required = ("M", "K", "N", "nz_rows")
    for key in required:
        if key not in cfg:
            raise ValueError(f"Missing required config field '{key}'")

    m = int(cfg["M"])
    k = int(cfg["K"])
    n = int(cfg["N"])
    nz_rows = int(cfg["nz_rows"])
    seed = int(cfg.get("seed", 42))

    if m <= 0 or k <= 0 or n <= 0:
        raise ValueError("M, K, and N must be positive")
    if nz_rows < 0 or nz_rows > m:
        raise ValueError("nz_rows must be in [0, M]")

    return {
        "M": m,
        "K": k,
        "N": n,
        "nz_rows": nz_rows,
        "seed": seed,
    }


def generate_data(cfg):
    rng = np.random.default_rng(cfg["seed"])
    m = cfg["M"]
    k = cfg["K"]
    n = cfg["N"]
    nz_rows = cfg["nz_rows"]

    # Pick nz_rows random sorted row indices from [0, M).
    nz_row_idx = np.sort(rng.choice(m, size=nz_rows, replace=False)).astype(
        np.uint32
    )

    # Generate dense_a: only non-zero rows have values.
    dense_a = np.zeros((m, k), dtype=np.float64)
    for row in nz_row_idx:
        dense_a[row, :] = rng.standard_normal(k)

    # Generate B.
    b = rng.standard_normal((k, n)).astype(np.float64)

    # Compute C = A * B.
    c = dense_a @ b
    checksum = float(np.sum(c))

    return {
        "M": m,
        "K": k,
        "N": n,
        "nz_rows": nz_rows,
        "nz_row_idx": nz_row_idx,
        "dense_a": dense_a,
        "b": b,
        "c": c,
        "checksum": checksum,
        "seed": cfg["seed"],
    }


def format_c_array(values, ctype, name):
    values = list(values)
    body = ",\n\t".join(str(v) for v in values)
    return (
        f"static {ctype} {name}[{len(values)}] "
        f"__attribute__((section(\".data\"), aligned(8))) = {{\n\t{body}\n}};\n"
    )


def format_float_array(values, name):
    return format_c_array((f"{float(v):.17g}" for v in values), "double", name)


def emit_header(data):
    lines = []
    lines.append("// Copyright 2025 ETH Zurich and University of Bologna.")
    lines.append(
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details."
    )
    lines.append("// SPDX-License-Identifier: Apache-2.0")
    lines.append("")
    lines.append("// This file was generated automatically.")
    lines.append("")
    lines.append('#include "layer.h"')
    lines.append("")
    lines.append(
        f"const rs_spmm_layer rs_spmm_l = "
        f"{{.M = {data['M']}, .K = {data['K']}, .N = {data['N']}, "
        f".nz_rows = {data['nz_rows']}, .dtype = FP64}};"
    )
    lines.append("")
    lines.append(
        format_c_array(
            (int(v) for v in data["nz_row_idx"]),
            "uint32_t",
            "rs_spmm_nz_row_idx_dram",
        )
    )
    lines.append(
        format_float_array(data["dense_a"].reshape(-1), "rs_spmm_dense_a_dram")
    )
    lines.append(format_float_array(data["b"].reshape(-1), "rs_spmm_b_dram"))
    lines.append(format_float_array(data["c"].reshape(-1), "rs_spmm_result"))
    lines.append(
        f"static double rs_spmm_checksum __attribute__((aligned(8))) = "
        f"{data['checksum']:.17g};"
    )
    lines.append("")
    return "\n".join(lines)


def output_path(m, k, n, nz_rows):
    base = pathlib.Path(__file__).resolve().parent.parent
    return base / "data" / f"data_{m}_{k}_{n}_{nz_rows}_64.h"


def main():
    args = parse_args()
    with args.cfg.open("r", encoding="utf-8") as handle:
        cfg = hjson.load(handle)

    cfg = override(cfg, args)
    cfg = validate(cfg)
    data = generate_data(cfg)
    out = output_path(data["M"], data["K"], data["N"], data["nz_rows"])
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(emit_header(data), encoding="utf-8")

    if args.verbose:
        print(f"Wrote {out}")
        print(
            f"M={data['M']} K={data['K']} N={data['N']} "
            f"nz_rows={data['nz_rows']} seed={data['seed']}"
        )
        print(f"Non-zero row indices: {list(data['nz_row_idx'])}")
        print(f"Checksum: {data['checksum']:.17g}")


if __name__ == "__main__":
    main()
