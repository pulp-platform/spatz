#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Data generator for the O-POPE matmul benchmark. Mirrors the O-POPE golden model
# (opope/golden-model/FP32/scripts/gemm.py): Z = Y + X @ W, X is stored transposed
# in memory when transpose_X is set (default of the O-POPE flow).

import argparse
import pathlib

import hjson
import numpy as np
import torch


def to_words(t):
    """FP32 tensor (row-major) -> list of uint32 bit patterns."""
    return np.ascontiguousarray(t.numpy().astype(np.float32)).view(np.uint32).flatten()


def carray(name, words, section=".data"):
    body = ", ".join("0x{:08x}".format(int(w)) for w in words)
    return (f'static uint32_t {name}[{len(words)}] '
            f'__attribute__((section("{section}"))) = {{{body}}};\n\n')


def main():
    parser = argparse.ArgumentParser(description="Generate O-POPE GEMM data")
    parser.add_argument("-c", "--cfg", type=pathlib.Path, required=True,
                        help="Select param config file")
    args = parser.parse_args()

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    M, N, K = param["M"], param["N"], param["K"]
    fmt = param["mem_fmt"]
    if param["comp_fmt"] != "FP32" or fmt != "FP32":
        raise SystemExit("Only FP32->FP32 is supported for now")

    torch.random.manual_seed(1337)
    X = torch.rand(M, N).float()
    W = torch.rand(N, K).float()
    Y = torch.rand(M, K).float()
    Z = Y + torch.mm(X, W)
    if param.get("transpose_X", True):
        X = X.T.contiguous()

    out = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
        '#include "archi_opope.h"\n\n'
        f"#define M_SIZE {M}\n#define N_SIZE {N}\n#define K_SIZE {K}\n"
        f"#define COMP_FMT {param['comp_fmt']}\n#define MEM_FMT {fmt}\n"
        "#define OPOPE_GEMM_OP GEMM\n\n"
    )
    out += carray("opope_x_dram", to_words(X))
    out += carray("opope_w_dram", to_words(W))
    out += carray("opope_y_dram", to_words(Y))
    out += carray("opope_golden", to_words(Z))

    data_dir = pathlib.Path(__file__).parent.parent / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    (data_dir / f"data_{M}_{N}_{K}_{fmt.lower()}.h").write_text(out)


if __name__ == "__main__":
    main()
