"""Deterministic test data and FP16 pair layouts for VME applications."""
import numpy as np


def words(values):
    return np.ascontiguousarray(np.asarray(values, dtype=np.float32)).view(np.uint32).ravel()


def halves(values):
    return np.ascontiguousarray(np.asarray(values, dtype=np.float16)).view(np.uint16).ravel()


def pack_pairs(bits):
    bits = np.asarray(bits, dtype=np.uint32)
    return bits[0::2, :] | (bits[1::2, :] << 16)


def carray(name, values, ctype="uint32_t"):
    values = np.asarray(values).ravel()
    body = ", ".join(f"0x{int(v):08x}" if ctype == "uint32_t" else str(int(v)) for v in values)
    return f'static {ctype} {name}[{len(values)}] __attribute__((section(".data"))) = {{{body}}};\n\n'


def check_footprint(buffers):
    total = sum((size + 255) // 256 * 256 for size in buffers.values())
    if total > 120 * 1024:
        raise ValueError(f"TCDM allocation {total} exceeds 120 KiB")
    return total


def rnd(rng, shape, scale=1.0):
    return (rng.standard_normal(shape) * scale).astype(np.float32)


def wgt(rng, cin, cout):
    return (rng.standard_normal((cin, cout)) / np.sqrt(cin)).astype(np.float32)


def ln(x, gamma, beta):
    x = x.astype(np.float64)
    mean = x.mean(axis=0, keepdims=True)
    var = ((x - mean) ** 2).mean(axis=0, keepdims=True)
    return (x - mean) / np.sqrt(var + 1e-5) * gamma[:, None].astype(np.float64) + beta[:, None].astype(np.float64)


def gelu(x):
    return 0.5 * x * (1 + np.tanh(np.sqrt(2 / np.pi) * (x + 0.044715 * x ** 3)))


def lin(w, x, bias):
    return w.astype(np.float64).T @ np.asarray(x, np.float64) + bias.astype(np.float64)[:, None]


def f32(x):
    return np.asarray(x, np.float64).astype(np.float32)


def h(x):
    return np.asarray(x).astype(np.float16).astype(np.float64)


def pk(x):
    return pack_pairs(halves(x).reshape(x.shape))


def p_attn(app_dir, tag, p):
    d, tokens = p["D"], p["T"]
    rng = np.random.default_rng(24)
    q = rnd(rng, (d, tokens)).astype(np.float16)
    k = rnd(rng, (d, tokens)).astype(np.float16)
    v = rnd(rng, (tokens, d)).astype(np.float16)
    score = (h(k).T @ h(q)) * (1 / np.sqrt(d))
    score = np.where(np.arange(tokens)[:, None] > np.arange(tokens)[None, :], -np.inf, score)
    exp = np.exp(score - score.max(axis=0, keepdims=True))
    prob = h(exp / exp.sum(axis=0, keepdims=True))
    output = f32(h(v).T @ prob)
    arrays = dict(qpk=pk(q), kpk=pk(k), vpk=pk(v), golden=output)
    footprint = dict(qpk=d // 2 * tokens * 4, kpk=d // 2 * tokens * 4,
                     vpk=tokens // 2 * d * 4, s=tokens * tokens * 4,
                     ppk=tokens // 2 * tokens * 4, o=d * tokens * 4)
    emit(app_dir, tag, dict(D_SIZE=d, T_SIZE=tokens,
                            SCALE_F=f"{1 / np.sqrt(d):.10f}f"), arrays, footprint)


def p_mlp(app_dir, tag, p):
    channels, tokens, hidden = p["C"], p["T"], p["HID"]
    rng = np.random.default_rng(25)
    x = rnd(rng, (channels, tokens))
    gamma = 1 + 0.1 * rnd(rng, channels)
    beta = 0.1 * rnd(rng, channels)
    w1 = wgt(rng, channels, hidden).astype(np.float16)
    b1 = 0.1 * rnd(rng, hidden)
    w2 = wgt(rng, hidden, channels).astype(np.float16)
    b2 = 0.1 * rnd(rng, channels)
    # Match optimized O-POPE: fold affine LN into the first linear layer.
    w1_64 = w1.astype(np.float64)
    w1 = (w1_64 * gamma.astype(np.float64)[:, None]).astype(np.float16)
    b1 = f32(b1.astype(np.float64) + w1_64.T @ beta.astype(np.float64))
    one = np.ones(channels, np.float32)
    zero = np.zeros(channels, np.float32)
    # Latest O-POPE treats the second bias as an FP16 GEMM input.
    b2 = b2.astype(np.float16).astype(np.float32)
    output = f32(x + lin(w2, h(gelu(lin(w1, h(ln(x, one, zero)), b1))), b2))
    # Match the latest O-POPE second GEMM: bias2 is a final FP16 reduction
    # pair {bias2, 0}, while the FP32 residual remains the initial C tensor.
    w2_bias = np.concatenate((w2, b2.astype(np.float16)[None, :],
                              np.zeros((1, channels), np.float16)), axis=0)
    arrays = dict(x=x, w1=pk(w1), hinit=np.repeat(b1[:, None], tokens, axis=1),
                  w2=pk(w2_bias), golden=output)
    footprint = dict(z=channels * tokens * 4, lnpk=channels // 2 * tokens * 4,
                     w1=channels // 2 * hidden * 4, h=hidden * tokens * 4,
                     hpk=(hidden // 2 + 1) * tokens * 4,
                     w2=(hidden // 2 + 1) * channels * 4)
    emit(app_dir, tag, dict(C_SIZE=channels, T_SIZE=tokens, HID_SIZE=hidden), arrays, footprint)


def emit(*args):
    raise RuntimeError("application must provide its own emitter")
