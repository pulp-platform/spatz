#!/usr/bin/env python3
# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

import numpy as np
import torch
import argparse
import pathlib
import hjson

np.random.seed(42)
torch.manual_seed(42)

global verbose


def array_to_cstr(a, fmt=float):
    out = "{"
    if fmt == float:
        if isinstance(a, np.ndarray):
            a = a.flat
        if isinstance(a, torch.Tensor):
            a = a.numpy().flat
        for el in a:
            out += "{}, ".format(el)
    else:
        for sign, exp, mant in zip(
            a["sign"].numpy().flat,
            a["exponent"].numpy().flat,
            a["mantissa"].numpy().flat,
        ):
            value = sign * 2**7 + exp * 2**2 + mant
            out += "0x{:02x}, ".format(value)
    out = out[:-2] + "}"
    return out


def emit_header_file(layer_type: str, **kwargs):

    file_path = pathlib.Path(__file__).parent.parent / "data"
    emit_str = (
        "// Copyright 2022 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
    )

    file = file_path / "data_jacobi2d.h"
    emit_str += emit_jacobi2d_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_jacobi2d_layer(name="jacobi2d", **kwargs):
    vec_A = kwargs["A"]
    vec_B = kwargs["B"]
    result = kwargs["result"]

    r = kwargs["R"]
    c = kwargs["C"]
    r_dim_core = kwargs["r_dim_core"]
    c_dim_core = kwargs["c_dim_core"]

    layer_str = ""
    layer_str += '#include "layer.h"\n\n'
    layer_str += f"jacobi2d_layer {name}_l = {{\n"
    layer_str += f"\t.R = {r},\n"
    layer_str += f"\t.C = {c},\n"
    layer_str += f"\t.r_dim_core = {r_dim_core},\n"
    layer_str += f"\t.c_dim_core = {c_dim_core},\n"
    layer_str += f'\t.dtype = FP{kwargs["prec"]},\n'
    layer_str += "};\n\n\n"

    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}

    dtype = ctypes[str(kwargs["prec"])]
    if dtype != "char":
        layer_str += (
            f'static {dtype} {name}_A_dram [{r}*{c}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_A)
            + ";\n\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_B_dram [{r}*{c}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_B)
            + ";\n\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_result [{r}*{c}] __attribute__((section(".data"))) = '
            + array_to_cstr(result)
            + ";\n\n\n"
        )
    else:
        layer_str += (
            f"static {dtype} {name}_A_dram [{r}*{c}] = "
            + array_to_cstr(kwargs["bits_A"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_B_dram [{r}*{c}] = "
            + array_to_cstr(kwargs["bits_B"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_result [{r}*{c}] = "
            + array_to_cstr(kwargs["result"], fmt="char")
            + ";\n\n\n"
        )

    return layer_str


def rand_data_generator(shape, prec, alt=False):
    if prec == 64:
        return torch.randn(shape, requires_grad=False, dtype=torch.float64), {}
    elif prec == 32:
        return torch.randn(shape, requires_grad=False, dtype=torch.float32), {}
    elif prec == 16:
        if alt:
            return torch.randn(shape, requires_grad=False, dtype=torch.bfloat16), {}
        else:
            return torch.randn(shape, requires_grad=False, dtype=torch.float16), {}
    elif prec == 8:
        sign = torch.randint(
            0, 2, shape, requires_grad=False, dtype=torch.uint8
        )  # -1 or 1
        exponent = torch.randint(
            0, 16, shape, requires_grad=False, dtype=torch.uint8
        )  # < 0b01111
        mantissa = torch.randint(
            0, 4, shape, requires_grad=False, dtype=torch.uint8
        )  # can be arbitrary
        bits = {"sign": sign, "exponent": exponent, "mantissa": mantissa}
        # TODO: not actually correct
        return ((-1.0) ** sign.double()) * (2.0 ** (exponent.double() - 15.0)) * (
            1.0 + mantissa.double() / (2**2)
        ), bits


def jacobi2d(a, b, temp, R, C):
    for i in range(1, R - 1):
        for j in range(1, C - 1):
            b[i][j] = 0.2 * (
                a[i][j] + a[i - 1][j] + a[i + 1][j] + a[i][j - 1] + a[i][j + 1]
            )
    for i in range(1, R - 1):
        for j in range(1, C - 1):
            temp[i][j] = 0.2 * (
                b[i][j] + b[i - 1][j] + b[i + 1][j] + b[i][j - 1] + b[i][j + 1]
            )
    return temp


def zero_pad(a, R, C):
    for i in range(R):
        a[i][0] = 0
        a[i][C - 1] = 0
    for j in range(C):
        a[0][j] = 0
        a[R - 1][j] = 0
    return a


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

    vec_A, bits_A = rand_data_generator((param["R"], param["C"]), param["prec"])
    vec_B, bits_B = rand_data_generator((param["R"], param["C"]), param["prec"])
    temp, tbits = rand_data_generator((param["R"], param["C"]), param["prec"])
    # Pad the images internally
    vec_A = zero_pad(vec_A, param["R"], param["C"])
    vec_B = zero_pad(vec_B, param["R"], param["C"])
    temp = zero_pad(temp, param["R"], param["C"])
    # Jacobi2d
    result = jacobi2d(vec_A, vec_B, temp, param["R"], param["C"])

    kwargs = {
        "A": vec_A,
        "B": vec_B,
        "result": result,
        "R": param["R"],
        "C": param["C"],
        "r_dim_core": param["r_dim_core"],
        "c_dim_core": param["c_dim_core"],
        "prec": param["prec"],
        "expand": param["expand"],
        "bits_A": bits_A,
        "bits_B": bits_B,
    }

    emit_header_file("jacobi2d", **kwargs)


if __name__ == "__main__":
    main()
