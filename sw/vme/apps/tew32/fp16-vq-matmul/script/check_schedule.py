#!/usr/bin/env python3
"""Host-side address/coverage model; not a replacement for RTL simulation."""
import numpy as np


def check(k):
    rng = np.random.default_rng(42)
    cb0 = rng.normal(size=(256, 8)).astype(np.float16)
    cb1 = rng.normal(size=(256, 8)).astype(np.float16)
    ids0 = rng.integers(256, size=(k, 8))
    ids1 = rng.integers(256, size=(k, 8))
    scale = rng.uniform(.5, 1.5, size=k).astype(np.float16)
    expected = ((cb0[ids0].astype(np.float32) +
                 cb1[ids1].astype(np.float32)).astype(np.float16).astype(np.float32) *
                scale[:, None, None].astype(np.float32)).astype(np.float16).reshape(k, 64)
    packed = np.empty((4, k // 2, 16), dtype=np.uint32)
    for tile in range(4):
        for pair in range(k // 2):
            for col in range(16):
                g, lane = tile * 2 + col // 8, col % 8
                bits = []
                for q in (2 * pair + 1, 2 * pair):
                    o0 = 2 * (8 * ids0[q, g] + lane)
                    o1 = 2 * (8 * ids1[q, g] + lane)
                    value = np.float16(np.float32(cb0.flat[o0 // 2]) +
                                       np.float32(cb1.flat[o1 // 2]))
                    value = np.float16(np.float32(value) * np.float32(scale[q]))
                    bits.append(int(value.view(np.uint16)))
                packed[tile, pair, col] = bits[0] | bits[1] << 16
    reference_bits = expected.view(np.uint16).astype(np.uint32)
    for tile in range(4):
        cols = slice(tile * 16, (tile + 1) * 16)
        assert np.array_equal(packed[tile],
                              (reference_bits[0::2, cols] << 16) |
                              reference_bits[1::2, cols])

    covered = np.zeros((4, 4, k // 2), dtype=int)
    stores = np.zeros((64, 64), dtype=int)
    pending = None

    def store_tile(block, tile):
        assert np.all(covered[block, tile] == 1), "store before completion"
        for row in range(16):
            # Actual byte address: base + 256 per stored tile row.
            addr = ((block * 16) * 64 + tile * 16) * 4 + row * 256
            r, c = divmod(addr // 4, 64)
            stores[r, c:c + 16] += 1

    for block in range(4):
        if block:
            covered[block, 0, :4] += 1
            assert pending == block - 1
            store_tile(pending, 3)
            pending = None  # mt12 can now be zeroed.
        for kg in range(k // 8 - 2):
            for tile in range(4):
                if block and kg == 0 and tile == 0:
                    continue
                covered[block, tile, kg * 4:kg * 4 + 4] += 1
        for tile in range(4):
            covered[block, tile, k // 2 - 8:] += 1
            if tile:
                store_tile(block, tile - 1)
        pending = block
    store_tile(pending, 3)
    assert np.all(covered == 1), "missing or duplicate K pairs"
    assert np.all(stores == 1), "missing or duplicate output elements"
    print(f"K{k}: decode packing, K coverage, store addresses: PASS")


if __name__ == "__main__":
    for k in (128, 256, 512):
        check(k)
