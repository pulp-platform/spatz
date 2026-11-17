#!/usr/bin/env python3
"""Generate the deterministic Tq16/T64 direct-VRF FlashAttention data."""

import argparse
import pathlib
import sys

import numpy as np

sys.path.append(str(pathlib.Path(__file__).resolve().parents[3] / "script"))
import vme_data as vd  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    tq, tokens, d, dv = 16, 64, 64, 16
    rng = np.random.default_rng(1337)
    q = rng.uniform(-1, 1, (tq, d)).astype(np.float16)
    k = rng.uniform(-1, 1, (tokens, d)).astype(np.float16)
    v = rng.uniform(-1, 1, (tokens, dv)).astype(np.float16)
    q = (q.astype(np.float64) / np.sqrt(d)).astype(np.float16)

    score = q.astype(np.float64) @ k.astype(np.float64).T
    probability = np.exp(score - score.max(axis=1, keepdims=True))
    packed_probability = probability.astype(np.float16).astype(np.float64)
    output = (packed_probability @ v.astype(np.float64) /
              probability.sum(axis=1, keepdims=True)).astype(np.float32)

    # QK consumes one 16-key matrix tile at a time; keep its reduction pairs
    # adjacent inside each tile, matching the original direct-VRF operand.
    packed_k = vd.pack_pairs(vd.halves(k.T).reshape(d, tokens))
    packed_k = packed_k.reshape(d // 2, tokens // 16, 16).transpose(1, 0, 2)
    arrays = [
        vd.carray("flash_q", vd.pack_pairs(vd.halves(q.T).reshape(d, tq)).ravel()),
        vd.carray("flash_k", packed_k.ravel()),
        vd.carray("flash_v", vd.pack_pairs(vd.halves(v).reshape(tokens, dv)).ravel()),
        vd.carray("flash_golden", vd.words(output.T.copy()).ravel()),
    ]
    definitions = {"FLASH_TQ": tq, "FLASH_T": tokens,
                   "FLASH_D": d, "FLASH_DV": dv}
    destination = pathlib.Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("#include <stdint.h>\n" +
                           "".join(f"#define {name} {value}\n"
                                   for name, value in definitions.items()) +
                           "\n".join(arrays))


if __name__ == "__main__":
    main()
