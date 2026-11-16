#!/usr/bin/env python3
# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

import numpy as np
import argparse
import pathlib
import hjson

np.random.seed(42)

global verbose


def array_to_cstr(a):
    out = "{"
    if isinstance(a, np.ndarray):
        a = a.flatten()
    for el in a:
        out += "{}, ".format(el)
    out = out[:-2] + "}"
    return out


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    emit_str = (
        "// Copyright 2023 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    file = file_path / ("data_{}.h".format(kwargs["M"]))
    emit_str += emit_precoding_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)


def emit_precoding_layer(name="precoding", **kwargs):
    m = kwargs["M"]
    nof_layers = kwargs["nof_layers"]
    input_re = kwargs["input_re"]
    port_weights = kwargs["port_weights"]

    layer_str = ""
    layer_str += '#include "layer.h"\n\n'
    # nof_layers is driven by the config; precoding.c reads it via NOF_LAYERS.
    layer_str += "#define NOF_LAYERS {}\n\n".format(nof_layers)
    layer_str += f"precoding_layer {name}_l = {{\n"
    layer_str += f"\t.M = {m},\n"
    layer_str += f'\t.dtype = FP{kwargs["prec"]},\n'
    layer_str += "};\n\n\n"

    # Names must match what main.c / precoding_v32b() expect:
    #   input_re_in_dram      : nof_layers * M floats, per layer [re, im, re, im, ...]
    #   port_weights_in_dram  : 2 floats per layer, [re, im] per layer
    layer_str += (
        f"static float input_re_in_dram [{nof_layers * m}] "
        '__attribute__((section(".data"))) = '
        + array_to_cstr(input_re)
        + ";\n\n\n"
    )
    layer_str += (
        f"static float port_weights_in_dram [{2 * nof_layers}] "
        '__attribute__((section(".data"))) = '
        + array_to_cstr(port_weights)
        + ";\n\n\n"
    )
    return layer_str


def main():
    parser = argparse.ArgumentParser(
        description="Generate data for the precoding kernel"
    )
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

    m = param["M"]
    nof_layers = param["nof_layers"]
    prec = param["prec"]

    input_re = np.random.randn(nof_layers, m).astype(np.float32)
    port_weights = np.random.randn(nof_layers, 2).astype(np.float32)

    if verbose:
        print(f"M={m}, nof_layers={nof_layers}, prec={prec}")
        print(f"input_re shape     : {input_re.shape}")
        print(f"port_weights shape : {port_weights.shape}")

    kwargs = {
        "M": m,
        "prec": prec,
        "nof_layers": nof_layers,
        "input_re": input_re,
        "port_weights": port_weights,
    }

    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()
