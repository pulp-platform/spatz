#!/bin/bash
# usage (from tb/spatz/sw/quadrilateroBenchmarks): bash scripts/gen_simple_data.sh [M N K]
set -e
M=${1:-64}; N=${2:-64}; K=${3:-64}
for d in sp-matmul-simple-quadrilatero sp-matmul-simple-trans-quadrilatero; do
  mkdir -p matmul/$d/data
  cp matmul/sp-matmul-quadrilatero/data/layer.h matmul/$d/data/
done
python3 scripts/dataMatMul.py --M $M --N $N --K $K --dtype int --bits 32 --transA 1 --transB 0 \
        --inc_dir matmul/sp-matmul-simple-quadrilatero/data
python3 scripts/dataMatMul.py --M $M --N $N --K $K --dtype int --bits 32 --transA 0 --transB 1 \
        --inc_dir matmul/sp-matmul-simple-trans-quadrilatero/data
f=data_${M}_${N}_${K}.h
if diff <(sed -n '/gemm_C_dram/,/};/p' matmul/sp-matmul-simple-quadrilatero/data/$f) \
        <(sed -n '/gemm_C_dram/,/};/p' matmul/sp-matmul-simple-trans-quadrilatero/data/$f) >/dev/null; then
  echo "SAME_C: normal and trans golden C are identical"
else
  echo "WARNING: golden C differs between the two data sets"; exit 1
fi
