#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
# Author: Ihor Sachok <ihor.sachok@unibo.it>

import numpy as np
import argparse
import pathlib
import hjson

np.random.seed(42)
global verbose

LLR_MAX = 120
LLR_INFTY = 127


def array_to_cstr(a):
    out = "{"
    if isinstance(a, np.ndarray):
        a = a.flat
    for el in a:
        out += "{}, ".format(int(el))
    return out[:-2] + "}"


def rand_llr_pair_generator(size, llr_max=LLR_MAX, inf_prob=0.0, equal_prob=0.0, saturate_prob=0.0):
    a = np.random.randint(-llr_max, llr_max + 1, size=size).astype(np.int16)
    b = np.random.randint(-llr_max, llr_max + 1, size=size).astype(np.int16)
    if saturate_prob > 0:
        mask = np.random.rand(size) < saturate_prob
        n = int(np.sum(mask))
        if n > 0:
            sign = np.random.choice([-1, 1], size=n)
            half = llr_max // 2 + 1
            a[mask] = sign * np.random.randint(half, llr_max + 1, size=n)
            b[mask] = sign * np.random.randint(half, llr_max + 1, size=n)
    if inf_prob > 0:
        mask = np.random.rand(size) < inf_prob
        n = int(np.sum(mask))
        if n > 0:
            sign = np.random.choice([-1, 1], size=n)
            which = np.random.rand(n) < 0.5
            idx = np.flatnonzero(mask)
            a[idx[which]] = sign[which] * LLR_INFTY
            b[idx[~which]] = sign[~which] * LLR_INFTY
    if equal_prob > 0:
        mask = np.random.rand(size) < equal_prob
        b[mask] = a[mask]
    return a.astype(np.int8), b.astype(np.int8)


def var_to_check_golden(a, b):
    a = a.astype(np.int16); b = b.astype(np.int16)
    tmp = a - b
    out = np.where(np.abs(tmp) > LLR_MAX, np.sign(tmp) * LLR_MAX, tmp)
    out = np.where(np.abs(b) == LLR_INFTY, -b, out)
    out = np.where(np.abs(a) == LLR_INFTY,  a, out)
    out = np.where(a == b, 0, out)
    return out.astype(np.int8)


def emit_header_file(**kw):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    s = ("// Copyright 2026 ETH Zurich and University of Bologna.\n"
         "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
         "// SPDX-License-Identifier: Apache-2.0\n\n"
         "// This file was generated automatically.\n\n")
    Z, d, n_nodes = kw["lifting_size"], kw["d"], kw["n_nodes"]
    length = d * Z
    s += f"const uint32_t lifting_size = {Z};\n"
    s += f"const uint32_t num_cores_gen = {kw['num_cores']};\n"
    s += f"const uint32_t nof_edges = {d};\n"
    s += f"const uint32_t n_nodes = {n_nodes};\n"
    s += f"const uint32_t seg_len = {length};\n\n"
    s += (f'int8_t this_soft_bits_dram [{n_nodes*Z}] __attribute__((section(".data"))) = '
          + array_to_cstr(kw["soft_pool"]) + ";\n\n")
    s += (f'uint16_t this_vidx_dram [{length}] __attribute__((section(".data"))) = '
          + array_to_cstr(kw["vidx"]) + ";\n\n")
    s += (f'int8_t this_check_to_var_dram [{length}] __attribute__((section(".data"))) = '
          + array_to_cstr(kw["check_to_var"]) + ";\n\n")
    file = file_path / f"data_{Z}_{kw['num_cores']}.h"
    file.write_text(s)
    return s


def main():
    p = argparse.ArgumentParser(description="Generate data for indexed compute_var_to_check")
    p.add_argument("-c", "--cfg", type=pathlib.Path, required=True)
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()
    global verbose; verbose = args.verbose

    param = hjson.loads(args.cfg.read_text())
    Z = param["lifting_size"]
    num_cores = param.get("num_cores", 1)
    d = param.get("nof_edges", 8)
    n_nodes = param.get("n_nodes", max(2*d, d + 4))
    inf_prob = param.get("inf_prob", 0.0)
    equal_prob = param.get("equal_prob", 0.0)
    saturate_prob = param.get("saturate_prob", 0.0)

    assert n_nodes >= d, "n_nodes must be >= nof_edges"
    length = d * Z

    a_flat, b_flat = rand_llr_pair_generator(length, LLR_MAX, inf_prob, equal_prob, saturate_prob)

    nodes = np.sort(np.random.choice(n_nodes, size=d, replace=False))

    vidx = np.empty(length, dtype=np.uint16)
    for seg in range(d):
        base = int(nodes[seg]) * Z
        vidx[seg*Z:(seg+1)*Z] = base + np.arange(Z, dtype=np.uint16)

    assert vidx.max() < 65536, f"index {vidx.max()} overflows uint16 -> нужен vluxei32"

    soft_pool = np.zeros(n_nodes * Z, dtype=np.int8)
    soft_pool[vidx.astype(np.int64)] = a_flat

    assert np.array_equal(soft_pool[vidx.astype(np.int64)], a_flat), "gather mismatch"

    golden = var_to_check_golden(a_flat, b_flat)

    if verbose:
        print(f"Z={Z} d={d} n_nodes={n_nodes} len={length}")
        print(f"nodes (scattered): {nodes.tolist()}")
        print(f"vidx range: [{vidx.min()}, {vidx.max()}]  (uint16 ok: {vidx.max()<65536})")
        print(f"a==b: {int(np.sum(a_flat==b_flat))}  "
              f"inf: {int(np.sum((np.abs(a_flat.astype(np.int16))==LLR_INFTY)|(np.abs(b_flat.astype(np.int16))==LLR_INFTY)))}  "
              f"sat: {int(np.sum(np.abs(a_flat.astype(np.int16)-b_flat.astype(np.int16))>LLR_MAX))}")
        print(f"golden range: [{golden.min()}, {golden.max()}]")

    emit_header_file(lifting_size=Z, num_cores=num_cores, d=d, n_nodes=n_nodes,
                     soft_pool=soft_pool, vidx=vidx, check_to_var=b_flat)


if __name__ == "__main__":
    main()
