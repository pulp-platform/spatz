#!/bin/bash
# Data for matmul/gemm-quadrilatero: one header per datatype in GEMM_DTYPES, size in GEMM_SIZES
# (both in CMakeLists.txt) and layout:
#   data/<DT>/data_M_N_K.h    --transA 1 --transB 0  (normal kernels:     A^T, B packed along K)
#   data_t/<DT>/data_M_N_K.h  --transA 0 --transB 1  (transposed kernels: A, Bt packed along K)
# usage (from tb/spatz/sw/quadrilateroBenchmarks): bash scripts/gen_gemm_data.sh [-f]   (-f regenerates all)
set -e
FORCE=0; [[ ${1:-} == -f ]] && FORCE=1
D=matmul/gemm-quadrilatero
LAYER=matmul/widening4x-bp-altfmatmul-quadrilatero/data/layer.h   # precision_t with FP8E4M3, as dataMatMul.py writes
cmake_list() { sed -n "/^set($1/,/)/p" CMakeLists.txt | sed "s/set($1//; s/)//" | tr -s ' \n' ' '; }
SIZES=$(cmake_list GEMM_SIZES); DTYPES=$(cmake_list GEMM_DTYPES)
[[ -n $SIZES && -n $DTYPES ]] || { echo "GEMM_SIZES / GEMM_DTYPES not found in CMakeLists.txt"; exit 1; }

dt_args() {   # datatype -> dataMatMul.py arguments
  case $1 in
    INT8|INT16|INT32)    echo "--dtype int --bits ${1#INT}" ;;
    UINT8|UINT16|UINT32) echo "--dtype uint --bits ${1#UINT}" ;;
    FP32)   echo "--dtype float --bits 32" ;;
    FP16)   echo "--dtype float --bits 16" ;;
    BF16)   echo "--dtype bfloat --bits 16" ;;
    FP8)    echo "--dtype float --bits 8 --fp8_fmt e5m2" ;;
    FP8ALT) echo "--dtype float --bits 8 --fp8_fmt e4m3" ;;
    *) echo "unknown datatype $1" >&2; exit 1 ;;
  esac
}

for dt in $DTYPES; do
  args=$(dt_args $dt)
  for sub in data/$dt data_t/$dt; do mkdir -p $D/$sub; cp $LAYER $D/$sub/; done
  for sz in $SIZES; do
    IFS=x read -r M N K <<< "$sz"
    for t in 0 1; do
      if [[ $t == 0 ]]; then sub=data/$dt; ta=1; tb=0; else sub=data_t/$dt; ta=0; tb=1; fi
      f=$D/$sub/data_${M}_${N}_${K}.h
      [[ -f $f && $FORCE == 0 ]] && continue
      python3 scripts/dataMatMul.py --M $M --N $N --K $K $args --transA $ta --transB $tb --inc_dir $D/$sub > /dev/null
      echo "generated $f"
    done
  done
done
