#!/usr/bin/env python3
# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

import numpy as np
import torch
import argparse
import pathlib
import scipy
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
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    file = file_path / ("data_" + str(kwargs["R"]) + "_" + str(kwargs["C"]) + "_" + str(kwargs["F"]) + ".h")
    emit_str += emit_fconv2d_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_fconv2d_layer(name="fconv2d", **kwargs):
    vec_I = kwargs["imtx"]
    vec_F = kwargs["fmtx"]
    vec_R = kwargs["rmtx"]
    vec_GR = kwargs["grmtx"]

    ch = kwargs["CH"]
    r = kwargs["R"]
    c = kwargs["C"]
    f = kwargs["F"]

    layer_str = ""
    layer_str += '#include "layer.h"\n\n'
    layer_str += f"fconv2d_layer {name}_l = {{\n"
    layer_str += f"\t.CH = {ch},\n"
    layer_str += f"\t.R  = {r},\n"
    layer_str += f"\t.C  = {c},\n"
    layer_str += f"\t.F  = {f},\n"
    layer_str += f'\t.dtype = FP{kwargs["prec"]},\n'
    layer_str += "};\n\n\n"

    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}

    dtype = ctypes[str(kwargs["prec"])]
    if dtype != "char":
        layer_str += (
            f'static {dtype} {name}_I_dram [{ch}*{r+f-1}*{c+f-1}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_I)
            + ";\n\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_F_dram [{ch}*{f}*{f}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_F)
            + ";\n\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_R_dram [{r}*{c}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_R)
            + ";\n\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_GR_dram [{r}*{c}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_GR)
            + ";\n\n\n"
        )
    else:
        layer_str += (
            f"static {dtype} {name}_I_dram [{ch}*{r+f-1}*{c+f-1}] = "
            + array_to_cstr(kwargs["bits_I"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_F_dram [{ch}*{f}*{f}] = "
            + array_to_cstr(kwargs["bits_F"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_R_dram [{r}*{c}] = "
            + array_to_cstr(kwargs["bits_R"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_GR_dram [{r}*{c}] = "
            + array_to_cstr(kwargs["bits_R"], fmt="char")
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


def zero_data_generator(shape, prec, alt=False):
    if prec == 64:
        return torch.zeros(shape, requires_grad=False, dtype=torch.float64), {}
    elif prec == 32:
        return torch.zeros(shape, requires_grad=False, dtype=torch.float32), {}
    elif prec == 16:
        if alt:
            return torch.zeros(shape, requires_grad=False, dtype=torch.bfloat16), {}
        else:
            return torch.zeros(shape, requires_grad=False, dtype=torch.float16), {}
    elif prec == 8:
        sign = torch.zeros(
            0, 2, shape, requires_grad=False, dtype=torch.uint8
        )  # -1 or 1
        exponent = torch.zeros(
            0, 16, shape, requires_grad=False, dtype=torch.uint8
        )  # < 0b01111
        mantissa = torch.zeros(
            0, 4, shape, requires_grad=False, dtype=torch.uint8
        )  # can be arbitrary
        bits = {"sign": sign, "exponent": exponent, "mantissa": mantissa}
        # TODO: not actually correct
        return ((-1.0) ** sign.double()) * (2.0 ** (exponent.double() - 15.0)) * (
            1.0 + mantissa.double() / (2**2)
        ), bits


def fconv2d(i, f, r, CH, R, C, F, dtype):
    for ch in range(CH):
        r += scipy.signal.convolve2d(np.flip(f.tolist()[ch]), i[ch], "valid")
    return r


def zero_pad(a, CH, R, C, F):
    for ch in range(CH):
        for j in range(int((F - 1) / 2)):
            for i in range(R):
                a[ch][i][j] = 0
                a[ch][i][C - 1 - j] = 0
        for i in range(int((F - 1) / 2)):
            for j in range(C):
                a[ch][i][j] = 0
                a[ch][R - 1 - i][j] = 0
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

    vec_I, bits_I = rand_data_generator(
        (param["CH"], param["R"] + (param["F"] - 1), param["C"] + (param["F"] - 1)),
        param["prec"],
    )
    vec_F, bits_F = rand_data_generator(
        (param["CH"], param["F"], param["F"]), param["prec"]
    )
    vec_R, bits_R = zero_data_generator((param["R"], param["C"]), param["prec"])
    vec_GR, bits_GR = zero_data_generator((param["R"], param["C"]), param["prec"])
    # Pad the images internally
    vec_I = zero_pad(
        vec_I,
        param["CH"],
        param["R"] + (param["F"] - 1),
        param["C"] + (param["F"] - 1),
        param["F"],
    )
    # Fconv2d
    vec_GR = fconv2d(
        vec_I,
        vec_F,
        vec_GR,
        param["CH"],
        param["R"],
        param["C"],
        param["F"],
        param["prec"],
    )

    kwargs = {
        "imtx": vec_I,
        "fmtx": vec_F,
        "rmtx": vec_R,
        "grmtx": vec_GR,
        "CH": param["CH"],
        "R": param["R"],
        "C": param["C"],
        "F": param["F"],
        "prec": param["prec"],
        "expand": param["expand"],
        "bits_I": bits_I,
        "bits_F": bits_F,
    }

    emit_header_file("fconv2d", **kwargs)


if __name__ == "__main__":
    main()
