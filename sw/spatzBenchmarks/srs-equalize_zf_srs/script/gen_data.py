#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Ihor Sachok <ihor.sachok@unibo.it>
#
# Data generator for the tiled equalize_zf_2xn test (2 layers, N rx ports).
#
# Memory layout (matches the kernel and srsRAN ch_est_list {re, rx_port, layer}):
#   ch_symbols   : [port][re][re/im]           bf16, rx_ports * nof_re * 2
#   ch_estimates : [layer][port][re][re/im]    bf16, 2 * rx_ports * nof_re * 2
#   eq_symbols   : [re][layer][re/im]          f32,  4 * nof_re
#   noise_vars   : [re][layer]                 f32,  2 * nof_re
#   noise_var_est: scalar f32 (same for all ports)
#
# Config keys (hjson):
#   nof_re        : total number of REs            (or nof_prb)
#   nof_prb       : number of PRBs, nof_re = nof_prb * 12 * nof_symbols
#   nof_symbols   : OFDM symbols per slot          (default 14)
#   rx_ports      : number of receive ports        (default 2, must be >= 2)
#   tile          : REs per tile                   (default: auto from TCDM budget)
#   tcdm_kib      : TCDM size used for auto tile   (default 256)
#   reserve_kib   : TCDM reserved for stack/runtime(default 16)
#   double_buffer : size auto tile for ping/pong   (default true)
#   tx_scaling    : tx gain scaling                (default 1.0)
#   noise_var     : noise variance, may be "nan"/"inf" (default 0.1)
#   zero_prob, inf_prob, nan_prob : abnormal channel estimate probabilities

import numpy as np
import argparse
import pathlib
import hjson

np.random.seed(42)
verbose = False

NOF_LAYERS = 2
NRE = 12
VL_E16_M1 = 32          # VLEN=512, e16, m1
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


def fma(a, b, c):
    """Single-rounding a * b + c in float32 (emulates vfmacc and friends).
    The f32 x f32 product is exact in f64; the final add rounds twice
    (f64 then f32), which differs from a true FMA only in rare ties."""
    f64 = np.float64
    return (np.asarray(a, f64) * np.asarray(b, f64) + np.asarray(c, f64)).astype(np.float32)


# --------------------------------------------------------------------------- #
# C emission helpers
# --------------------------------------------------------------------------- #
def array_to_cstr_hex16(a):
    return "{" + ", ".join("0x{:04x}".format(int(el)) for el in np.asarray(a).flat) + "}"


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
    return "{" + ", ".join(float_lit(el) for el in np.asarray(a, dtype=np.float32).flat) + "}"


def emit_header_file(**kwargs):
    file_path = pathlib.Path(__file__).parent.parent / "data"
    file_path.mkdir(parents=True, exist_ok=True)
    header = (
        "// Copyright 2026 ETH Zurich and University of Bologna.\n"
        "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        "// SPDX-License-Identifier: Apache-2.0\n\n"
        "// This file was generated automatically.\n\n"
    )
    file = file_path / (
        "data_" + str(kwargs["nof_re"]) + "_" + str(kwargs["rx_ports"]) + ".h"
    )
    with file.open("w") as f:
        f.write(header)
        f.write(emit_equalize_zf_2xn_layer(**kwargs))
    if verbose:
        print("Wrote", file)


def emit_equalize_zf_2xn_layer(**kwargs):
    nof_re = kwargs["nof_re"]
    rx_ports = kwargs["rx_ports"]
    n_sym = rx_ports * nof_re * 2               # u16 elements of ch_symbols
    n_est = NOF_LAYERS * rx_ports * nof_re * 2  # u16 elements of ch_estimates
    dram = '__attribute__((section(".data"), aligned(64)))'

    parts = []
    parts.append(f"const uint32_t RX_PORTS = {rx_ports};\n")
    parts.append(f"const uint32_t nof_re_dram = {nof_re};\n")
    parts.append(f"const uint32_t tile_dram = {kwargs['tile']};\n")
    parts.append(f"const float tx_scaling_dram = {float_lit(kwargs['tx_scaling'])};\n")
    parts.append(f"const float noise_var_est_dram = {float_lit(kwargs['noise_var_est'])};\n\n")

    # Inputs
    parts.append(f"uint16_t ch_symbols_dram [{n_sym}] {dram} = ")
    parts.append(array_to_cstr_hex16(kwargs["ch_symbols"]))
    parts.append(";\n\n")
    parts.append(f"uint16_t ch_estimates_dram [{n_est}] {dram} = ")
    parts.append(array_to_cstr_hex16(kwargs["ch_estimates"]))
    parts.append(";\n\n")

    # Golden references
    parts.append(f"float symbols_ref_dram [{4 * nof_re}] {dram} = ")
    parts.append(array_to_cstr_float(kwargs["symbols_ref"]))
    parts.append(";\n\n")
    parts.append(f"float nvars_ref_dram [{2 * nof_re}] {dram} = ")
    parts.append(array_to_cstr_float(kwargs["nvars_ref"]))
    parts.append(";\n\n")

    # Output buffers written back by DMA (no initializer, zero-filled in .data)
    parts.append(f"float eq_symbols_dram [{4 * nof_re}] {dram};\n")
    parts.append(f"float nvars_out_dram [{2 * nof_re}] {dram};\n\n")
    return "".join(parts)


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
# Golden model (float32, same operation order and fusion as the kernel)
# --------------------------------------------------------------------------- #
def golden(ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, rx_ports):
    f32 = np.float32
    nv = f32(noise_var_est)
    tx = f32(tx_scaling)

    # Bad noise variance -> whole output is zeros / inf (early exit in kernel).
    if not bool(is_pos_normal(nv)):
        return (np.zeros(4 * nof_re, dtype=f32),
                np.full(2 * nof_re, np.inf, dtype=f32),
                np.zeros(nof_re, dtype=bool))

    y = bf16_to_f32(ch_symbols).reshape(rx_ports, nof_re, 2)
    h = bf16_to_f32(ch_estimates).reshape(NOF_LAYERS, rx_ports, nof_re, 2)

    z = lambda: np.zeros(nof_re, dtype=f32)
    n0, n1 = z(), z()          # ||h0||^2, ||h1||^2
    xr, xi = z(), z()          # xi = sum conj(h0) * h1
    m0r, m0i = z(), z()        # mf0 = sum conj(h0) * y
    m1r, m1i = z(), z()        # mf1 = sum conj(h1) * y

    with np.errstate(all="ignore"):
        # Widening bf16 x bf16 products are exact in f32, so
        # vfwmacc/vfwnmsac == separate multiply + add here.
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

        # xi_mod_sq: vfmul + vfmacc (fused)
        xms = xr * xr
        xms = fma(xi, xi, xms)
        # det: vfmsac (fused) n0 * n1 - xms
        det = fma(n0, n1, -xms)
        # d_pinv: vfmul.vf
        d_pinv = det * tx
        ok = is_normal(d_pinv)

        rcp = f32(1.0) / d_pinv

        # Zero forcing: vfmul, then fused vfnmsac / vfmacc
        t = n1 * m0r; t = fma(-xr, m1r, t); t = fma(xi, m1i, t);  s0r = t * rcp
        t = n1 * m0i; t = fma(-xr, m1i, t); t = fma(-xi, m1r, t); s0i = t * rcp
        t = n0 * m1r; t = fma(-xr, m0r, t); t = fma(-xi, m0i, t); s1r = t * rcp
        t = n0 * m1i; t = fma(-xr, m0i, t); t = fma(xi, m0r, t);  s1i = t * rcp

        # Noise: n * (nv / tx) * (1 / d_pinv), as in the kernel and srsRAN SIMD path
        nv_scaled = f32(nv / tx)
        v0 = (n1 * nv_scaled) * rcp
        v1 = (n0 * nv_scaled) * rcp

    symbols = np.empty(4 * nof_re, dtype=f32)
    for k, s in enumerate((s0r, s0i, s1r, s1i)):
        symbols[k::4] = np.where(ok, s, f32(0.0))

    nvars = np.empty(2 * nof_re, dtype=f32)
    nvars[0::2] = np.where(ok, v0, f32(np.inf))
    nvars[1::2] = np.where(ok, v1, f32(np.inf))
    return symbols, nvars, ok


# --------------------------------------------------------------------------- #
# Tile sizing
# --------------------------------------------------------------------------- #
def bytes_per_re(rx_ports):
    """TCDM bytes for one RE of one buffer set (inputs + outputs)."""
    in_bytes = 4 * rx_ports + 4 * NOF_LAYERS * rx_ports   # symbols + estimates, bf16 complex
    out_bytes = 8 * NOF_LAYERS + 4 * NOF_LAYERS           # eq symbols f32 complex + nvars f32
    return in_bytes + out_bytes


def auto_tile(nof_re, rx_ports, tcdm_kib, reserve_kib, double_buffer):
    budget = (tcdm_kib - reserve_kib) * 1024
    per_re = bytes_per_re(rx_ports) * (2 if double_buffer else 1)
    tile = (budget // per_re) // VL_E16_M1 * VL_E16_M1
    if tile <= 0:
        raise ValueError("TCDM budget too small for a single tile")
    # No point in a tile larger than the problem (rounded up to vl).
    max_tile = (nof_re + VL_E16_M1 - 1) // VL_E16_M1 * VL_E16_M1
    return min(tile, max_tile)


# --------------------------------------------------------------------------- #
def parse_noise(v):
    """Allow 'nan' / 'inf' strings in the config for early-exit tests."""
    if isinstance(v, str):
        return np.float32(float(v))
    return np.float32(v)


def main():
    parser = argparse.ArgumentParser(description="Generate data for tiled equalize_zf_2xn")
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

    nof_symbols = param.get("nof_symbols", 14)
    if "nof_prb" in param:
        nof_re = param["nof_prb"] * NRE * nof_symbols
    else:
        nof_re = param["nof_re"]
    rx_ports = param.get("rx_ports", 2)
    tx_scaling = param.get("tx_scaling", 1.0)
    noise_var = parse_noise(param.get("noise_var", 0.1))
    zero_prob = param.get("zero_prob", 0.0)
    inf_prob = param.get("inf_prob", 0.0)
    nan_prob = param.get("nan_prob", 0.0)

    if nof_re <= 0:
        raise ValueError("nof_re must be > 0")
    if rx_ports < NOF_LAYERS:
        raise ValueError(f"rx_ports ({rx_ports}) must be >= {NOF_LAYERS} for 2xN ZF")

    if "tile" in param:
        tile = int(param["tile"])
    else:
        tile = auto_tile(nof_re, rx_ports,
                         param.get("tcdm_kib", 256),
                         param.get("reserve_kib", 16),
                         param.get("double_buffer", True))
    if tile <= 0:
        raise ValueError("tile must be > 0")
    if tile % VL_E16_M1 != 0:
        print(f"warning: tile={tile} is not a multiple of vl={VL_E16_M1}")

    n_tiles = (nof_re + tile - 1) // tile
    tail = nof_re - (n_tiles - 1) * tile

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
        per_set = bytes_per_re(rx_ports) * tile
        print(f"nof_re={nof_re} rx_ports={rx_ports} tile={tile} "
              f"n_tiles={n_tiles} tail={tail}")
        print(f"TCDM per buffer set: {per_set} B ({per_set / 1024:.1f} KiB), "
              f"x2 = {2 * per_set / 1024:.1f} KiB")
        bad = np.flatnonzero(~ok)
        print(f"masked RE: {bad.size}", bad[:50].tolist(),
              "..." if bad.size > 50 else "")

    emit_header_file(
        nof_re=nof_re,
        rx_ports=rx_ports,
        tile=tile,
        tx_scaling=tx_scaling,
        noise_var_est=noise_var,
        ch_symbols=ch_symbols,
        ch_estimates=ch_estimates,
        symbols_ref=symbols_ref,
        nvars_ref=nvars_ref,
    )


if __name__ == "__main__":
    main()