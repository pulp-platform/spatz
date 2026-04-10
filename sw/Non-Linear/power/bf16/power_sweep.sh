#!/usr/bin/env bash
# power_sweep.sh – Build & run all BF16 power kernels × LMUL combos
#
# Usage:
#   ./power_sweep.sh [BUILD_DIR]
#
# Default BUILD_DIR: ../../hw/system/spatz_cluster/build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${1:-${SCRIPT_DIR}/../../hw/system/spatz_cluster/build}"
TEST_PREFIX="test-power-bf16-"

KERNELS="exp_sw exp_hw cosh_sw cosh_hw tanh_sw tanh_hw log_sw log_hw rec_sw rec_hw rsqrt_sw rsqrt_hw sin_sw sin_hw cos_sw cos_hw"
LMULS="1 2 4 8"

echo "=== BF16 Power Sweep ==="
echo "BUILD_DIR: ${BUILD_DIR}"
echo ""

total=0
pass=0
fail=0

for k in ${KERNELS}; do
    for m in ${LMULS}; do
        target="${TEST_PREFIX}${k}_m${m}"
        echo -n "[${k} LMUL=${m}] "
        total=$((total + 1))

        # Build
        if ! make -C "${BUILD_DIR}" "${target}" -j$(nproc) > /dev/null 2>&1; then
            echo "BUILD FAILED"
            fail=$((fail + 1))
            continue
        fi

        # Run
        output=$(make -C "${BUILD_DIR}" "run-${target}" 2>&1) || true
        cycles=$(echo "${output}" | grep -oP 'cycles=\K[0-9]+' || echo "?")
        echo "cycles=${cycles}"
        pass=$((pass + 1))
    done
done

echo ""
echo "=== Summary: ${pass}/${total} passed, ${fail} failed ==="
