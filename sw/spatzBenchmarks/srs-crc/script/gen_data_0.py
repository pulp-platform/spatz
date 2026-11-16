#!/usr/bin/env python3
# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Ihor Sachok <ihor.sachok@unibo.it>

import numpy as np
import argparse
import pathlib
import hjson

np.random.seed(42)


def array_to_cstr(a):
    return "{" + ", ".join(str(int(x)) for x in a) + "}"


def emit_header_file(msg_len, num_cores, msg):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    emit_str = (
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    emit_str += f"const uint32_t msg_len = {msg_len};\n"
    emit_str += f"const uint32_t num_cores_gen = {num_cores};\n\n"
    emit_str += (
        f'int8_t crc_msg_dram [{msg_len}] __attribute__((section(".data"))) = '
        + array_to_cstr(msg)
        + ";\n"
    )

    # File name matches the CMake macro: data_<M>_<num_cores>.h
    file = file_path / ("data_" + str(msg_len) + "_" + str(num_cores) + ".h")
    with file.open("w") as f:
        f.write(emit_str)


def main():
    parser = argparse.ArgumentParser(description="Generate data for CRC kernel")
    parser.add_argument(
        "-c", "--cfg", type=pathlib.Path, required=True,
        help="Select param config file for kernel",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")
    args = parser.parse_args()

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    msg_len   = param["M"]
    num_cores = param.get("num_cores", 2)

    # Random message bytes as signed int8 (-128..127) to match int8_t storage
    msg = np.random.randint(-128, 128, size=msg_len, dtype=np.int8)

    emit_header_file(msg_len, num_cores, msg)


if __name__ == "__main__":
    main()