#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import argparse
import pathlib

import hjson
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(description="Generate CSR SpMV benchmark data")
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help="HJSON configuration file",
    )
    parser.add_argument("--M", type=int, help="Number of rows")
    parser.add_argument("--N", type=int, help="Number of columns")
    parser.add_argument("--K", type=int, help="Number of non-zeros")
    parser.add_argument("--prec", type=int, choices=(64,), help="Precision")
    parser.add_argument(
        "--density",
        type=float,
        help="Sparse density in [0, 1]; used when K is not specified",
    )
    parser.add_argument(
        "--min-per-row",
        type=int,
        help="Minimum number of non-zeros per row",
    )
    parser.add_argument(
        "--min-nnz-per-row",
        dest="min_per_row",
        type=int,
        help="Alias of --min-per-row",
    )
    parser.add_argument(
        "--max-per-row",
        type=int,
        help="Maximum number of non-zeros per row",
    )
    parser.add_argument("--seed", type=int, help="Random seed")
    parser.add_argument(
        "--power-of-two",
        dest="power_of_two",
        action="store_true",
        help="Force K/nnz to a power of two",
    )
    parser.add_argument(
        "--no-power-of-two",
        dest="power_of_two",
        action="store_false",
        help="Allow arbitrary K/nnz",
    )
    parser.add_argument(
        "--debug-comment",
        dest="debug_comment",
        action="store_true",
        help="Emit a human-readable dense matrix/vector comment in the header",
    )
    parser.add_argument(
        "--no-debug-comment",
        dest="debug_comment",
        action="store_false",
        help="Do not emit the dense matrix/vector debug comment",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Print generation summary"
    )
    parser.set_defaults(power_of_two=None)
    parser.set_defaults(debug_comment=None)
    return parser.parse_args()


def override(cfg, args):
    out = dict(cfg)
    for key in ("M", "N", "K", "prec", "density", "seed"):
        value = getattr(args, key, None)
        if value is not None:
            out[key] = value
    if args.min_per_row is not None:
        out["min_per_row"] = args.min_per_row
    if args.max_per_row is not None:
        out["max_per_row"] = args.max_per_row
    if args.power_of_two is not None:
        out["power_of_two"] = args.power_of_two
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
    required = ("M", "N", "prec")
    for key in required:
        if key not in cfg:
            raise ValueError(f"Missing required config field '{key}'")

    m = int(cfg["M"])
    n = int(cfg["N"])
    prec = int(cfg["prec"])
    if m <= 0 or n <= 0:
        raise ValueError("M and N must be positive")
    if prec != 64:
        raise ValueError("prec must be 64")

    if "K" in cfg:
        k = int(cfg["K"])
    else:
        density = float(cfg.get("density", 0.1))
        if density < 0.0 or density > 1.0:
            raise ValueError("density must be in [0, 1]")
        k = int(round(m * n * density))
        k = max(1, k)

    if k < 0 or k > m * n:
        raise ValueError("K must be in [0, M*N]")

    min_per_row = int(cfg.get("min_per_row", 8))
    max_per_row = int(cfg.get("max_per_row", n))
    if min_per_row < 0:
        raise ValueError("min_per_row must be >= 0")
    if max_per_row <= 0 or max_per_row > n:
        raise ValueError("max_per_row must be in [1, N]")
    if min_per_row > max_per_row:
        raise ValueError("min_per_row must be <= max_per_row")
    power_of_two = bool(cfg.get("power_of_two", True))
    if power_of_two and min_per_row < 1:
        min_per_row = 1

    if m * min_per_row > k and not power_of_two:
        raise ValueError("K is too small for the requested min_per_row")
    if m * max_per_row < k and not power_of_two:
        raise ValueError("K is too large for the requested max_per_row")

    requested_k = k

    seed = int(cfg.get("seed", 42))

    return {
        "M": m,
        "N": n,
        "K": k,
        "prec": prec,
        "seed": seed,
        "requested_K": requested_k,
        "min_per_row": min_per_row,
        "max_per_row": max_per_row,
        "power_of_two": power_of_two,
        "debug_comment": bool(cfg.get("debug_comment", False)),
    }


def allocate_row_counts(m, k, min_per_row, max_per_row, rng):
    row_counts = np.full(m, min_per_row, dtype=np.int64)
    remaining = k - int(row_counts.sum())

    # Keep a reasonable number of non-empty rows by seeding one extra element
    # into randomly selected rows when possible.
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


def allocate_row_counts_power_of_two(m, target_k, min_per_row, max_per_row, rng):
    del rng
    choices = power_of_two_choices(min_per_row, max_per_row)
    if not choices:
        raise ValueError("No per-row power-of-two nnz is valid in the requested range")

    max_sum = m * max(choices)
    states = {0: None}
    prev = []

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

    best_total = min(states.keys(), key=lambda total: (abs(total - target_k), total))

    counts = np.empty(m, dtype=np.int64)
    current = best_total
    for row in range(m - 1, -1, -1):
        previous, count = prev[row][current]
        counts[row] = count
        current = previous

    return counts


def generate_csr(cfg):
    rng = np.random.default_rng(cfg["seed"])
    m = cfg["M"]
    n = cfg["N"]
    target_k = cfg["K"]

    if cfg["power_of_two"]:
        row_counts = allocate_row_counts_power_of_two(
            m, target_k, cfg["min_per_row"], cfg["max_per_row"], rng
        )
    else:
        row_counts = allocate_row_counts(
            m, target_k, cfg["min_per_row"], cfg["max_per_row"], rng
        )

    k = int(np.sum(row_counts))

    row_ptr = np.zeros(m + 1, dtype=np.uint32)
    row_ptr[1:] = np.cumsum(row_counts, dtype=np.uint32)

    col_idx = np.empty(k, dtype=np.uint32)
    offset = 0
    for row, row_nnz in enumerate(row_counts):
        if row_nnz == 0:
            continue
        cols = np.sort(rng.choice(n, size=int(row_nnz), replace=False)).astype(
            np.uint32
        )
        col_idx[offset : offset + row_nnz] = cols
        offset += int(row_nnz)

    val = rng.standard_normal(k).astype(np.float64)
    x = rng.standard_normal(n).astype(np.float64)

    y = np.zeros(m, dtype=val.dtype)
    for row in range(m):
        start = int(row_ptr[row])
        end = int(row_ptr[row + 1])
        if start == end:
            continue
        y[row] = np.sum(val[start:end] * x[col_idx[start:end]], dtype=val.dtype)

    checksum = float(np.sum(y.astype(np.float64)))

    dense = np.zeros((m, n), dtype=np.float64)
    for row in range(m):
        start = int(row_ptr[row])
        end = int(row_ptr[row + 1])
        if start == end:
            continue
        dense[row, col_idx[start:end]] = val[start:end].astype(np.float64)

    return row_ptr, col_idx, val, x, y, checksum, dense, k


def scalar_to_c_str(value, prec):
    del prec
    return repr(float(value))


def array_to_c_str(array, prec=None):
    elems = []
    if prec is None:
        for value in np.asarray(array).reshape(-1):
            elems.append(str(int(value)))
    else:
        for value in np.asarray(array).reshape(-1):
            elems.append(scalar_to_c_str(value, prec))
    return "{\n\t" + ",\n\t".join(elems) + "\n}"


def format_debug_scalar(value):
    return f"{float(value): .6f}"


def emit_debug_comment(dense, x, y):
    lines = ["// Debug view of generated dense inputs/results.", "// Dense A:"]
    for row in dense:
        lines.append("//   [" + ", ".join(format_debug_scalar(v) for v in row) + "]")
    lines.append("// x:")
    lines.append("//   [" + ", ".join(format_debug_scalar(v) for v in x) + "]")
    lines.append("// y = A*x:")
    lines.append("//   [" + ", ".join(format_debug_scalar(v) for v in y) + "]")
    return "\n".join(lines) + "\n\n"


def emit_header(cfg, row_ptr, col_idx, val, x, y, checksum, dense, actual_k):
    data_dir = pathlib.Path(__file__).parent.parent / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    file_path = data_dir / f"data_{cfg['M']}_{cfg['N']}_{actual_k}_{cfg['prec']}.h"

    ctype = "double"
    header = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    if cfg["debug_comment"]:
        header += emit_debug_comment(dense, x.astype(np.float64), y.astype(np.float64))
    header += (
        '#include "layer.h"\n\n'
        f"const spmv_layer spmv_l = {{.M = {cfg['M']}, .N = {cfg['N']}, "
        f".K = {actual_k}, .dtype = FP{cfg['prec']}}};\n\n"
        f'static uint32_t spmv_row_ptr_dram[{cfg["M"] + 1}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(row_ptr)};\n"
        f'static uint32_t spmv_col_idx_dram[{actual_k}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(col_idx)};\n"
        f'static {ctype} spmv_val_dram[{actual_k}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(val, cfg['prec'])};\n"
        f'static {ctype} spmv_x_dram[{cfg["N"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(x, cfg['prec'])};\n"
        f'static {ctype} spmv_result[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(y, cfg['prec'])};\n"
        f"static double spmv_checksum __attribute__((aligned(8))) = {repr(checksum)};\n"
    )

    file_path.write_text(header)
    return file_path


def main():
    args = parse_args()
    with args.cfg.open() as f:
        cfg = override(hjson.loads(f.read()), args)

    cfg = validate(cfg)
    row_ptr, col_idx, val, x, y, checksum, dense, actual_k = generate_csr(cfg)
    file_path = emit_header(cfg, row_ptr, col_idx, val, x, y, checksum, dense, actual_k)

    if args.verbose:
        row_nnz = np.diff(row_ptr)
        print(f"Wrote {file_path}")
        print(
            f"M={cfg['M']} N={cfg['N']} K={actual_k} prec={cfg['prec']} "
            f"seed={cfg['seed']}"
        )
        if actual_k != cfg["requested_K"]:
            print(f"requested K={cfg['requested_K']} adjusted to K={actual_k}")
        print(
            f"row nnz: min={int(row_nnz.min())} max={int(row_nnz.max())} "
            f"avg={float(row_nnz.mean()):.2f}"
        )


if __name__ == "__main__":
    main()
