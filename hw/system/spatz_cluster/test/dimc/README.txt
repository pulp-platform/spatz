DIMC RTL regressions
====================
Use the patched spatz_vpu dependency and the DIMC cluster configuration.
The tests require Bender, Verilator, a native C++ compiler, and the Python
packages used by the official RTL configuration generators.
Use Verilator 5.050, matching util/iis-env.sh in this upstream version.

From the Spatz repository root:
  bash hw/system/spatz_cluster/test/dimc/run.sh

The early-done test checks numerical dot products, queue turnover, delayed
operand reads, response IDs, and completion after accepted VRF writes.
The overlap test checks independent DIMC/IPU execution, shared-port arbitration,
held results, scoreboard dependencies, and ordering around floating-point work.

Software tests:
Use SPATZ_CLUSTER_CFG=spatz_cluster.dimc.dram.hjson when building the cluster.
Software runtime settings must match the generated hardware. For this DIMC
configuration: ELEN=64, SNRT_CLUSTER_CORE_NUM=2, SNRT_TCDM_START_ADDR=1048576,
SNRT_TCDM_SIZE=131072, and RUNTIME_PRINT=ON. Use the normal cluster build
configuration to supply these, or pass them when building tests separately.
Enable SPATZ_DIMC_TESTS=ON for the basic instruction/CSR regression and
SPATZ_DIMC_VMVM_TESTS=ON for matrix scheduling regressions. Both are CMake options.
The matrix tests generate deterministic synthetic inputs in the build directory.
The .word encoding does not require a private sf.vqmmacc compiler extension.

No generated fixture, simulation output, or local performance report belongs
in the source change.

All six existing ResNet workloads
--------------------------------
Enable SPATZ_DIMC_RESNET_TESTS=ON to generate synthetic inputs and build:
  test-riscvTests-vmvm_resnet_Conv1_check
  test-riscvTests-vmvm_resnet_Conv2_check
  test-riscvTests-vmvm_resnet_Conv3_check
  test-riscvTests-vmvm_resnet_Conv4_check
  test-riscvTests-vmvm_resnet_Conv5_check
  test-riscvTests-vmvm_resnet_FinalFC_check
Run each ELF using the official cluster simulator in the DIMC configuration.
The default DIMC_RESNET_RUN_ROWS=0 checks every output of each full shape.
For a shorter numerical prefix run, set DIMC_RESNET_RUN_ROWS to a positive
row count divisible by 8 (for example 8 or 16). K, N, input layout, and the full reference shapes remain unchanged;
FinalFC always checks all 1000 outputs. Prefix checks do not validate omitted
rows or the full convolution scheduling sequence.

Workload     M        logical K    padded K    N
Conv1        12544    147          256         64
Conv2        3136     576          640         64
Conv3        784      1152         1152        128
Conv4        196      2304         2304        256
Conv5        49       4608         4608        512
FinalFC      1        2048         2048        1000

These are the six GEMM workloads in the existing local ResNet suite. They
are not separate tests for every convolution/projection in ResNet-50 and
do not implement end-to-end inference, pooling, residual additions or ReLU.
No trained weights or private fixture files are required.
