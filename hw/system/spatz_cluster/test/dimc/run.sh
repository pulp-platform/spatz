#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
repo_root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
cluster="$repo_root/hw/system/spatz_cluster"
build_dir=${DIMC_TEST_BUILD_DIR:-"$repo_root/build/dimc"}
bender=${BENDER:-bender}
verilator=${VERILATOR:-verilator}
mkdir -p "$build_dir"
cd "$repo_root"
PYTHON=${PYTHON:-python3} python3 util/clustergen.py \
    -c "$cluster/cfg/spatz_cluster.dimc.dram.hjson" -o "$cluster/src"
"$bender" script verilator -t rtl -t spatz -t spatz_test -t snitch_test \
    --define COMMON_CELLS_ASSERTS_OFF > "$build_dir/files"
for top in tb_dimc_early_done tb_dimc_ipu_overlap; do
    "$verilator" --binary --timing --top-module "$top" \
        -Wno-fatal -Wno-BLKANDNBLK -Wno-WIDTH -Wno-WIDTHCONCAT \
        --unroll-count 1024 -j "${JOBS:-2}" --Mdir "$build_dir/$top" \
        -f "$build_dir/files" "$cluster/test/dimc/$top.sv"
    if [[ "$top" == tb_dimc_early_done ]]; then
        "$build_dir/$top/V$top" +expect_early
    else
        "$build_dir/$top/V$top"
    fi
done
