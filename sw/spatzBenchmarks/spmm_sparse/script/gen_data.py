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
        description="Generate dense-backed sparse SpMM benchmark data"
    )
    parser.add_argument(
        "-c", "--cfg", type=pathlib.Path, required=True, help="HJSON config file"
    )
    parser.add_argument("--M", type=int, help="Number of rows in sparse A")
    parser.add_argument("--N", type=int, help="Number of columns in dense B / C")
    parser.add_argument("--K", type=int, help="Inner dimension")
    parser.add_argument("--nnz", type=int, help="Requested number of structural non-zeros")
    parser.add_argument("--prec", type=int, choices=(64,), help="Precision")
    parser.add_argument(
        "--density",
        type=float,
        help="Sparse density in [0, 1]; used when nnz is not specified",
    )
    parser.add_argument("--min-per-row", dest="min_per_row", type=int)
    parser.add_argument("--min-nnz-per-row", dest="min_per_row", type=int)
    parser.add_argument("--max-per-row", type=int)
    parser.add_argument("--seed", type=int, help="Random seed")
    parser.add_argument(
        "--power-of-two",
        dest="power_of_two",
        action="store_true",
        help="Force each row nnz count to be a power of two",
    )
    parser.add_argument(
        "--no-power-of-two",
        dest="power_of_two",
        action="store_false",
        help="Allow arbitrary per-row nnz counts",
    )
    parser.add_argument(
        "--sorted-cols",
        dest="sorted_cols",
        action="store_true",
        help="Sort columns within each row",
    )
    parser.add_argument(
        "--unsorted-cols",
        dest="sorted_cols",
        action="store_false",
        help="Keep random column order to worsen locality",
    )
    parser.add_argument(
        "--debug-comment",
        dest="debug_comment",
        action="store_true",
        help="Emit a human-readable A/B/C comment in the header",
    )
    parser.add_argument(
        "--no-debug-comment",
        dest="debug_comment",
        action="store_false",
        help="Do not emit the human-readable debug comment",
    )
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.set_defaults(power_of_two=None)
    parser.set_defaults(sorted_cols=None)
    parser.set_defaults(debug_comment=None)
    return parser.parse_args()


def override(cfg, args):
    out = dict(cfg)
    for key in ("M", "N", "K", "nnz", "prec", "density", "seed"):
        value = getattr(args, key, None)
        if value is not None:
            out[key] = value
    if args.min_per_row is not None:
        out["min_per_row"] = args.min_per_row
    if args.max_per_row is not None:
        out["max_per_row"] = args.max_per_row
    if args.power_of_two is not None:
        out["power_of_two"] = args.power_of_two
    if args.sorted_cols is not None:
        out["sorted_cols"] = args.sorted_cols
    if args.debug_comment is not None:
        out["debug_comment"] = args.debug_comment
    return out


def power_of_two_choices(min_count, max_count):
    counts = []
    value = 1
    while value <= max_count:
        if value >= min_count:
            counts.append(value)
        value <<= 1
    return counts


def validate(cfg):
    required = ("M", "N", "K", "prec")
    for key in required:
        if key not in cfg:
            raise ValueError(f"Missing required config field '{key}'")

    m = int(cfg["M"])
    n = int(cfg["N"])
    k = int(cfg["K"])
    prec = int(cfg["prec"])
    if m <= 0 or n <= 0 or k <= 0:
        raise ValueError("M, N, and K must be positive")
    if prec != 64:
        raise ValueError("prec must be 64")

    if "nnz" in cfg:
        nnz = int(cfg["nnz"])
    else:
        density = float(cfg.get("density", 0.125))
        if density < 0.0 or density > 1.0:
            raise ValueError("density must be in [0, 1]")
        nnz = int(round(m * k * density))
        nnz = max(1, nnz)

    if nnz < 0 or nnz > m * k:
        raise ValueError("nnz must be in [0, M*K]")

    min_per_row = int(cfg.get("min_per_row", 8))
    max_per_row = int(cfg.get("max_per_row", min(k, 64)))
    if min_per_row < 0:
        raise ValueError("min_per_row must be >= 0")
    if max_per_row <= 0 or max_per_row > k:
        raise ValueError("max_per_row must be in [1, K]")
    if min_per_row > max_per_row:
        raise ValueError("min_per_row must be <= max_per_row")

    power_of_two = bool(cfg.get("power_of_two", True))
    if power_of_two and min_per_row < 1:
        min_per_row = 1

    if m * min_per_row > nnz and not power_of_two:
        raise ValueError("nnz is too small for the requested min_per_row")
    if m * max_per_row < nnz and not power_of_two:
        raise ValueError("nnz is too large for the requested max_per_row")

    return {
        "M": m,
        "N": n,
        "K": k,
        "nnz": nnz,
        "requested_nnz": nnz,
        "prec": prec,
        "seed": int(cfg.get("seed", 42)),
        "min_per_row": min_per_row,
        "max_per_row": max_per_row,
        "power_of_two": power_of_two,
        "sorted_cols": bool(cfg.get("sorted_cols", False)),
        "debug_comment": bool(cfg.get("debug_comment", False)),
    }


def allocate_row_counts(m, nnz, min_per_row, max_per_row, rng):
    row_counts = np.full(m, min_per_row, dtype=np.int64)
    remaining = nnz - int(row_counts.sum())

    if remaining > 0 and min_per_row == 0:
        active_rows = min(m, remaining)
        chosen = rng.choice(m, size=active_rows, replace=False)
        row_counts[chosen] += 1
        remaining -= active_rows

    while remaining > 0:
        candidates = np.flatnonzero(row_counts < max_per_row)
        if len(candidates) == 0:
            raise ValueError("Unable to distribute non-zeros across rows")
        row = int(rng.choice(candidates))
        row_counts[row] += 1
        remaining -= 1

    return row_counts


def allocate_row_counts_power_of_two(m, target_nnz, min_per_row, max_per_row):
    choices = power_of_two_choices(min_per_row, max_per_row)
    if not choices:
        raise ValueError("No valid power-of-two row nnz count in range")

    states = {0: None}
    prev = []
    max_sum = m * max(choices)

    for _ in range(m):
        next_states = {}
        for partial in states:
            for count in choices:
                total = partial + count
                if total > max_sum or total in next_states:
                    continue
                next_states[total] = (partial, count)
        if not next_states:
            raise ValueError("Unable to build row counts with power-of-two nnz")
        prev.append(next_states)
        states = next_states

    best_total = min(states.keys(), key=lambda total: (abs(total - target_nnz), total))

    counts = np.empty(m, dtype=np.int64)
    current = best_total
    for row in range(m - 1, -1, -1):
        previous, count = prev[row][current]
        counts[row] = count
        current = previous

    return counts


def generate_sparse_dense_mm(cfg):
    rng = np.random.default_rng(cfg["seed"])

    if cfg["power_of_two"]:
        row_counts = allocate_row_counts_power_of_two(
            cfg["M"], cfg["nnz"], cfg["min_per_row"], cfg["max_per_row"]
        )
    else:
        row_counts = allocate_row_counts(
            cfg["M"], cfg["nnz"], cfg["min_per_row"], cfg["max_per_row"], rng
        )

    actual_nnz = int(row_counts.sum())
    row_ptr = np.empty(cfg["M"] + 1, dtype=np.uint32)
    row_ptr[0] = 0
    np.cumsum(row_counts, out=row_ptr[1:])

    col_idx = np.empty(actual_nnz, dtype=np.uint32)
    dense_a = np.zeros((cfg["M"], cfg["K"]), dtype=np.float64)

    offset = 0
    for row, count in enumerate(row_counts):
        count = int(count)
        cols = rng.choice(cfg["K"], size=count, replace=False)
        if cfg["sorted_cols"]:
            cols.sort()
        vals = rng.standard_normal(count)
        col_idx[offset : offset + count] = cols.astype(np.uint32)
        dense_a[row, cols] = vals
        offset += count

    b = rng.standard_normal((cfg["K"], cfg["N"]))
    c = dense_a @ b
    checksum = float(np.sum(c))

    return {
        "M": cfg["M"],
        "N": cfg["N"],
        "K": cfg["K"],
        "nnz": actual_nnz,
        "requested_nnz": cfg["requested_nnz"],
        "row_counts": row_counts,
        "row_ptr": row_ptr,
        "col_idx": col_idx,
        "dense_a": dense_a,
        "b": b,
        "c": c,
        "checksum": checksum,
        "prec": cfg["prec"],
        "seed": cfg["seed"],
        "debug_comment": cfg["debug_comment"],
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


def format_comment_matrix(lines, name, mat):
    lines.append(f"// {name}:")
    for row in mat:
        row_str = ", ".join(f"{float(v): .6f}" for v in row)
        lines.append(f"//   [{row_str}]")


def emit_header(data):
    lines = []
    lines.append("// Copyright 2025 ETH Zurich and University of Bologna.")
    lines.append("// Licensed under the Apache License, Version 2.0, see LICENSE for details.")
    lines.append("// SPDX-License-Identifier: Apache-2.0")
    lines.append("")
    lines.append("// This file was generated automatically.")
    lines.append("")

    if data["debug_comment"]:
        lines.append("// Debug view of generated dense-backed sparse SpMM inputs/results.")
        lines.append(
            f"// Requested nnz: {data['requested_nnz']}, actual nnz: {data['nnz']}, seed: {data['seed']}"
        )
        format_comment_matrix(lines, "Dense sparse-backed A", data["dense_a"])
        format_comment_matrix(lines, "Dense B", data["b"])
        format_comment_matrix(lines, "Dense C = A*B", data["c"])
        lines.append("")

    lines.append('#include "layer.h"')
    lines.append("")
    lines.append(
        "const spmm_sparse_layer spmm_sparse_l = {"
        f".M = {data['M']}, .N = {data['N']}, .K = {data['K']}, .nnz = {data['nnz']}, .dtype = FP64}};"
    )
    lines.append("")
    lines.append(
        format_c_array((int(v) for v in data["row_ptr"]), "uint32_t", "spmm_sparse_row_ptr_dram")
    )
    lines.append(
        format_c_array((int(v) for v in data["col_idx"]), "uint32_t", "spmm_sparse_col_idx_dram")
    )
    lines.append(
        format_float_array(data["dense_a"].reshape(-1), "spmm_sparse_dense_a_dram")
    )
    lines.append(format_float_array(data["b"].reshape(-1), "spmm_sparse_b_dram"))
    lines.append(format_float_array(data["c"].reshape(-1), "spmm_sparse_result"))
    lines.append(f"static double spmm_sparse_checksum = {data['checksum']:.17g};")
    lines.append("")
    return "\n".join(lines)


def output_path(m, n, k, prec):
    base = pathlib.Path(__file__).resolve().parent.parent
    return base / "data" / f"data_{m}_{n}_{k}_{prec}.h"


def main():
    args = parse_args()
    with args.cfg.open("r", encoding="utf-8") as handle:
        cfg = hjson.load(handle)

    cfg = override(cfg, args)
    cfg = validate(cfg)
    data = generate_sparse_dense_mm(cfg)
    out = output_path(data["M"], data["N"], data["K"], data["prec"])
    out.write_text(emit_header(data), encoding="utf-8")

    if args.verbose:
        counts = ", ".join(str(int(v)) for v in data["row_counts"])
        print(f"Wrote {out}")
        print(f"Requested nnz={data['requested_nnz']}, actual nnz={data['nnz']}")
        print(f"Row nnz counts: {counts}")


if __name__ == "__main__":
    main()
