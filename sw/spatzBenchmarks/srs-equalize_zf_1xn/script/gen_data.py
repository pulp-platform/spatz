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

BF16_INF = 0x7F80
BF16_NINF = 0xFF80
BF16_NAN = 0x7FC0
BF16_SIGN = 0x8000
BF16_SUBNORMAL = 0x0001  # smallest bf16 subnormal, its square underflows to 0

# Special noise_var values that can be written as strings in the hjson config.
NOISE_SPECIAL = {
    "inf": np.inf,
    "-inf": -np.inf,
    "nan": np.nan,
    "zero": 0.0,
    "-zero": -0.0,
    "neg": -0.1,
    "denorm": 1e-40,
}


def f32_to_bf16(x):
    """float32 array -> uint16 bf16 bits, round to nearest even (NaN stays NaN)."""
    x = np.asarray(x, dtype=np.float32)
    bits = x.view(np.uint32).astype(np.uint64)
    rounding = np.uint64(0x7FFF) + ((bits >> np.uint64(16)) & np.uint64(1))
    out = ((bits + rounding) >> np.uint64(16)).astype(np.uint16)
    out[np.isnan(x)] = BF16_NAN
    return out


def bf16_const(v):
    """Single float -> bf16 bits (uint16 scalar)."""
    return f32_to_bf16(np.array([v], dtype=np.float32))[0]


def array_to_cstr_hex16(a):
    out = "{"
    for el in np.asarray(a).flat:
        out += "0x{:04x}, ".format(int(el))
    return out[:-2] + "}"


def float_lit(x):
    """C float literal that is always valid (1 -> 1.0f, inf -> INFINITY, nan -> NAN).
    INFINITY and NAN come from <math.h>, which main.c includes before the data header."""
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
    name = "data_" + str(kwargs["nof_re"]) + "_" + str(kwargs["rx_ports"])
    if kwargs.get("tag"):
        name += "_" + str(kwargs["tag"])
    file = file_path / (name + ".h")
    emit_str += emit_equalize_zf_1xn_layer(**kwargs)
    with file.open("w") as f:
        f.write(emit_str)
    if verbose:
        print("Wrote", file)


def emit_equalize_zf_1xn_layer(**kwargs):
    nof_re = kwargs["nof_re"]
    rx_ports = kwargs["rx_ports"]
    tx_scaling = kwargs["tx_scaling"]
    n_cplx = rx_ports * nof_re * 2  # re, im interleaved, port after port

    layer_str = ""
    layer_str += f"const uint32_t RX_PORTS = {rx_ports};\n"
    layer_str += f"const uint32_t nof_re_dram = {nof_re};\n"
    layer_str += f"const float tx_scaling_dram = {float_lit(tx_scaling)};\n\n"
    layer_str += (
        f'uint16_t ch_symbols_dram [{n_cplx}] __attribute__((section(".data"))) = '
        + array_to_cstr_hex16(kwargs["ch_symbols"])
        + ";\n\n"
    )
    layer_str += (
        f'uint16_t ch_estimates_dram [{n_cplx}] __attribute__((section(".data"))) = '
        + array_to_cstr_hex16(kwargs["ch_estimates"])
        + ";\n\n"
    )
    layer_str += (
        f'float noise_var_est_dram [{rx_ports}] __attribute__((section(".data"))) = '
        + array_to_cstr_float(kwargs["noise_var_est"])
        + ";\n\n"
    )
    layer_str += (
        f"float symbols_ref_dram [{nof_re * 2}] = "
        + array_to_cstr_float(kwargs["symbols_ref"])
        + ";\n\n"
    )
    layer_str += (
        f"float nvars_ref_dram [{nof_re}] = "
        + array_to_cstr_float(kwargs["nvars_ref"])
        + ";\n\n\n"
    )
    return layer_str


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


def _random_sign(nsel):
    return (np.random.rand(nsel) < 0.5).astype(np.uint16) * np.uint16(BF16_SIGN)


def inject_special(re, im, probs):
    """Replace a random subset of channel estimates with abnormal values.

    Every RE gets at most one kind of corruption (one uniform draw split into
    consecutive intervals), so kinds do not overwrite each other.

    kind        effect on norm = re^2 + im^2            kernel must
    zero        0                                       mask out
    inf_re      inf (random sign)                       mask out
    inf_im      inf (random sign)                       mask out
    nan_re      NaN                                     mask out
    nan_im      NaN                                     mask out
    huge        1e20 -> square overflows to +inf        mask out
    denorm      1e-20 -> square is f32 subnormal        mask out
    tiny        bf16 subnormal -> square underflows 0   mask out
    """
    kinds = [(k, float(p)) for k, p in probs.items() if p and p > 0]
    counts = {}
    if not kinds:
        return re, im, counts
    total = sum(p for _, p in kinds)
    if total > 1.0:
        raise ValueError(f"sum of special probabilities is {total} > 1")

    n = re.size
    u = np.random.rand(n)
    lo = 0.0
    for kind, p in kinds:
        sel = (u >= lo) & (u < lo + p)
        lo += p
        k = int(sel.sum())
        counts[kind] = k
        if k == 0:
            continue
        if kind == "zero":
            re[sel] = 0
            im[sel] = 0
        elif kind == "inf_re":
            re[sel] = np.uint16(BF16_INF) | _random_sign(k)
        elif kind == "inf_im":
            im[sel] = np.uint16(BF16_INF) | _random_sign(k)
        elif kind == "nan_re":
            re[sel] = BF16_NAN
        elif kind == "nan_im":
            im[sel] = BF16_NAN
        elif kind == "huge":
            re[sel] = bf16_const(1e20)
            im[sel] = bf16_const(1e20)
        elif kind == "denorm":
            re[sel] = bf16_const(1e-20)
            im[sel] = bf16_const(1e-20)
        elif kind == "tiny":
            re[sel] = BF16_SUBNORMAL
            im[sel] = BF16_SUBNORMAL
        else:
            raise ValueError(f"unknown special kind '{kind}'")
    return re, im, counts


def gen_channel(n, probs=None):
    """Channel estimates: magnitude in [0.1, 1], random phase.

    Optionally forces abnormal estimates (see inject_special) to exercise the
    mask path in the kernel.
    """
    mag = np.random.uniform(0.1, 1.0, n).astype(np.float32)
    phase = np.random.uniform(0.0, 2.0 * np.pi, n).astype(np.float32)
    re = f32_to_bf16(mag * np.cos(phase))
    im = f32_to_bf16(mag * np.sin(phase))
    return inject_special(re, im, probs or {})


def bf16_to_f32(b):
    return (np.asarray(b, dtype=np.uint32) << np.uint32(16)).view(np.float32)


def is_normal(x):
    """Same integer test as the kernel: (bits - 0x00800000) <u 0x7F000000.

    True only for POSITIVE normal floats. Zero, subnormals, inf, NaN and all
    negative values (including negative normals) give False.
    NOTE: std::isnormal() in srsRAN also accepts negative normals; for the
    quantities tested here (norm >= 0, tx_scaling > 0) that makes no difference,
    for a negative noise_var it does (the kernel skips the port).
    Always returns an array (atleast_1d) so uint32 wrap-around never warns.
    """
    bits = np.atleast_1d(np.asarray(x, dtype=np.float32)).view(np.uint32)
    return (bits - np.uint32(0x00800000)) < np.uint32(0x7F000000)


def port_norm_and_valid(h_port, sig):
    """Channel norm of one port and the per-RE mask the kernel uses.

    Port is skipped entirely when noise_var is not a positive normal float.
    """
    f32 = np.float32
    hr, hi = h_port[:, 0], h_port[:, 1]
    with np.errstate(all="ignore"):
        norm = (hr * hr + hi * hi).astype(f32)
    sig_ok = bool(is_normal(sig)[0])
    return norm, is_normal(norm) & sig_ok


def poison_symbols(ch_symbols, ch_estimates, noise_var_est, nof_re, rx_ports, prob):
    """Put NaN/Inf into Rx symbols, but only where the kernel must NOT use them
    (masked-out RE or skipped port). If the masked MACs are really masked, the
    output stays clean. NaN/Inf in symbols at a valid RE would propagate in
    srsRAN as well, so they are never placed there."""
    y = np.asarray(ch_symbols, dtype=np.uint16).reshape(rx_ports, nof_re, 2).copy()
    h = bf16_to_f32(ch_estimates).reshape(rx_ports, nof_re, 2)
    vals = np.array([BF16_NAN, BF16_INF, BF16_NINF], dtype=np.uint16)
    poisoned = 0
    for p in range(rx_ports):
        _, valid = port_norm_and_valid(h[p], noise_var_est[p])
        pick = (~valid) & (np.random.rand(nof_re) < prob)
        k = int(pick.sum())
        poisoned += k
        if k:
            y[p, pick, 0] = vals[np.random.randint(0, len(vals), k)]
            y[p, pick, 1] = vals[np.random.randint(0, len(vals), k)]
    return y.reshape(-1), poisoned


def golden(ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, rx_ports):
    """float32 reference of equalize_zf_1xn (same logic as srsRAN, with the
    kernel's integer isnormal test). Inputs: interleaved bf16 bits, port after
    port. Returns interleaved symbols (2*nof_re) and nvars (nof_re)."""
    f32 = np.float32
    y = bf16_to_f32(ch_symbols).reshape(rx_ports, nof_re, 2)
    h = bf16_to_f32(ch_estimates).reshape(rx_ports, nof_re, 2)

    ch_mod_sq = np.zeros(nof_re, dtype=f32)
    nvar_acc = np.zeros(nof_re, dtype=f32)
    re_out = np.zeros(nof_re, dtype=f32)
    im_out = np.zeros(nof_re, dtype=f32)

    with np.errstate(all="ignore"):
        for p in range(rx_ports):
            sig = f32(noise_var_est[p])
            hr, hi = h[p, :, 0], h[p, :, 1]
            yr, yi = y[p, :, 0], y[p, :, 1]
            norm, m = port_norm_and_valid(h[p], sig)
            ch_mod_sq = np.where(m, ch_mod_sq + norm, ch_mod_sq).astype(f32)
            nvar_acc = np.where(m, nvar_acc + norm * sig, nvar_acc).astype(f32)
            re_out = np.where(m, re_out + (yr * hr + yi * hi), re_out).astype(f32)
            im_out = np.where(m, im_out + (yi * hr - yr * hi), im_out).astype(f32)

        d = (f32(tx_scaling) * ch_mod_sq).astype(f32)
        ok = is_normal(d) & is_normal(nvar_acc)
        rcp = (f32(1.0) / d).astype(f32)
        sym_re = np.where(ok, re_out * rcp, f32(0.0)).astype(f32)
        sym_im = np.where(ok, im_out * rcp, f32(0.0)).astype(f32)
        nvar = np.where(ok, (nvar_acc * rcp) * rcp, f32(np.inf)).astype(f32)

    symbols = np.empty(2 * nof_re, dtype=f32)
    symbols[0::2] = sym_re
    symbols[1::2] = sym_im

    # The reference must never contain NaN: symbols are finite, nvar is finite or +inf.
    assert not np.isnan(symbols).any(), "NaN in reference symbols"
    assert not np.isnan(nvar).any(), "NaN in reference nvars"
    assert not np.isinf(symbols).any(), "Inf in reference symbols"
    return symbols, nvar, ok


def parse_noise(v):
    if isinstance(v, str):
        return NOISE_SPECIAL[v] if v in NOISE_SPECIAL else float(v)
    return float(v)


def build_noise(param, rx_ports):
    """noise_var: scalar, or list (cycled over ports). Entries may be numbers or
    the strings in NOISE_SPECIAL (inf, -inf, nan, zero, -zero, neg, denorm)."""
    nv = param.get("noise_var", 0.1)
    if not isinstance(nv, (list, tuple)):
        nv = [nv]
    vals = [parse_noise(v) for v in nv]
    return np.array([vals[p % len(vals)] for p in range(rx_ports)], dtype=np.float32)


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

    nof_re = param["nof_re"]
    rx_ports = param.get("rx_ports", 1)
    tx_scaling = param.get("tx_scaling", 1.0)
    poison_prob = param.get("poison_prob", 0.0)
    tag = param.get("tag", "")

    # Special channel estimates. New style: "special": {"nan_re": 0.02, ...}.
    # Old keys zero_prob / inf_prob / nan_prob still work.
    special = dict(param.get("special", {}))
    for old, new in (("zero_prob", "zero"), ("inf_prob", "inf_re"), ("nan_prob", "nan_re")):
        if param.get(old, 0.0) > 0:
            special.setdefault(new, param[old])

    n = rx_ports * nof_re
    sym_re, sym_im = gen_symbols(n)
    est_re, est_im, counts = gen_channel(n, special)

    ch_symbols = interleave(sym_re, sym_im)
    ch_estimates = interleave(est_re, est_im)
    noise_var_est = build_noise(param, rx_ports)

    poisoned = 0
    if poison_prob > 0:
        ch_symbols, poisoned = poison_symbols(
            ch_symbols, ch_estimates, noise_var_est, nof_re, rx_ports, poison_prob
        )

    symbols_ref, nvars_ref, ok = golden(
        ch_symbols, ch_estimates, noise_var_est, tx_scaling, nof_re, rx_ports
    )

    if verbose:
        print(f"nof_re={nof_re} rx_ports={rx_ports} tx_scaling={tx_scaling}")
        print("noise_var_est:", [float_lit(v) for v in noise_var_est])
        h = bf16_to_f32(ch_estimates).reshape(rx_ports, nof_re, 2)
        for p in range(rx_ports):
            _, valid = port_norm_and_valid(h[p], noise_var_est[p])
            print(f"  port {p}: valid RE {int(valid.sum())}/{nof_re}")
        print("injected special channel values:", counts or "none")
        print("poisoned symbol RE:", poisoned)
        print(f"RE with normal output: {int(ok.sum())}/{nof_re}, "
              f"filtered (symbols=0, nvar=inf): {int((~ok).sum())}/{nof_re}")

    kwargs = {
        "nof_re": nof_re,
        "rx_ports": rx_ports,
        "tx_scaling": tx_scaling,
        "tag": tag,
        "ch_symbols": ch_symbols,
        "ch_estimates": ch_estimates,
        "noise_var_est": noise_var_est,
        "symbols_ref": symbols_ref,
        "nvars_ref": nvars_ref,
    }
    emit_header_file(**kwargs)


if __name__ == "__main__":
    main()