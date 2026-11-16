 
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
 
    emit_str += emit_compute_check_to_var_msgs_layer(**kwargs)
 
    with file.open("w") as f:
        f.write(emit_str)
 
 
def emit_compute_check_to_var_msgs_layer(**kwargs):
    this_var_to_check = kwargs["this_var_to_check"]
    min_vtc = kwargs["min_var_to_check"]
    second_min_vtc = kwargs["second_min_var_to_check"]
    min_vtc_index = kwargs["min_var_to_check_index"]
    sign_prod_vtc = kwargs["sign_prod_var_to_check"]
    lifting_size = kwargs["lifting_size"]
    num_cores = kwargs["num_cores"]
    var_node = kwargs["var_node"]
    shift = kwargs["shift"]
 
    total_size = lifting_size * num_cores
 
    layer_str = ""
    layer_str += f"const uint32_t lifting_size = {lifting_size};\n"
    layer_str += f"const uint32_t num_cores_gen = {num_cores};\n"
    layer_str += f"const uint32_t var_node = {var_node};\n"
    layer_str += f"const uint32_t shift = {shift};\n\n"
 
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
    layer_str += (
        f'int8_t this_var_to_check_dram [{total_size}] __attribute__((section(".data"))) = '
        + array_to_cstr(this_var_to_check)
        + ";\n\n\n"
    )
    return layer_str
 
 
def rand_llr_generator(size, inf_prob=0.0, zero_prob=0.0):
    """Un-rotated v2c LLRs of the current node (this_var_to_check).
 
    Base values lie in [-LLR_MAX, LLR_MAX]. Only the sign is consumed by compute,
    but values stay in the valid LLR range for realism.
 
    inf_prob  : probability of forcing +/- LLR_INFTY.
    zero_prob : probability of forcing 0. This is the sign-branch boundary: the
                decoder treats v2c >= 0 as a positive sign, so v2c == 0 must NOT
                flip the sign product (kernel uses v2c < 0).
 
    Note that -128 is never emitted (it is not a valid LLR magnitude).
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
 
 
def rand_aggregate_generator(
    size, var_node, tie_prob=0.0, empty_prob=0.0, hit_prob=0.0
):
    """Check-node aggregates as produced by analyze (the input to compute).
 
    Per lifted position: min1 <= min2 magnitudes, argmin index, sign product.
    The invariant min1 <= min2 is preserved, as in the decoder.
 
    hit_prob   : probability of forcing argmin == var_node. This is THE compute
                 edge case: it selects min2 (second) instead of min1. Without it
                 only ~1/26 of positions exercise the second-minimum branch.
    tie_prob   : probability of min1 == min2 (both branches yield the same value).
    empty_prob : probability of a saturated aggregate (min1 = min2 = LLR_INFTY),
                 exercising +/- LLR_INFTY magnitude flow and vneg(127) -> -127.
    """
    min_vtc = np.random.randint(0, LLR_INFTY + 1, size=size).astype(np.int16)
    gap = np.random.randint(0, 40, size=size).astype(np.int16)
 
    if tie_prob > 0:
        mask = np.random.rand(size) < tie_prob
        gap[mask] = 0
 
    second_min_vtc = np.minimum(min_vtc + gap, LLR_INFTY)
 
    if empty_prob > 0:
        mask = np.random.rand(size) < empty_prob
        min_vtc[mask] = LLR_INFTY
        second_min_vtc[mask] = LLR_INFTY
 
    min_vtc_index = np.random.randint(0, 26, size=size).astype(np.uint8)
 
    if hit_prob > 0:
        mask = np.random.rand(size) < hit_prob
        min_vtc_index[mask] = np.uint8(var_node)  # force the min2 (second) branch
 
    sign_prod_vtc = np.random.randint(0, 2, size=size).astype(np.uint8)
 
    return (
        min_vtc.astype(np.int8),
        second_min_vtc.astype(np.int8),
        min_vtc_index,
        sign_prod_vtc,
    )
 
 
def compute_check_to_var_msgs_golden(
    this_var_to_check,
    min_vtc,
    second_min_vtc,
    min_vtc_index,
    sign_prod_vtc,
    shift,
    var_node,
    lifting_size,
    num_cores,
):
    """Reference model of ldpc_decoder_generic::compute_check_to_var_msgs.
 
    Double-buffer variant, WITHOUT scaling (the kernel does not scale). tmp is the
    chunk-local un-rotated position (j - shift) mod Z.
    """
    total = lifting_size * num_cores
    out = np.empty(total, dtype=np.int8)
 
    for c in range(num_cores):
        base = c * lifting_size
        for j in range(lifting_size):
            tmp = (j + lifting_size - shift) % lifting_size
            a = base + tmp
            k = base + j
 
            if int(var_node) != int(min_vtc_index[a]):
                mag = int(min_vtc[a])
            else:
                mag = int(second_min_vtc[a])
 
            neg = 0 if int(this_var_to_check[k]) >= 0 else 1
            fsign = int(sign_prod_vtc[a]) ^ neg
            out[k] = np.int8(-mag if fsign else mag)
 
    return out
 
 
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
    empty_prob = param.get("empty_prob", 0.0)
    hit_prob = param.get("hit_prob", 0.0)
    shift_cfg = param.get("shift", None)
 
    total_size = lifting_size * num_cores
 
    # Aggregates (analyze output = compute input) and this node's un-rotated v2c.
    min_vtc, second_min_vtc, min_vtc_index, sign_prod_vtc = rand_aggregate_generator(
        total_size, var_node, tie_prob=tie_prob, empty_prob=empty_prob, hit_prob=hit_prob
    )
    this_var_to_check = rand_llr_generator(
        total_size, inf_prob=inf_prob, zero_prob=zero_prob
    )
 
    # shift drawn last so array contents don't depend on whether it is configured.
    if shift_cfg is None:
        shift = int(np.random.randint(0, lifting_size))
    else:
        shift = int(shift_cfg) % lifting_size
 
    if verbose:
        golden = compute_check_to_var_msgs_golden(
            this_var_to_check,
            min_vtc,
            second_min_vtc,
            min_vtc_index,
            sign_prod_vtc,
            shift,
            var_node,
            lifting_size,
            num_cores,
        )
        n_hit = int(np.sum(min_vtc_index.astype(np.int16) == var_node))
        n_v2c_neg = int(np.sum(this_var_to_check < 0))
        n_v2c_zero = int(np.sum(this_var_to_check == 0))
        n_tie = int(np.sum(min_vtc == second_min_vtc))
        n_min_inf = int(np.sum(min_vtc.astype(np.int16) == LLR_INFTY))
        n_out_neg = int(np.sum(golden < 0))
        n_out_zero = int(np.sum(golden == 0))
        print(f"elements          : {total_size}")
        print(f"lifting_size      : {lifting_size}")
        print(f"num_cores         : {num_cores}")
        print(f"var_node          : {var_node}")
        print(f"shift             : {shift}")
        print(f"argmin == var_node: {n_hit}   (positions taking the min2 branch)")
        print(f"v2c < 0           : {n_v2c_neg}")
        print(f"v2c == 0          : {n_v2c_zero}")
        print(f"min1 == min2      : {n_tie}")
        print(f"min1 == LLR_INFTY : {n_min_inf}")
        print(f"output < 0        : {n_out_neg}")
        print(f"output == 0       : {n_out_zero}")
        print(f"output range      : [{int(golden.min())}, {int(golden.max())}]")
 
    kwargs = {
        "this_var_to_check": this_var_to_check,
        "min_var_to_check": min_vtc,
        "second_min_var_to_check": second_min_vtc,
        "min_var_to_check_index": min_vtc_index,
        "sign_prod_var_to_check": sign_prod_vtc,
        "lifting_size": lifting_size,
        "num_cores": num_cores,
        "var_node": var_node,
        "shift": shift,
    }
 
    emit_header_file("compute_check_to_var_msgs", **kwargs)
 
 
if __name__ == "__main__":
    main()
 