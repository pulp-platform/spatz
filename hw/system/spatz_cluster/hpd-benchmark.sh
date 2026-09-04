insn=hpd-new2

# MSHR sweep: one entry per cfg/<name>.hjson (see cfg/hpd-<sets>x<ways>.hjson).
# Add/remove entries here to change what the sweep covers.
# CFG_LIST=(hpd-16x4 hpd-4x4 hpd-8x4 hpd-4x2)
CFG_LIST=(hpd-16x4)

for CONFIG in "${CFG_LIST[@]}"; do
    make clean sw.vsim -B CFG=${CONFIG} SNRT_LINK=dram MEAS_1ITER=0 PYTHON=python3
    DIR=${CONFIG}-dram
    mkdir -p $insn/$DIR
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M128_N128 > $insn/$DIR/gemv-128x128.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-gemv_M4096_N128 > $insn/$DIR/gemv-4096x128.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-sa-gemv_M128_N4096_K512 > $insn/$DIR/sa-gemv-128x4096.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$DIR/dotp-4096.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$DIR/fft-2048.rpt
    # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$DIR/gemm-64x64x64.rpt
    # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$DIR/gemm-128x128x128.rpt

    make clean sw.vsim -B CFG=${CONFIG} SNRT_LINK=l2 MEAS_1ITER=0 PYTHON=python3
    DIR=${CONFIG}-l2
    mkdir -p $insn/$DIR
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M128_N128 > $insn/$DIR/gemv-128x128.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-gemv_M4096_N128 > $insn/$DIR/gemv-4096x128.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-sa-gemv_M128_N4096_K512 > $insn/$DIR/sa-gemv-128x4096.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$DIR/dotp-4096.rpt
    ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$DIR/fft-2048.rpt
    # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$DIR/gemm-64x64x64.rpt
    # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$DIR/gemm-128x128x128.rpt
done
