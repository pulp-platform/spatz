#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Generates a synthetic non-bonded Lennard-Jones MD benchmark: M particles
# at random positions in a box, each with a CSR-style neighbor list of
# random (not necessarily spatially close) other particles. Non-Newton: no
# reciprocal force bookkeeping needed, golden forces are just each
# particle's independent sum over its own neighbor list.

import argparse
import pathlib

import hjson
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(description="Generate MD-LJ benchmark data")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True)
    parser.add_argument("--M", type=int, help="Number of particles")
    parser.add_argument("--K", type=int, help="Total neighbor-list entries")
    parser.add_argument("--prec", type=int, choices=(64,))
    parser.add_argument("--min-per-row", type=int, dest="min_per_row")
    parser.add_argument("--max-per-row", type=int, dest="max_per_row")
    parser.add_argument("--box", type=float, help="Cubic box side length")
    parser.add_argument("--seed", type=int)
    parser.add_argument("-v", "--verbose", action="store_true")
    return parser.parse_args()


def override(cfg, args):
    out = dict(cfg)
    for key in ("M", "K", "prec", "seed", "box"):
        value = getattr(args, key, None)
        if value is not None:
            out[key] = value
    if args.min_per_row is not None:
        out["min_per_row"] = args.min_per_row
    if args.max_per_row is not None:
        out["max_per_row"] = args.max_per_row
    return out


def validate(cfg):
    m = int(cfg["M"])
    prec = int(cfg.get("prec", 64))
    if m <= 0:
        raise ValueError("M must be positive")
    if prec != 64:
        raise ValueError("prec must be 64")

    min_per_row = int(cfg.get("min_per_row", 16))
    max_per_row = int(cfg.get("max_per_row", min_per_row))
    if min_per_row < 0 or max_per_row < min_per_row:
        raise ValueError("invalid min/max_per_row")
    if max_per_row > m - 1:
        raise ValueError("max_per_row must be <= M-1 (no self-neighbors)")

    return {
        "M": m,
        "prec": prec,
        "seed": int(cfg.get("seed", 42)),
        "box": float(cfg.get("box", 20.0)),
        "min_per_row": min_per_row,
        "max_per_row": max_per_row,
    }


def generate(cfg):
    rng = np.random.default_rng(cfg["seed"])
    m = cfg["M"]

    pos = rng.uniform(0.0, cfg["box"], size=(m, 3))

    row_counts = rng.integers(cfg["min_per_row"], cfg["max_per_row"] + 1,
                              size=m, endpoint=False) \
        if cfg["max_per_row"] > cfg["min_per_row"] \
        else np.full(m, cfg["min_per_row"], dtype=np.int64)

    neigh_ptr = np.zeros(m + 1, dtype=np.uint32)
    neigh_ptr[1:] = np.cumsum(row_counts, dtype=np.uint32)
    k = int(neigh_ptr[-1])

    neigh_idx = np.empty(k, dtype=np.uint32)
    offset = 0
    for i in range(m):
        nnz = int(row_counts[i])
        if nnz == 0:
            continue
        choices = rng.choice(m - 1, size=nnz, replace=False)
        choices = np.where(choices >= i, choices + 1, choices)
        neigh_idx[offset:offset + nnz] = choices
        offset += nnz

    # Division-free, MD-flavored pairwise force: f = K*(RCUT2-r2)^2,
    # applied unconditionally to every listed neighbor (no cutoff mask --
    # this hardware has no float div/sqrt, see kernel/md.h).
    rcut2 = 200.0
    force_k = 0.0001
    force = np.zeros((m, 3), dtype=np.float64)
    for i in range(m):
        start, end = int(neigh_ptr[i]), int(neigh_ptr[i + 1])
        if start == end:
            continue
        js = neigh_idx[start:end]
        d = pos[js] - pos[i]
        r2 = np.sum(d * d, axis=1)
        diff = rcut2 - r2
        fpair = force_k * diff * diff
        force[i] = np.sum(fpair[:, None] * d, axis=0)

    checksum = float(np.sum(force))
    return pos, neigh_ptr, neigh_idx, force, checksum, k


def array_to_c_str(array, is_float):
    elems = []
    for value in np.asarray(array).reshape(-1):
        elems.append(repr(float(value)) if is_float else str(int(value)))
    return "{\n\t" + ",\n\t".join(elems) + "\n}"


def emit_header(cfg, pos, neigh_ptr, neigh_idx, force, checksum, actual_k):
    data_dir = pathlib.Path(__file__).parent.parent / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    file_path = data_dir / f"data_{cfg['M']}_{actual_k}_{cfg['prec']}.h"

    header = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
        '#include "layer.h"\n\n'
        f"const md_layer md_l = {{.M = {cfg['M']}, .K = {actual_k}, "
        f".dtype = FP{cfg['prec']}}};\n\n"
        f'static uint32_t md_neigh_ptr_dram[{cfg["M"] + 1}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(neigh_ptr, False)};\n"
        f'static uint32_t md_neigh_idx_dram[{actual_k}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(neigh_idx, False)};\n"
        f'static double md_pos_x_dram[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(pos[:, 0], True)};\n"
        f'static double md_pos_y_dram[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(pos[:, 1], True)};\n"
        f'static double md_pos_z_dram[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(pos[:, 2], True)};\n"
        f'static double md_force_x[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(force[:, 0], True)};\n"
        f'static double md_force_y[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(force[:, 1], True)};\n"
        f'static double md_force_z[{cfg["M"]}] __attribute__((section(".data"), aligned(8))) = '
        f"{array_to_c_str(force[:, 2], True)};\n"
        f"static double md_checksum __attribute__((aligned(8))) = {repr(checksum)};\n"
    )
    file_path.write_text(header)
    return file_path


def main():
    args = parse_args()
    with args.cfg.open() as f:
        cfg = override(hjson.loads(f.read()), args)
    cfg = validate(cfg)
    pos, neigh_ptr, neigh_idx, force, checksum, actual_k = generate(cfg)
    file_path = emit_header(cfg, pos, neigh_ptr, neigh_idx, force, checksum,
                            actual_k)

    if args.verbose:
        row_nnz = np.diff(neigh_ptr)
        print(f"Wrote {file_path}")
        print(f"M={cfg['M']} K={actual_k} prec={cfg['prec']} seed={cfg['seed']}")
        print(f"row nnz: min={int(row_nnz.min())} max={int(row_nnz.max())} "
              f"avg={float(row_nnz.mean()):.2f}")
        print(f"checksum={checksum}")


if __name__ == "__main__":
    main()
