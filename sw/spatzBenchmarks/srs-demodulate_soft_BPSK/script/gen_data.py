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
RANGE_LIMIT = np.float32(24.0)


def float_lit(x):
    """C float literal that is always valid (1 -> 1.0f, inf -> INFINITY, nan -> NAN)."""
    x = float(x)
    if np.isnan(x):
        return "NAN"
    if np.isinf(x):
        return "INFINITY" if x > 0 else "-INFINITY"
    s = "{:.9g}".format(x)
    if not any(c in s for c in ".en"):
        s += ".0"
    return s + "f"


def array_to_cstr_float(a):
    out = "{"
    for el in np.asarray(a, dtype=np.float32).flat:
        out += float_lit(el) + ", "
    return out[:-2] + "}"


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    emit_str = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )
    name = "data_" + str(kwargs["nof_symbols"])
    if kwargs.get("tag"):
        name += "_" + str(kwargs["tag"])
    file = file_path / (name + ".h")
    emit_str += emit_demodulate_soft_bpsk_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)
    if verbose:
        print("Wrote", file)


def emit_demodulate_soft_bpsk_layer(**kwargs):
    nof_symbols = kwargs["nof_symbols"]

    layer_str = ""
    layer_str += f"const uint32_t nof_symbols_dram = {nof_symbols};\n\n"
    layer_str += (
        f'float symbols_dram [{2 * nof_symbols}] __attribute__((section(".data"))) = '
        + array_to_cstr_float(kwargs["symbols"])
        + ";\n\n"
    )
    layer_str += (
        f'float noise_vars_dram [{nof_symbols}] __attribute__((section(".data"))) = '
        + array_to_cstr_float(kwargs["noise_vars"])
        + ";\n\n"
    )
    layer_str += (
        f'int8_t llrs_result_dram [{nof_symbols}] __attribute__((section(".data"))) = '
        + "{" + ", ".join(str(int(v)) for v in kwargs["llrs_result"]) + "}"
        + ";\n\n\n"
    )
    return layer_str


def golden(symbols, noise_vars):
    """float32 reference, same as demodulate_soft_BPSK_check / quantize in main.c."""
    f32 = np.float32
    re, im = symbols[0::2], symbols[1::2]
    with np.errstate(all="ignore"):
        x = (f32(2.0) * f32(np.sqrt(2.0))) * (re + im) / noise_vars
        x = np.clip(x, -RANGE_LIMIT, RANGE_LIMIT).astype(f32)
        x = (x / RANGE_LIMIT * f32(LLR_MAX)).astype(f32)
    # roundf: half away from zero (f32 -> f64 is exact, so +0.5 is exact)
    x = x.astype(np.float64)
    llrs = np.sign(x) * np.floor(np.abs(x) + 0.5)
    llrs[~(noise_vars > 0)] = 0
    return llrs.astype(np.int8)


def gen_symbols(n):
    """Complex symbols, re/im interleaved, uniform in [-1, 1]."""
    out = np.empty(2 * n, dtype=np.float32)
    out[0::2] = np.random.uniform(-1.0, 1.0, n)
    out[1::2] = np.random.uniform(-1.0, 1.0, n)
    return out


def build_noise(param, nof_symbols):
    """Own noise_var for every symbol, uniform in [0.05, 1.0].
    |re + im| <= 2, so |2*sqrt(2)*(re + im) / nv| reaches the clip limit 24
    only for nv < ~0.24: the range gives both clipped and unclipped LLRs.
    With probability zero_prob noise_var is 0."""
    nv = np.random.uniform(0.05, 1.0, nof_symbols).astype(np.float32)
    nv[np.random.rand(nof_symbols) < param.get("zero_prob", 0.0)] = 0.0
    return nv


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

    nof_symbols = param["nof_symbols"]
    tag = param.get("tag", "")

    symbols = gen_symbols(nof_symbols)
    noise_vars = build_noise(param, nof_symbols)

    kwargs = {
        "nof_symbols": nof_symbols,
        "tag": tag,
        "symbols": symbols,
        "noise_vars": noise_vars,
        "llrs_result": golden(symbols, noise_vars),
    }
    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()