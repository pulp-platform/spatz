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

    emit_str += emit_analyze_var_to_check_msgs_layer(**kwargs)

    with file.open("w") as f:
        f.write(emit_str)


def emit_analyze_var_to_check_msgs_layer(**kwargs):
    rotated_node = kwargs["rotated_node"]
    min_vtc = kwargs["min_var_to_check"]
    second_min_vtc = kwargs["second_min_var_to_check"]
    min_vtc_index = kwargs["min_var_to_check_index"]
    sign_prod_vtc = kwargs["sign_prod_var_to_check"]
    lifting_size = kwargs["lifting_size"]
    num_cores = kwargs["num_cores"]
    var_node = kwargs["var_node"]

    total_size = lifting_size * num_cores

    layer_str = ""
    layer_str += f"const uint32_t lifting_size = {lifting_size};\n"
    layer_str += f"const uint32_t num_cores_gen = {num_cores};\n"
    layer_str += f"const uint32_t var_node = {var_node};\n\n"

    layer_str += (
        f'int8_t rotated_node_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(rotated_node)
        + ";\n\n\n"
    )
    layer_str += (
        f'int8_t min_var_to_check_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(min_vtc)
        + ";\n\n\n"
    )
    layer_str += (
        f'int8_t second_min_var_to_check_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(second_min_vtc)
        + ";\n\n\n"
    )
    layer_str += (
        f'uint8_t min_var_to_check_index_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(min_vtc_index)
        + ";\n\n\n"
    )
    layer_str += (
        f'uint8_t sign_prod_var_to_check_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(sign_prod_vtc)
        + ";\n\n\n"
    )
    return layer_str


def rand_rotated_node_generator(size, inf_prob=0.0, zero_prob=0.0):
    """Generates the incoming rotated LLR messages.

    Base values lie in [-LLR_MAX, LLR_MAX].

    inf_prob  : probability of forcing +/- LLR_INFTY, the largest magnitude the
                absolute value has to handle without overflowing int8.
    zero_prob : probability of forcing 0, which exercises the sign branch
                boundary (rot >= 0 must not flip the sign product).

    Note that -128 is never emitted: abs(-128) is not representable in int8 and
    the vector kernel would return -128 for it. The decoder never produces such
    a value, so the generator must not either.
    """
    v = np.random.randint(-LLR_MAX, LLR_MAX + 1, size=size).astype(np.int16)

    if inf_prob > 0:
        mask = np.random.rand(size) < inf_prob
        n = int(np.sum(mask))
        if n > 0:
            v[mask] = np.random.choice([-LLR_INFTY, LLR_INFTY], size=n)

    if zero_prob > 0:
        mask = np.random.rand(size) < zero_prob
        v[mask] = 0

    return v.astype(np.int8)


def rand_state_generator(size, tie_prob=0.0, narrow_prob=0.0, empty_prob=0.0):
    """Generates the accumulator state carried across var nodes.

    The invariant min <= second is preserved, as in the decoder.

    narrow_prob : probability of a small gap between min and second. This is the
                  case that catches a kernel writing the new min before the new
                  second is derived from the old one, since only elements with
                  min < abs < second expose that ordering bug.
    tie_prob    : probability of min == second, where both comparisons must agree.
    empty_prob  : probability of a saturated state (min = second = LLR_INFTY),
                  which is how the decoder initializes the accumulator before
                  the first var node, so every element takes the update path.
    """
    min_vtc = np.random.randint(0, LLR_INFTY + 1, size=size).astype(np.int16)
    gap = np.random.randint(0, 40, size=size).astype(np.int16)

    if narrow_prob > 0:
        mask = np.random.rand(size) < narrow_prob
        gap[mask] = np.random.randint(1, 4, size=int(np.sum(mask)))

    if tie_prob > 0:
        mask = np.random.rand(size) < tie_prob
        gap[mask] = 0

    second_min_vtc = np.minimum(min_vtc + gap, LLR_INFTY)

    if empty_prob > 0:
        mask = np.random.rand(size) < empty_prob
        min_vtc[mask] = LLR_INFTY
        second_min_vtc[mask] = LLR_INFTY

    min_vtc_index = np.random.randint(0, 26, size=size).astype(np.uint8)
    sign_prod_vtc = np.random.randint(0, 2, size=size).astype(np.uint8)

    return (
        min_vtc.astype(np.int8),
        second_min_vtc.astype(np.int8),
        min_vtc_index,
        sign_prod_vtc,
    )


def analyze_var_to_check_msgs_golden(
    rotated_node, min_vtc, second_min_vtc, min_vtc_index, sign_prod_vtc, var_node
):
    """Reference model of ldpc_decoder_generic::analyze_var_to_check_msgs."""
    rot = rotated_node.astype(np.int16)
    old_min = min_vtc.astype(np.int16)
    old_second = second_min_vtc.astype(np.int16)

    abs_v = np.abs(rot)
    is_min = abs_v < old_min
    new_second = np.where(is_min, old_min, abs_v)
    is_best_two = abs_v < old_second

    out_second = np.where(is_best_two, new_second, old_second)
    out_min = np.where(is_min, abs_v, old_min)
    out_index = np.where(is_min, var_node, min_vtc_index.astype(np.int16))
    out_sign = sign_prod_vtc ^ np.where(rot >= 0, 0, 1).astype(np.uint8)

    return (
        out_min.astype(np.int8),
        out_second.astype(np.int8),
        out_index.astype(np.uint8),
        out_sign,
    )


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
    var_node = param.get("var_node", 7)
    inf_prob = param.get("inf_prob", 0.0)
    zero_prob = param.get("zero_prob", 0.0)
    tie_prob = param.get("tie_prob", 0.0)
    narrow_prob = param.get("narrow_prob", 0.0)
    empty_prob = param.get("empty_prob", 0.0)

    total_size = lifting_size * num_cores

    rotated_node = rand_rotated_node_generator(
        total_size, inf_prob=inf_prob, zero_prob=zero_prob
    )
    min_vtc, second_min_vtc, min_vtc_index, sign_prod_vtc = rand_state_generator(
        total_size, tie_prob=tie_prob, narrow_prob=narrow_prob, empty_prob=empty_prob
    )

    if verbose:
        g_min, g_second, g_index, g_sign = analyze_var_to_check_msgs_golden(
            rotated_node, min_vtc, second_min_vtc, min_vtc_index, sign_prod_vtc, var_node
        )
        abs_v = np.abs(rotated_node.astype(np.int16))
        n_is_min = int(np.sum(abs_v < min_vtc.astype(np.int16)))
        n_between = int(
            np.sum(
                (abs_v >= min_vtc.astype(np.int16))
                & (abs_v < second_min_vtc.astype(np.int16))
            )
        )
        n_no_update = int(np.sum(abs_v >= second_min_vtc.astype(np.int16)))
        n_tie = int(np.sum(min_vtc == second_min_vtc))
        n_inf = int(np.sum(abs_v == LLR_INFTY))
        n_zero = int(np.sum(rotated_node == 0))
        print(f"elements        : {total_size}")
        print(f"var_node        : {var_node}")
        print(f"new min         : {n_is_min}")
        print(f"new second only : {n_between}")
        print(f"no update       : {n_no_update}")
        print(f"min == second   : {n_tie}")
        print(f"isinf(rot)      : {n_inf}")
        print(f"rot == 0        : {n_zero}")
        print(f"min range       : [{g_min.min()}, {g_min.max()}]")
        print(f"second range    : [{g_second.min()}, {g_second.max()}]")

    kwargs = {
        "rotated_node": rotated_node,
        "min_var_to_check": min_vtc,
        "second_min_var_to_check": second_min_vtc,
        "min_var_to_check_index": min_vtc_index,
        "sign_prod_var_to_check": sign_prod_vtc,
        "lifting_size": lifting_size,
        "num_cores": num_cores,
        "var_node": var_node,
    }

    emit_header_file("analyze_var_to_check_msgs", **kwargs)


if __name__ == "__main__":
    main()