# VME widening FP16-to-FP32 MatMul benchmark

This benchmark implements a one-tile widening matrix multiplication with
FP16 inputs and an FP32 output. It mirrors the directory organization of
`sw/spatzBenchmarks/widening-hp-fmatmul`, while using the VME accumulator
tile directly.

The supported shape is `1 <= M,N <= TE`, where `TE=16`, and `K >= 1`.
The kernel sets `tm=M` and `tn=N` at runtime. The reduction uses packed FP16
pairs with `tk=2`, except for an odd final K element, which uses `tk=1`.
`MTWIDEN=2` retains the result in FP32, and each active accumulator row is
written directly to row-major FP32 memory with `vtse32`.

Input layout:

- `Apack[ceil(K/2)][M]` packs `{A[m][2p+1], A[m][2p]}`.
- `Bpack[ceil(K/2)][N]` packs `{B[2p+1][n], B[2p][n]}`.
- `C[M][N]` is ordinary row-major FP32.

Tests:

- `M13_N11_K19`: partial TM/TN masks and an odd-K `tk=1` tail.
- `M16_N16_K64`: full one-tile boundary.

## Two-tile schedules

The two-tile kernels currently use a `1x2` output layout: `M<=16` and
`16<N<=32`, with `mt0` holding columns 0-15 and `mt4` holding the remaining
columns.

- `compute-store-ping-pong` completes mt0 first, then interleaves one mt0
  `vtse32` row with each mt4 reduction step. Only the final mt4 drain remains
  outside useful matrix computation.
- `simultaneous-reduction/A-reuse` loads each packed A vector once, updates
  mt0 and mt4 together, and drains both tiles after the reduction finishes.

Two-tile tests use `M13_N29_K19` for partial/odd tails and `M16_N32_K64` for
the full two-tile boundary.

## Four-tile schedules

Four tiles use a `2x2` output layout for `16<M,N<=32`:

- `mt0=A0*B0`, `mt4=A0*B1`
- `mt8=A1*B0`, `mt12=A1*B1`

The pair ping-pong schedule completes `mt0/mt4`, then interleaves their row
stores while reducing `mt8/mt12`. The simultaneous schedule loads A0, A1, B0,
and B1 once per K pair and updates all four tiles, providing both A and B
reuse before draining the complete panel.

Four-tile tests use `M29_N27_K19` for partial/odd tails and `M32_N32_K64` for
the full `2x2` boundary.

## Automatic outer tiling

`vme_widening_hp_fmatmul_auto` accepts arbitrary positive M, N, and K. Its
outer loops split the output into panels of at most `32x32`; each panel selects
its tile grid from the active shape:

- `M<=16, N<=16`: one tile.
- Exactly one dimension exceeds 16: two tiles (`1x2` or `2x1`).
- Both dimensions exceed 16: four tiles (`2x2`).

Packed A/B keep their full-matrix pair strides, so no operand repacking is
needed between panels. `M45_N37_K19` exercises all four panel types in one
kernel call, including partial M/N and odd-K tails.
