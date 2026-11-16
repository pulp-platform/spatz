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
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )
 
    file = file_path / (
        "data_" + str(kwargs["lifting_size"]) + "_" + str(kwargs["num_cores"]) + ".h"
    )
 
    emit_str += emit_compute_soft_bits_layer(**kwargs)
 
    with file.open("w") as f:
        f.write(emit_str)
 
 
def emit_compute_soft_bits_layer(name="this", **kwargs):
    var_to_check = kwargs["var_to_check"]
    check_to_var = kwargs["check_to_var"]
    lifting_size = kwargs["lifting_size"]
    num_cores    = kwargs["num_cores"]

    total_size = lifting_size * num_cores

    layer_str = ""
    layer_str += f"const uint32_t lifting_size = {lifting_size};\n"
    layer_str += f"const uint32_t num_cores_gen = {num_cores};\n\n"

    layer_str += (
        f'int8_t {name}_var_to_check_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(var_to_check)
        + ";\n\n\n"
    )
    layer_str += (
        f'int8_t {name}_check_to_var_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(check_to_var)
        + ";\n\n\n"
    )
    return layer_str
 
 
def rand_llr_pair_generator(
    size,
    llr_max=LLR_MAX,
    inf_prob=0.0,
    opposite_prob=0.0,
    saturate_prob=0.0,
):
    """Generates two random int8 LLR arrays (a, b) for promotion_sum.
 
    Base values lie in [-llr_max, llr_max]. Special-case coverage is injected on top:
 
    inf_prob      : probability of forcing a or b to +/- LLR_INFTY (isinf path).
    opposite_prob : probability of forcing b = -a (the a == -b path, returns 0).
    saturate_prob : probability of forcing |a + b| > llr_max without either being
                    infinite (the clipping path, returns +/- LLR_INFTY).
    """
    a = np.random.randint(-llr_max, llr_max + 1, size=size).astype(np.int16)
    b = np.random.randint(-llr_max, llr_max + 1, size=size).astype(np.int16)
 
    # Clipping path: both summands large and of the same sign.
    if saturate_prob > 0:
        mask = np.random.rand(size) < saturate_prob
        n = int(np.sum(mask))
        if n > 0:
            sign = np.random.choice([-1, 1], size=n)
            half = llr_max // 2 + 1
            a[mask] = sign * np.random.randint(half, llr_max + 1, size=n)
            b[mask] = sign * np.random.randint(half, llr_max + 1, size=n)
 
    # Infinity path: one of the two summands saturates to +/- LLR_INFTY.
    if inf_prob > 0:
        mask = np.random.rand(size) < inf_prob
        n = int(np.sum(mask))
        if n > 0:
            sign = np.random.choice([-1, 1], size=n)
            which = np.random.rand(n) < 0.5
            idx = np.flatnonzero(mask)
            a[idx[which]] = sign[which] * LLR_INFTY
            b[idx[~which]] = sign[~which] * LLR_INFTY
 
    # Opposite path: b = -a, so the sum is exactly zero. Applied last so it wins,
    # mirroring the priority of tackle_special_sums.
    if opposite_prob > 0:
        mask = np.random.rand(size) < opposite_prob
        b[mask] = -a[mask]
 
    return a.astype(np.int8), b.astype(np.int8)
 
 
def promotion_sum_golden(a, b):
    """Reference model of log_likelihood_ratio::promotion_sum, elementwise."""
    a = a.astype(np.int16)
    b = b.astype(np.int16)
 
    tmp = a + b
    out = np.where(np.abs(tmp) > LLR_MAX, np.sign(tmp) * LLR_INFTY, tmp)
 
    # tackle_special_sums, in reverse priority order.
    out = np.where(np.abs(b) == LLR_INFTY, b, out)
    out = np.where(np.abs(a) == LLR_INFTY, a, out)
    out = np.where(a == -b, 0, out)
 
    return out.astype(np.int8)
 
 
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
    inf_prob = param.get("inf_prob", 0.0)
    opposite_prob = param.get("opposite_prob", 0.0)
    saturate_prob = param.get("saturate_prob", 0.0)
 
    # Generate independent data for every core: total length = num_cores * lifting_size.
    var_to_check, check_to_var = rand_llr_pair_generator(
        lifting_size * num_cores,
        llr_max=LLR_MAX,
        inf_prob=inf_prob,
        opposite_prob=opposite_prob,
        saturate_prob=saturate_prob,
    )
 
    if verbose:
        golden = promotion_sum_golden(var_to_check, check_to_var)
        n_zero = int(np.sum(var_to_check == -check_to_var))
        n_inf = int(
            np.sum(
                (np.abs(var_to_check.astype(np.int16)) == LLR_INFTY)
                | (np.abs(check_to_var.astype(np.int16)) == LLR_INFTY)
            )
        )
        n_sat = int(
            np.sum(
                np.abs(
                    var_to_check.astype(np.int16) + check_to_var.astype(np.int16)
                )
                > LLR_MAX
            )
        )
        print(f"elements     : {var_to_check.size}")
        print(f"a == -b      : {n_zero}")
        print(f"isinf(a|b)   : {n_inf}")
        print(f"|a + b| > max: {n_sat}")
        print(f"golden range : [{golden.min()}, {golden.max()}]")
 
    kwargs = {
        "var_to_check": var_to_check,
        "check_to_var": check_to_var,
        "lifting_size": lifting_size,
        "num_cores": num_cores,
    }
 
    emit_header_file("compute_soft_bits", **kwargs)
 
 
if __name__ == "__main__":
    main()