# VME FP16-to-FP32 MatMul microbenchmark summary

Measured on 2026-11-16 with TE=16. VME is measured in this repository;
O-POPE is measured on the loosely coupled HWPE in the sibling
`baseline_spatz` repository. Reported cycles cover the kernel call only. Input
DMA, allocation, printing, and golden checks are outside the timed region. All
listed RTL runs completed with `Errors: 0` and `Status: PASS`.

Both systems receive the same logical FP16 matrices generated with seed 42.
O-POPE requires at least 16 elements per dimension, an even reduction size,
and an output-row count divisible by 16. Irregular workloads are therefore
zero-padded for O-POPE; the table shows the logical shape while the padding
column records the actual HWPE job. Only the logical output region is checked.

The ratio is `O-POPE cycles / VME cycles`. A value above 1 means VME is faster;
a value below 1 means O-POPE is faster.

## FP16-to-FP32 widening MatMul

VME uses `MTWIDEN=2` and stores FP32 accumulator rows with `vtse32`. O-POPE
uses its widening FP16 GEMM mode and writes FP32 output directly.

| Logical shape | O-POPE HWPE shape | VME schedule | VME cycles | O-POPE cycles | O-POPE/VME |
|---|---|---|---:|---:|---:|
| M13xN11xK19 | M16xN16xK20 | 1 tile | 363 | 314 | 0.865x |
| M16xN16xK64 | M16xN16xK64 | 1 tile | 826 | 403 | 0.488x |
| M13xN29xK19 | M16xN29xK20 | 2-tile compute/store ping-pong | 842 | 355 | 0.422x |
| M13xN29xK19 | M16xN29xK20 | 2-tile simultaneous reduction, A reuse | 537 | 355 | 0.661x |
| M16xN32xK64 | M16xN32xK64 | 2-tile compute/store ping-pong | 1822 | 535 | 0.294x |
| M16xN32xK64 | M16xN32xK64 | 2-tile simultaneous reduction, A reuse | 1139 | 535 | 0.470x |
| M29xN27xK19 | M32xN27xK20 | 4-tile pair ping-pong | 1502 | 463 | 0.308x |
| M29xN27xK19 | M32xN27xK20 | 4-tile simultaneous reduction, A+B reuse | 916 | 463 | 0.505x |
| M32xN32xK64 | M32xN32xK64 | 4-tile pair ping-pong | 2564 | 787 | 0.307x |
| M32xN32xK64 | M32xN32xK64 | 4-tile simultaneous reduction, A+B reuse | 1696 | 787 | 0.464x |
| M45xN37xK19 | M48xN37xK20 | automatic outer tiling | 2705 | 730 | 0.270x |
| M64xN64xK64 | M64xN64xK64 | automatic outer tiling | 5741 | 2323 | 0.405x |

## Findings

- For FP32 output, the loosely coupled O-POPE HWPE is faster in every measured
  case, including irregular shapes that require padded work.
- Within VME, simultaneous reduction remains substantially better than
  compute/store ping-pong because it reuses A, or both A and B. This is a VME
  schedule comparison and does not imply a win over O-POPE.

## Runtime shape masking in VME hardware

VME supports exact runtime matrix shapes through `tm`, `tn`, and `tk`; the
software does not need to pad each tile to 16x16x16. The controller clamps the
requested dimensions to the implemented limits and forwards them with every
OPE request. In the OPE MAC scheduler:

- `tm` and `tn` determine how many spatial groups are issued and generate a
  per-lane `mac_lane_active` mask for a partial edge tile.
- The lane-active tag follows the operation through the FPU pipeline. An
  inactive output lane does not assert its accumulator write enable.
- `tk` determines the reduction length. For an odd FP16 reduction tail, the
  unused upper element of the final packed pair is forced to zero and therefore
  makes no arithmetic contribution.
- A zero `tm`, `tn`, or `tk` is treated as an empty MAC operation.

This behavior is implemented in `spatz_controller.sv`, `spatz_ope.sv`, and
`opope_accumulator.sv`. It explains why the VME kernels can execute a logical
M13xN11xK19 job directly, whereas the current loosely coupled O-POPE interface
executes the padded M16xN16xK20 job.

The mask guarantees functional correctness and suppresses inactive
accumulator writes. It does **not**, by itself, prove a proportional power
reduction: clock distribution, operand delivery, and combinational logic inside
an inactive FPU lane may still toggle. Any power-saving claim must therefore be
based on post-synthesis or post-layout activity and power analysis.

## Recommendations for backend power evaluation

Use the following four single-tile cases to separate spatial masking from the
odd-K tail behavior. Keep the input distribution, random seed, frequency,
voltage, PVT corner, memory placement, and initial state identical.

| Case | Logical shape | Purpose |
|---|---|---|
| Full reference | M16xN16xK20 | No masked spatial lane and no odd-K tail |
| Spatial mask | M13xN11xK20 | Isolate the effect of `tm`/`tn` masking |
| Reduction tail | M16xN16xK19 | Isolate the odd `tk` tail |
| Combined mask | M13xN11xK19 | Measure the actual irregular workload |

Backend measurement guidance:

1. Capture VCD or SAIF only between the kernel benchmark start and stop
   markers. Exclude DMA setup, printing, and golden-result checking.
2. Prefer a post-layout netlist with extracted parasitics and clock-tree power.
   A post-synthesis estimate is useful for an early comparison but should be
   identified as such.
3. Report average dynamic, leakage, and total power; kernel cycles and time;
   total kernel energy; and energy per logical MAC (`M*N*K`). Power alone can
   be misleading when the cases have different execution times.
4. Break down activity and power for the OPE FPU array, accumulator
   tiles, VRF/VLSU, controller/front end, clock network, and whole cluster.
   Inspect the synthesized equivalents of `mac_lane_active`, the pipelined
   lane-active tag, and accumulator FPU write enables to confirm that the mask
   reaches the intended lanes.
5. Compare the exact VME M13xN11xK19 execution both with the full VME reference
   and with O-POPE's padded M16xN16xK20 execution. Clearly separate savings due
   to fewer cycles from savings due to lower switching activity per cycle.

For M13xN11, 143 of 256 spatial output lanes are active (55.9%). The remaining
44.1% inactive-lane ratio is a hardware-utilization figure, **not** an expected
44.1% total-power reduction. The backend results should be used to quantify
the real reduction after clock, control, memory, and always-active overheads.

## Binary targets

VME binaries are under
`spatz/hw/system/spatz_cluster/sw/build/vme/VMEBenchmarks`. Their targets are:

- `test-vme-widening-hp-fmatmul-1tile_M{13_N11_K19,16_N16_K64}`
- `test-vme-widening-hp-fmatmul-2tile-{pingpong,reuse}_M{13_N29_K19,16_N32_K64}`
- `test-vme-widening-hp-fmatmul-4tile-{pingpong,reuse}_M{29_N27_K19,32_N32_K64}`
- `test-vme-widening-hp-fmatmul-auto_M{45_N37_K19,64_N64_K64}`

Matched O-POPE binaries are under
`baseline_spatz/hw/system/spatz_cluster/sw/build/opopeBenchmarks/VMEBenchmarks/fmatmul`.
They use the same suffixes as the VME targets and the prefix
`test-opopeBenchmarks-`.

Build from the corresponding repository root with:

```sh
cmake --build hw/system/spatz_cluster/sw/build --target <target> -j 4
```
