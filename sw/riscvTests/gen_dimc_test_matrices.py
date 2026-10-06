#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate deterministic synthetic DIMC inputs and integer reference results."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--rows', type=int, default=16)
    parser.add_argument('--k', type=int, default=256)
    parser.add_argument('--cols', type=int, default=32)
    parser.add_argument('--logical-k', type=int)
    args = parser.parse_args()
    logical = args.logical_k or args.k
    if args.rows <= 0 or args.k <= 0 or args.k % 128 or args.cols <= 0 or args.cols % 8 or not 0 < logical <= args.k:
        parser.error('Require positive rows, K divisible by 128, N divisible by 8, and 0 < logical K <= K')
    if args.rows > 1 and args.cols % 32:
        parser.error('Multi-row scheduling tests require N divisible by 32')
    a = [[(r * 3 + k * 5 + 1) % 16 if k < logical else 0 for k in range(args.k)] for r in range(args.rows)]
    b = [[(c * 7 + k * 3 + 2) % 16 if k < logical else 0 for k in range(args.k)] for c in range(args.cols)]
    # Inputs repeat every 16 rows/channels. Compute each independent dot
    # product once; full layer references then need O(M*N), not O(M*N*K).
    dots = {(r, c): sum(x * y for x, y in zip(a[r], b[c]))
            for r in range(min(args.rows, 16)) for c in range(min(args.cols, 16))}
    golden = [dots[r % 16, c % 16]
              for cb in range(0, args.cols, 16) for r in range(args.rows)
              for c in range(cb, min(cb + 16, args.cols))]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w') as f:
        f.write('#ifndef DIMC_TEST_MATRICES_H\n#define DIMC_TEST_MATRICES_H\n#include <stdint.h>\n')
        for name, value in {'FEAT_ROWS': args.rows, 'FEAT_COLS': args.k, 'KERN_ROWS': args.cols,
                            'KERN_COLS': args.k, 'OUT_ROWS': args.rows, 'OUT_COLS': args.cols,
                            'VMVM_LOGICAL_K': logical, 'VMVM_PADDED_K': args.k,
                            'VMVM_BENCH_CASE': 0, 'VMVM_TILE_ROWS_OVERRIDE': 0,
                            'VMVM_LOOP_PROFILE': 0}.items():
            f.write(f'#define {name} {value}\n')
        for name, data in [('data_A', a), ('data_B', b)]:
            f.write(f'static uint8_t {name}[{len(data)}][{args.k}] = {{\n')
            for row in data:
                f.write('{' + ','.join(map(str, row)) + '},\n')
            f.write('};\n')
        f.write('static uint32_t serialized_C[] = {' + ','.join(map(str, golden)) + '};\n#endif\n')


if __name__ == '__main__':
    main()
