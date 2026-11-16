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


def emit_header_file(layer_type: str, **kwargs):

    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    emit_str = (
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    file = file_path / (
        "data_" + str(kwargs["lifting_size"]) + "_" + str(kwargs["num_cores"]) + ".h"
    )
    emit_str += emit_hard_decision_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_hard_decision_layer(name="hard_decision", **kwargs):
    soft_bits = kwargs["soft_bits"]
    lifting_size = kwargs["lifting_size"]
    num_cores = kwargs["num_cores"]
    total_size = lifting_size * num_cores

    layer_str = ""
    layer_str += f"const uint32_t lifting_size = {lifting_size};\n"
    layer_str += f"const uint32_t num_cores_gen = {num_cores};\n\n"
    layer_str += (
        f'int8_t {name}_soft_bits_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(soft_bits)
        + ";\n\n\n"
    )

    return layer_str


def rand_llr_generator(size, llr_max=120, zero_prob=0.0):
    """Generates random int8 LLR values in [-llr_max, llr_max].
    zero_prob: probability of forcing an element to exactly 0 (to test the "found zero" path).
    """
    data = np.random.randint(-llr_max, llr_max + 1, size=size).astype(np.int8)
    if zero_prob > 0:
        mask = np.random.rand(size) < zero_prob
        data[mask] = 0
    elif zero_prob == 0:
        zero_mask = data == 0
        while np.any(zero_mask):
            data[zero_mask] = np.random.randint(
                -llr_max, llr_max + 1, size=np.sum(zero_mask)
            ).astype(np.int8)
            zero_mask = data == 0
    return data


def main():

    parser = argparse.ArgumentParser(description="Generate data for kernels")
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help="Select param config file kernel",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")

    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    lifting_size = param["lifting_size"]
    num_cores = param.get("num_cores", 1)
    zero_prob = param.get("zero_prob", 0.0)

    # Generate independent data for every core: total length = num_cores * lifting_size.
    soft_bits = rand_llr_generator(
        lifting_size * num_cores, llr_max=120, zero_prob=zero_prob
    )

    kwargs = {
        "soft_bits": soft_bits,
        "lifting_size": lifting_size,
        "num_cores": num_cores,
    }

    emit_header_file("hard_decision", **kwargs)


if __name__ == "__main__":
    main()