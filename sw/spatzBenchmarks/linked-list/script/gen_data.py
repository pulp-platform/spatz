#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Diyou Shen <dishen@iis.ee.ethz.ch>

import numpy as np
import random
import argparse
import pathlib
import hjson

np.random.seed(42)
random.seed(42)

def emit_struct(**kwargs):
    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}
    dtype = ctypes[str(kwargs["prec"])]

    layer_str = (
        "typedef struct payload_t {\n"
        "  int id;\n"
        f"  {dtype} data;\n"
        "} payload_t;\n\n"
        "typedef struct node_t {\n"
        "  payload_t payload;\n"
        "  struct node_t *next;\n"
        "} node_t;\n\n"
    )
    return layer_str


def generate_node_array(**kwargs):
    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}
    dtype = ctypes[str(kwargs["prec"])]
    M = kwargs["M"]

    # Generate random IDs and data
    ids = list(range(M))
    random.shuffle(ids)

    data_values = []
    for _ in range(M):
        if kwargs["prec"] == 8:
            data_values.append(random.randint(-128, 127))  # char range
        elif kwargs["prec"] == 16:
            data_values.append(round(random.uniform(-100, 100), 3))  # Simulate __fp16
        else:
            data_values.append(round(random.uniform(-100.0, 100.0), 6))  # float/double

    layer_str = '#include "layer.h"\n\n'

    layer_str += f'int N = {M};\n\n'

    # Create the array of nodes
    layer_str += f'node_t nodes[{M}] __attribute__((section(".data"))) = {{\n'
    for i in range(M):
        layer_str += f"    {{.payload = {{.id = {ids[i]}, .data = ({dtype}){data_values[i]}}}, .next = NULL}},\n"
    layer_str += "};\n\n"
    return layer_str


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    header_content = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    # header_content += emit_struct(**kwargs)
    header_content += generate_node_array(**kwargs)

    file = file_path / f"data_{kwargs['M']}.h"
    with file.open("w") as f:
        f.write(header_content)

    print(f"Header file generated: {file}")


def main():
    parser = argparse.ArgumentParser(description="Generate data for kernels")
    parser.add_argument(
        "-c", "--cfg", type=pathlib.Path, required=True, help="Configuration file"
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    args = parser.parse_args()

    if not args.cfg.exists():
        print(f"Error: Config file {args.cfg} does not exist.")
        return

    with args.cfg.open() as f:
        try:
            param = hjson.loads(f.read())
        except Exception as e:
            print(f"Error reading config file: {e}")
            return

    kwargs = {
        "M": param.get("M"),
        "prec": param.get("prec"),
    }

    if not kwargs["M"] or not kwargs["prec"]:
        print("Error: Missing required parameters 'M' or 'prec' in config file.")
        return

    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()
