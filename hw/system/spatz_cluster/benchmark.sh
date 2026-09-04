# make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=l2 MEAS_1ITER=1
# CONFIG=l2_cold
insn=coal-fix
mkdir -p $insn
# mkdir -p $insn/$CONFIG
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$CONFIG/fft-2048.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N256_K128 > $insn/$CONFIG/gemm-128x256x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M256_N256_K256 > $insn/$CONFIG/gemm-256x256x256.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K128_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x128x64-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K256_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x256x64-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M64_N512_K16 > $insn/$CONFIG/sa-spmv-64x512-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M128_N512_K16 > $insn/$CONFIG/sa-spmv-128x512-16.rpt

# make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=dram MEAS_1ITER=1
# CONFIG=dram_cold
# mkdir -p $insn/$CONFIG
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$CONFIG/fft-2048.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N256_K128 > $insn/$CONFIG/gemm-128x256x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M256_N256_K256 > $insn/$CONFIG/gemm-256x256x256.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K128_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x128x64-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K256_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x256x64-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M64_N512_K16 > $insn/$CONFIG/sa-spmv-64x512-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M128_N512_K16 > $insn/$CONFIG/sa-spmv-128x512-16.rpt

# make clean sw.vsim -B USE_CACHE=0 SNRT_LINK=l2 MEAS_1ITER=1
# CONFIG=l2_spm
# mkdir -p $insn/$CONFIG
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fft_M1024_N4_K2 > $insn/$CONFIG/fft-1024-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fft_M2048_N8_K2 > $insn/$CONFIG/fft-2048-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M128_N256_K128 > $insn/$CONFIG/gemm-128x256x128-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M256_N256_K256 > $insn/$CONFIG/gemm-256x256x256-db.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K128_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x128x64-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K256_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x256x64-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M64_N512_K16 > $insn/$CONFIG/sa-spmv-64x512-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M128_N512_K16 > $insn/$CONFIG/sa-spmv-128x512-16.rpt


# make clean sw.vsim -B USE_CACHE=0 SNRT_LINK=dram MEAS_1ITER=1
# CONFIG=dram_spm
# mkdir -p $insn/$CONFIG
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$CONFIG/fft-2048.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M128_N256_K128 > $insn/$CONFIG/gemm-128x256x128-db.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-db-fmatmul-4x4vl_M256_N256_K256 > $insn/$CONFIG/gemm-256x256x256-db.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K128_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x128x64-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmm_M64_K256_N64_NZR16 > $insn/$CONFIG/sa-spmm-64x256x64-16.rpt
# ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M64_N512_K16 > $insn/$CONFIG/sa-spmv-64x512-16.rpt
# # ./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-rs-spmv_M128_N512_K16 > $insn/$CONFIG/sa-spmv-128x512-16.rpt




make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=l2 MEAS_1ITER=0
CONFIG=l2_hot
mkdir -p $insn/$CONFIG
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M128_N128 > $insn/$CONFIG/gemv-128x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-gemv_M4096_N128 > $insn/$CONFIG/gemv-4096x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-sa-gemv_M128_N4096_K512 > $insn/$CONFIG/sa-gemv-128x4096.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$CONFIG/fft-2048.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128.rpt
make clean sw.vsim -B USE_CACHE=1 SNRT_LINK=dram MEAS_1ITER=0
CONFIG=dram_hot
mkdir -p $insn/$CONFIG
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M64_N128 > $insn/$CONFIG/gemv-64x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-gemv_M128_N128 > $insn/$CONFIG/gemv-128x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-gemv_M4096_N128 > $insn/$CONFIG/gemv-4096x128.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-hp-sa-gemv_M128_N4096_K512 > $insn/$CONFIG/sa-gemv-128x4096.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fdotp_M4096 > $insn/$CONFIG/dotp-4096.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M1024_N2 > $insn/$CONFIG/fft-1024.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fft_M2048_N2 > $insn/$CONFIG/fft-2048.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M64_N64_K64 > $insn/$CONFIG/gemm-64x64x64.rpt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-dp-fmatmul-4x4vl_M128_N128_K128 > $insn/$CONFIG/gemm-128x128x128.rpt

