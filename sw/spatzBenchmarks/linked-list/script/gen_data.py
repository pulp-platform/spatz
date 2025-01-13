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


def generate_single_list(M, prec, index):
    """
    Generate a single linked list with random data.

    Args:
        M (int): Number of nodes in the list.
        prec (int): Precision of the data (8, 16, 32, or 64 bits).
        index (int): Index of the linked list (for naming).

    Returns:
        tuple: A tuple containing the nodes C code and the head pointer.
    """
    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}
    dtype = ctypes[str(prec)]

    # Generate sequential IDs and random data
    ids = list(range(M))
    data_values = []
    for _ in range(M):
        if prec == 8:
            data_values.append(random.randint(-128, 127))  # char range
        elif prec == 16:
            data_values.append(round(random.uniform(-100, 100), 3))  # Simulate __fp16
        else:
            data_values.append(round(random.uniform(-100.0, 100.0), 6))  # float/double

    # Create nodes in order
    nodes = [{"id": node_id, "data": data_values[i], "next": None} for i, node_id in enumerate(ids)]

    # Assign .next pointers in order
    for i in range(M - 1):
        nodes[i]["next"] = i + 1  # Point to the next node
    nodes[M - 1]["next"] = None  # Last node points to NULL

    # Shuffle the nodes
    shuffled_indices = list(range(M))
    random.shuffle(shuffled_indices)
    shuffled_nodes = [nodes[i] for i in shuffled_indices]

    # Map original indices to shuffled indices for .next pointers
    index_map = {original: shuffled_indices.index(original) for original in range(M)}

    # Adjust .next pointers to shuffled indices
    for node in shuffled_nodes:
        if node["next"] is not None:
            node["next"] = index_map[node["next"]]
        else:
            node["next"] = "NULL"

    # Generate C code for this linked list
    list_str = f"node_t nodes{index}[] = {{\n"
    for node in shuffled_nodes:
        next_pointer = f"&nodes{index}[{node['next']}]" if node["next"] != "NULL" else "NULL"
        list_str += (
            f"    {{.next = {next_pointer}, .payload = {{.id = {node['id']}, .data = ({dtype}){node['data']}}}}},\n"
        )
    list_str += "};\n\n"

    # Find the index of the node with id = 0 for the head pointer
    head_index = shuffled_indices.index(0)
    head_pointer = f"&nodes{index}[{head_index}]"
    return list_str, head_pointer

def generate_multiple_lists(**kwargs):
    """
    Generate multiple linked lists with random data.

    Args:
        kwargs: Contains M (number of nodes per list), prec (data precision), and N_list (number of lists).

    Returns:
        str: C code for all linked lists and the head array.
    """
    M = kwargs["M"]
    prec = kwargs["prec"]
    N_list = kwargs["N_list"]

    list_code = ""
    head_pointers = []

    for i in range(N_list):
        single_list_code, head_pointer = generate_single_list(M, prec, i)
        list_code += single_list_code
        head_pointers.append(head_pointer)

    # Generate the head array
    head_array = f"node_t* head[{N_list}] = {{\n"
    for head_pointer in head_pointers:
        head_array += f"    {head_pointer},\n"
    head_array += "};\n\n"

    return list_code + head_array


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    header_content = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )

    header_content += '#include "layer.h"\n\n'

    header_content += f'int N = {kwargs["M"]};\n\n'

    header_content += f'int N_list = {kwargs["N_list"]};\n\n'

    # header_content += emit_struct(**kwargs)
    # header_content += generate_node_array(**kwargs)

    # for i in range(kwargs["Nlist"]):
    header_content += generate_multiple_lists(**kwargs)

    file = file_path / f"data_{kwargs['M']}_{kwargs['N_list']}.h"
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
        "N_list": param.get("N_list"),
        "M": param.get("M"),
        "prec": param.get("prec"),
    }

    if not kwargs["M"] or not kwargs["prec"]:
        print("Error: Missing required parameters 'M' or 'prec' in config file.")
        return

    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()
