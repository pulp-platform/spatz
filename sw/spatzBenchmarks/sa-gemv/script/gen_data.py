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
    out = "{\n"
    if fmt == float:
        if isinstance(a, np.ndarray):
            a = a.flat
        if isinstance(a, torch.Tensor):
            a = a.numpy().flat
        for el in a:
            out += "\t{},\n".format(el)
    else:
        for sign, exp, mant in zip(
            a["sign"].numpy().flat,
            a["exponent"].numpy().flat,
            a["mantissa"].numpy().flat,
        ):
            value = sign * 2**7 + exp * 2**2 + mant
            out += "0x{:02x},\n".format(value)
    out = out[:-2] + "}"
    return out


def emit_header_file(layer_type: str, **kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    emit_str = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    file = file_path / ("data_" + str(kwargs["M"]) + "_" + str(kwargs["N"]) + "_" + str(kwargs["tot_nz"]) + "_" + str(kwargs["prec"]) + ".h")
    emit_str += emit_gemv_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_gemv_layer(name="gemv", **kwargs):
    mat_A = kwargs["A"]
    vec_B = kwargs["B"]
    result = kwargs["result"]

    m = kwargs["M"]
    n = kwargs["N"]
    tot_nz = kwargs["tot_nz"]

    layer_str = ""
    layer_str += '#include "layer.h"\n\n'
    layer_str += f"const gemv_layer {name}_l = {{\n"
    layer_str += f"\t.M = {m},\n"
    layer_str += f"\t.N = {n},\n"
    layer_str += f'\t.dtype = FP{kwargs["prec"]}'
    layer_str += "};\n\n"

    # Export the total non-zeros directly so the kernel can use it
    layer_str += f"const uint32_t tot_nz_dram = {tot_nz};\n\n"

    ctypes = {"64": "double", "32": "float", "16": "__fp16", "8": "char"}

    dtype = ctypes[str(kwargs["prec"])]
    if dtype != "char":
        layer_str += (
            f'static {dtype} {name}_mat_dram[{m}*{n}] __attribute__((section(".data"))) = '
            + array_to_cstr(mat_A)
            + ";\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_vec_dram[{n}] __attribute__((section(".data"))) = '
            + array_to_cstr(vec_B)
            + ";\n\n"
        )
        layer_str += (
            f'static {dtype} {name}_result[{m}] __attribute__((section(".data"))) = '
            + array_to_cstr(result)
            + ";\n"
        )
        # Assuming you have variables like M (output size) and tot_nz (number of non-zeros)
        layer_str += f'// Auto-generated buffers for Cache Mode\n'
        layer_str += f'static uint16_t dense_idx_dram[{tot_nz}] __attribute__((section(".data"))) = {{0}};\n'
        layer_str += f'static {dtype} dense_vec_dram[{tot_nz}] __attribute__((section(".data"))) = {{0.0}};\n'
        layer_str += f'static {dtype} result_buf_dram[{m}] __attribute__((section(".data"))) = {{0.0}};\n'
    else:
        layer_str += (
            f"static {dtype} {name}_mat_dram[{m}*{n}] = "
            + array_to_cstr(kwargs["bits_A"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_vec_dram[{n}] = "
            + array_to_cstr(kwargs["bits_B"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_result[{m}] = "
            + array_to_cstr(kwargs["result"], fmt="char")
            + ";\n\n\n"
        )
        layer_str += (
            f"static {dtype} {name}_result_buf_dram[{m}] ="
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
            # Generate in FP32, cast to BF16
            return torch.randn(shape, requires_grad=False, dtype=torch.float32).to(torch.bfloat16), {}
        else:
            # Generate in FP32, cast to FP16
            return torch.randn(shape, requires_grad=False, dtype=torch.float32).to(torch.float16), {}
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


def gemv(a, b):
    print(a.shape, b.shape)
    # Upcast to float32 for CPU math, then downcast back to the original dtype
    return torch.matmul(a.float(), b.float()).to(a.dtype)

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

    # Read tot_nz from the hjson file
    tot_nz = param["tot_nz"]

    mat_A, bits_A = rand_data_generator((param["M"], param["N"]), param["prec"])
    vec_B, bits_B = rand_data_generator((param["N"], 1), param["prec"])

    # --- Sparsity Logic ---
    # Randomly select `tot_nz` indices to keep, set the rest to 0.0
    nz_indices = torch.randperm(param["N"])[:tot_nz]
    mask = torch.zeros((param["N"], 1), dtype=torch.bool)
    mask[nz_indices, 0] = True

    # Temporarily upcast to float32 for the masking math, then cast back
    vec_B = (vec_B.float() * mask).to(vec_B.dtype)

    # Also zero out the raw bits if using 8-bit precision to maintain parity
    if bool(bits_B):
        for k in bits_B.keys():
            # Apply the mask, ensuring the shape matches the 1D bits array format
            bits_B[k] = bits_B[k] * mask.squeeze().byte()
    # ----------------------

    # Calculate result using the now-sparse vector
    result = gemv(mat_A, vec_B)

    # Store A in col major format
    mat_A = mat_A.T

    kwargs = {
        "A": mat_A,
        "B": vec_B,
        "result": result,
        "M": param["M"],
        "N": param["N"],
        "tot_nz": tot_nz,  # Pass the new parameter down
        "prec": param["prec"],
        "expand": param["expand"],
        "bits_A": bits_A,
        "bits_B": bits_B,
    }

    emit_header_file("gemv", **kwargs)


if __name__ == "__main__":
    main()
