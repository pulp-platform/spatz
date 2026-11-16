#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Ihor Sachok <ihor.sachok@unibo.it>
#
# Data generator for equalize_zf_2xn (2 layers, N rx ports).
#
# Memory layout (matches the kernel and srsRAN ch_est_list {re, rx_port, layer}):
#   ch_symbols   : [port][re][re/im]           bf16, rx_ports * nof_re * 2
#   ch_estimates : [layer][port][re][re/im]    bf16, 2 * rx_ports * nof_re * 2
#   eq_symbols   : [re][layer][re/im]          f32,  4 * nof_re
#   noise_vars   : [re][layer]                 f32,  2 * nof_re
#   noise_var_est: scalar f32 (same for all ports)

import numpy as np
import argparse
import pathlib
import hjson

np.random.seed(42)
verbose = False

NOF_LAYERS = 2
BF16_INF = 0x7F80
BF16_NAN = 0x7FC0


# --------------------------------------------------------------------------- #
# bf16 helpers
# --------------------------------------------------------------------------- #
def f32_to_bf16(x):
    """float32 array -> uint16 bf16 bits, round to nearest even (NaN stays NaN)."""
    x = np.asarray(x, dtype=np.float32)
    bits = x.view(np.uint32).astype(np.uint64)
    rounding = np.uint64(0x7FFF) + ((bits >> np.uint64(16)) & np.uint64(1))
    out = ((bits + rounding) >> np.uint64(16)).astype(np.uint16)
    out[np.isnan(x)] = BF16_NAN
    return out


def bf16_to_f32(b):
    return (np.asarray(b, dtype=np.uint32) << np.uint32(16)).view(np.float32)


def is_pos_normal(x):
    """Same integer test as the kernel: (bits - 0x00800000) <u 0x7F000000.
    True only for positive normal floats."""
    bits = np.asarray(x, dtype=np.float32).view(np.uint32)
    return (bits - np.uint32(0x00800000)) < np.uint32(0x7F000000)


def is_normal(x):
    """std::isnormal: normal, any sign (kernel: vfclass bits 1 | 6)."""
    return is_pos_normal(np.abs(np.asarray(x, dtype=np.float32)))


# --------------------------------------------------------------------------- #
# C emission helpers
# --------------------------------------------------------------------------- #
def array_to_cstr_hex16(a):
    out = "{"
    for el in np.asarray(a).flat:
        out += "0x{:04x}, ".format(int(el))
    return out[:-2] + "}"


def float_lit(x):
    """C float literal that is always valid (1 -> 1.0f)."""
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
    file = file_path / (
        "data_" + str(kwargs["nof_re"]) + "_" + str(kwargs["rx_ports"]) + ".h"
    )
    emit_str += emit_equalize_zf_2xn_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)
    if verbose:
        print("Wrote", file)


def emit_equalize_zf_2xn_layer(**kwargs):
    nof_re = kwargs["nof_re"]
    rx_ports = kwargs["rx_ports"]
    n_sym = rx_ports * nof_re * 2               # u16 elements of ch_symbols
    n_est = NOF_LAYERS * rx_ports * nof_re * 2  # u16 elements of ch_estimates

    s = ""
    s += f"const uint32_t RX_PORTS = {rx_ports};\n"
    s += f"const uint32_t nof_re_dram = {nof_re};\n"
    s += f"const float tx_scaling_dram = {float_lit(kwargs['tx_scaling'])};\n"
    s += f"const float noise_var_est_dram = {float_lit(kwargs['noise_var_est'])};\n\n"
    s += (
        f'uint16_t ch_symbols_dram [{n_sym}] __attribute__((section(".data"))) = '
        + array_to_cstr_hex16(kwargs["ch_symbols"])
        + ";\n\n"
    )
    s += (
        f'uint16_t ch_estimates_dram [{n_est}] __attribute__((section(".data"))) = '
        + array_to_cstr_hex16(kwargs["ch_estimates"])
        + ";\n\n"
    )
    s += (
        f"float symbols_ref_dram [{4 * nof_re}] = "
        + array_to_cstr_float(kwargs["symbols_ref"])
        + ";\n\n"
    )
    s += (
        f"float nvars_ref_dram [{2 * nof_re}] = "
        + array_to_cstr_float(kwargs["nvars_ref"])
        + ";\n\n\n"
    )
    return s


# --------------------------------------------------------------------------- #
# Input generation
# --------------------------------------------------------------------------- #
def interleave(re, im):
    out = np.empty(re.size * 2, dtype=np.uint16)
    out[0::2] = re
    out[1::2] = im
    return out


def gen_symbols(n):
    """Rx symbols, uniform in [-1, 1] for both parts."""
    re = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    im = np.random.uniform(-1.0, 1.0, n).astype(np.float32)
    return f32_to_bf16(re), f32_to_bf16(im)


def gen_channel(n, zero_prob=0.0, inf_prob=0.0, nan_prob=0.0):
    """Channel estimates: magnitude in [0.1, 1], random phase.

    Optionally forces abnormal estimates to exercise the mask path:
    zero, inf and NaN.
    """
    mag = np.random.uniform(0.1, 1.0, n).astype(np.float32)
    phase = np.random.uniform(0.0, 2.0 * np.pi, n).astype(np.float32)
    re = f32_to_bf16(mag * np.cos(phase))
    im = f32_to_bf16(mag * np.sin(phase))

    if zero_prob > 0:
        m = np.random.rand(n) < zero_prob
        re[m] = 0
        im[m] = 0
    if inf_prob > 0:
        m = np.random.rand(n) < inf_prob
        re[m] = BF16_INF
    if nan_prob > 0:
        m = np.random.rand(n) < nan_prob
        re[m] = BF16_NAN
    return re, im


# --------------------------------------------------------------------------- #
# Golden model (float32, same operation order as the kernel)
# --------------------------------------------------------------------------- #
def golden(ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, rx_ports):
    f32 = np.float32
    nv = f32(noise_var_est)
    tx = f32(tx_scaling)

    # Bad noise variance -> whole output is zeros / inf (early exit in kernel).
    if not bool(is_pos_normal(nv)):
        return (np.zeros(4 * nof_re, dtype=f32),
                np.full(2 * nof_re, np.inf, dtype=f32))

    y = bf16_to_f32(ch_symbols).reshape(rx_ports, nof_re, 2)
    h = bf16_to_f32(ch_estimates).reshape(NOF_LAYERS, rx_ports, nof_re, 2)

    z = lambda: np.zeros(nof_re, dtype=f32)
    n0, n1 = z(), z()          # ||h0||^2, ||h1||^2
    xr, xi = z(), z()          # xi = sum conj(h0) * h1
    m0r, m0i = z(), z()        # mf0 = sum conj(h0) * y
    m1r, m1i = z(), z()        # mf1 = sum conj(h1) * y

    with np.errstate(all="ignore"):
        for p in range(rx_ports):
            ar, ai = h[0, p, :, 0], h[0, p, :, 1]
            br, bi = h[1, p, :, 0], h[1, p, :, 1]
            yr, yi = y[p, :, 0], y[p, :, 1]

            n0 = n0 + ar * ar; n0 = n0 + ai * ai
            n1 = n1 + br * br; n1 = n1 + bi * bi

            xr = xr + ar * br; xr = xr + ai * bi
            xi = xi + ar * bi; xi = xi - ai * br

            m0r = m0r + ar * yr; m0r = m0r + ai * yi
            m0i = m0i + ar * yi; m0i = m0i - ai * yr

            m1r = m1r + br * yr; m1r = m1r + bi * yi
            m1i = m1i + br * yi; m1i = m1i - bi * yr

        xms = xr * xr; xms = xms + xi * xi
        det = n0 * n1 - xms
        d_pinv = tx * det
        d_nvars = tx * d_pinv
        ok = is_normal(d_pinv)

        rcp = f32(1.0) / d_pinv
        rcpn = f32(1.0) / d_nvars

        t = n1 * m0r; t = t - xr * m1r; t = t + xi * m1i; s0r = t * rcp
        t = n1 * m0i; t = t - xr * m1i; t = t - xi * m1r; s0i = t * rcp
        t = n0 * m1r; t = t - xr * m0r; t = t - xi * m0i; s1r = t * rcp
        t = n0 * m1i; t = t - xr * m0i; t = t + xi * m0r; s1i = t * rcp

        v0 = (nv * n1) * rcpn
        v1 = (nv * n0) * rcpn

    symbols = np.empty(4 * nof_re, dtype=f32)
    for k, s in enumerate((s0r, s0i, s1r, s1i)):
        symbols[k::4] = np.where(ok, s, f32(0.0))

    nvars = np.empty(2 * nof_re, dtype=f32)
    nvars[0::2] = np.where(ok, v0, f32(np.inf))
    nvars[1::2] = np.where(ok, v1, f32(np.inf))
    return symbols, nvars, ok


# --------------------------------------------------------------------------- #
def parse_noise(v):
    """Allow 'nan' / 'inf' strings in the config for early-exit tests."""
    if isinstance(v, str):
        return np.float32(float(v))
    return np.float32(v)


def main():
    parser = argparse.ArgumentParser(description="Generate data for equalize_zf_2xn")
    parser.add_argument(
        "-c", "--cfg", type=pathlib.Path, required=True,
        help="Select param config file kernel",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Set verbose")
    args = parser.parse_args()

    global verbose
    verbose = args.verbose

    with args.cfg.open() as f:
        param = hjson.loads(f.read())

    nof_re = param["nof_re"]
    rx_ports = param.get("rx_ports", 2)
    tx_scaling = param.get("tx_scaling", 1.0)
    noise_var = parse_noise(param.get("noise_var", 0.1))
    zero_prob = param.get("zero_prob", 0.0)
    inf_prob = param.get("inf_prob", 0.0)
    nan_prob = param.get("nan_prob", 0.0)

    if rx_ports < NOF_LAYERS:
        raise ValueError(f"rx_ports ({rx_ports}) must be >= {NOF_LAYERS} for 2xN ZF")

    n_sym = rx_ports * nof_re
    n_est = NOF_LAYERS * rx_ports * nof_re

    sym_re, sym_im = gen_symbols(n_sym)
    est_re, est_im = gen_channel(n_est, zero_prob, inf_prob, nan_prob)

    ch_symbols = interleave(sym_re, sym_im)
    ch_estimates = interleave(est_re, est_im)   # layer-major: [L0 ports..][L1 ports..]

    symbols_ref, nvars_ref, ok = golden(
        ch_symbols, ch_estimates, noise_var, tx_scaling, nof_re, rx_ports
    )

    if verbose:
        n_masked = int(np.sum(np.isinf(nvars_ref[0::2])))
        print(f"nof_re={nof_re} rx_ports={rx_ports} masked RE={n_masked}")
        bad = np.flatnonzero(~ok)
        print("masked RE (golden: zeros/inf):", bad.tolist())

    emit_header_file(
        nof_re=nof_re,
        rx_ports=rx_ports,
        tx_scaling=tx_scaling,
        noise_var_est=noise_var,
        ch_symbols=ch_symbols,
        ch_estimates=ch_estimates,
        symbols_ref=symbols_ref,
        nvars_ref=nvars_ref,
    )


if __name__ == "__main__":
    main()