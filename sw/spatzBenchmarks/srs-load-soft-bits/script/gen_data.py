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


def array_to_cstr(a, fmt=int):
    out = "{"
    if isinstance(a, np.ndarray):
        a = a.flat
    for el in a:
        out += "{}, ".format(int(el))
    out = out[:-2] + "}"
    return out


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    emit_str = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )
    file = file_path / (
        "data_" + str(kwargs["lifting_size"]) + "_" + str(kwargs["num_cores"]) + ".h"
    )
    emit_str += emit_load_soft_bits_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_load_soft_bits_layer(name="load_soft_bits", **kwargs):
    llrs         = kwargs["llrs"]
    total_llr    = kwargs["total_llr"]
    lifting_size = kwargs["lifting_size"]
    buffer_nodes = kwargs["buffer_nodes"]
    num_cores    = kwargs["num_cores"]

    layer_str = ""
    layer_str += f"const uint32_t lifting_size = {lifting_size};\n"
    layer_str += f"const uint32_t buffer_nodes = {buffer_nodes};\n"
    layer_str += f"const uint32_t total_llr    = {total_llr};\n"
    layer_str += f"const uint32_t num_cores_gen = {num_cores};\n\n"
    layer_str += (
        f'int8_t {name}_llrs_dram [{total_llr}] __attribute__((section(".data"))) = '
        + array_to_cstr(llrs)
        + ";\n\n\n"
    )
    return layer_str


def gen_llrs(total_llr, body, nof_valid, clamp_edge_frac=0.15, llr_max=120):
    data = np.zeros(total_llr, dtype=np.int8)

    valid = np.random.randint(-llr_max, llr_max + 1, size=nof_valid).astype(np.int8)

    n_edge = max(1, int(nof_valid * clamp_edge_frac))
    idx = np.random.choice(nof_valid, size=n_edge, replace=False)
    half = n_edge // 2
    valid[idx[:half]] = np.random.randint(65, llr_max + 1, size=half).astype(np.int8)
    valid[idx[half:]] = np.random.randint(-llr_max, -64, size=n_edge - half).astype(np.int8)

    for pos, val in [(0, 64), (1, -64), (2, 65), (3, -65)]:
        if pos < nof_valid:
            valid[pos] = val

    if valid[nof_valid - 1] == 0:
        valid[nof_valid - 1] = 42

    data[:nof_valid] = valid
    return data


def main():
    parser = argparse.ArgumentParser(description="Generate data for load_soft_bits kernel")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True,
                        help="Select param config file kernel")
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    lifting_size = param["lifting_size"]
    num_cores    = param.get("num_cores", 1)
    buffer_nodes = param["buffer_nodes"]

    body = (buffer_nodes - 2) * lifting_size

    valid_frac = param.get("valid_frac", 0.75)
    nof_valid  = int(body * valid_frac)

    tail_frac = param.get("tail_frac", 0.1)
    total_llr = nof_valid + int(body * tail_frac)

    llrs = gen_llrs(total_llr, body, nof_valid, llr_max=120)

    if verbose:
        print(f"lifting_size={lifting_size} buffer_nodes={buffer_nodes} "
              f"body={body} nof_valid={nof_valid} total_llr={total_llr}")

    kwargs = {
        "llrs": llrs,
        "total_llr": total_llr,
        "lifting_size": lifting_size,
        "buffer_nodes": buffer_nodes,
        "num_cores": num_cores,
    }
    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()