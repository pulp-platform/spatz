#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Bowen Wang, ETH Zurich
#
# Generates data for the sp-dictdecode benchmark (database dictionary
# decompression): a column of N_CODES codes indexes a dictionary of K
# fixed-width entries (D x fp32); the golden output is the materialized
# values column. Follows the emit_header_file pattern of
# hp-vqmatmul/script/gen_data.py.
#
# The benchmark verifies with an exact bit-match, so fp32 values are
# emitted as IEEE-754 bit patterns (uint32) rather than decimal literals.

import argparse
import json
import pathlib

import numpy as np

np.random.seed(42)

global verbose


def array_to_cstr(a, fmt="0x{:08x}"):
    """Emit a C initializer list of hex literals."""
    out = "{"
    for el in np.asarray(a).flat:
        out += (fmt + ", ").format(int(el))
    return out[:-2] + "}"


def emit_dictdecode_layer(**kwargs):
    n_codes = kwargs["N_CODES"]
    k = kwargs["K"]
    d = kwargs["D"]
    code_bytes = kwargs["CODE_BYTES"]
    dict_vals = kwargs["dict_vals"]  # [K, D] fp32
    codes = kwargs["codes"]  # [N_CODES] uint
    golden = kwargs["golden"]  # [N_CODES * D] fp32

    code_ctype = "uint8_t" if code_bytes == 1 else "uint16_t"
    code_fmt = "0x{:02x}" if code_bytes == 1 else "0x{:04x}"

    layer_str = ""
    layer_str += "#include <stdint.h>\n"
    layer_str += '#include "layer.h"\n\n'
    layer_str += f"#define DICT_N_CODES {n_codes}\n"
    layer_str += f"#define DICT_K {k}\n"
    layer_str += f"#define DICT_D {d}\n"
    layer_str += f"#define DICT_CODE_BYTES {code_bytes}\n\n"

    layer_str += "const dictdecode_layer dict_l = {\n"
    layer_str += f"\t.N_CODES = {n_codes},\n"
    layer_str += f"\t.K = {k},\n"
    layer_str += f"\t.D = {d},\n"
    layer_str += f"\t.CODE_BYTES = {code_bytes}\n"
    layer_str += "};\n\n\n"

    # fp32 values as IEEE-754 bit patterns (bit-exact init for exact-match
    # verification; the benchmark DMAs raw bytes and treats them as float).
    layer_str += (
        f"static uint32_t dict_vals_dram[{k}*{d}] "
        '__attribute__((section(".data"))) = '
        + array_to_cstr(dict_vals.astype(np.float32).view(np.uint32))
        + ";\n\n\n"
    )
    layer_str += (
        f"static {code_ctype} dict_codes_dram[{n_codes}] "
        '__attribute__((section(".data"))) = '
        + array_to_cstr(codes, fmt=code_fmt)
        + ";\n\n\n"
    )
    layer_str += (
        f"static const uint32_t dict_golden[{n_codes}*{d}] = "
        + array_to_cstr(golden.astype(np.float32).view(np.uint32))
        + ";\n\n\n"
    )

    return layer_str


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)

    emit_str = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for "
        "details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )

    # Keep the historical name for the original D=4 shape; disambiguate
    # other slot widths with a _d<D> suffix.
    d_suffix = "" if kwargs["D"] == 4 else f"_d{kwargs['D']}"
    file = file_path / (
        "data_dict_"
        + str(kwargs["N_CODES"])
        + "_"
        + str(kwargs["K"])
        + d_suffix
        + ".h"
    )
    emit_str += emit_dictdecode_layer(**kwargs)

    with file.open("w") as f:
        f.write(emit_str)

    if verbose:
        print(f"Wrote: {file}")


def main():
    parser = argparse.ArgumentParser(description="Generate data for kernels")
    parser.add_argument(
        "-c",
        "--cfg",
        type=pathlib.Path,
        required=True,
        help="Select param config file kernel (JSON)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = json.load(f)

    if param["kernel"] != "DICT-DECODE":
        raise ValueError('Unsupported kernel in cfg. Use kernel: "DICT-DECODE".')

    n_codes = int(param["N_CODES"])
    k = int(param["K"])
    d = int(param.get("D", 4))
    code_bytes = int(param.get("CODE_BYTES", 2))

    assert d in (1, 2, 4, 8, 16, 32, 64, 128), "sp-dictdecode kernels support pow2 D in 1..128"
    assert code_bytes in (1, 2), "CODE_BYTES must be 1 or 2"
    assert k <= (256 if code_bytes == 1 else 65536), "K exceeds code width"
    assert k * d * 4 <= 65536, "RVV baseline uses 16-bit byte offsets"

    # Dictionary: K entries of D fp32 (a decimal/date record slot each).
    dict_vals = np.random.randn(k, d).astype(np.float32)
    # Codes: uniform in [0, K).
    codes = np.random.randint(0, k, size=n_codes).astype(
        np.uint8 if code_bytes == 1 else np.uint16
    )
    # Golden: materialized values column (pure gather, no arithmetic).
    golden = dict_vals[codes].reshape(-1)

    emit_header_file(
        N_CODES=n_codes,
        K=k,
        D=d,
        CODE_BYTES=code_bytes,
        dict_vals=dict_vals,
        codes=codes,
        golden=golden,
    )

    if verbose:
        print(
            f"Generated dictdecode data: N_CODES={n_codes}, K={k}, D={d}, "
            f"CODE_BYTES={code_bytes}"
        )


if __name__ == "__main__":
    main()
