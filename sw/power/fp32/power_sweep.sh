#!/usr/bin/env bash
# power_sweep.sh – Automated VCD → PrimeTime power analysis for all FP32 kernels
#
# Usage:  bash power_sweep.sh [tests.list]
#   If no argument, generates tests.list from all 64 binaries, then runs the sweep.
#
set -euo pipefail

##########################
# CONFIGURATION
##########################

TOP_DIR="/usr/scratch2/pisoc1/msc25h16/spatz-cluster-gf12"
SIM_DIR="${TOP_DIR}/gf12/modelsim"
GF12_DIR="${TOP_DIR}/gf12"
VCD_DIR="${GF12_DIR}/modelsim/vcd"
REPORTS_DIR="${GF12_DIR}/fusion/reports"
REPORT_SUBDIR="spatz_cluster_wrapper_1.05ns"
REPORTS_RUN_DIR="${REPORTS_DIR}/${REPORT_SUBDIR}"
BACKUP_ROOT="${TOP_DIR}/reports_archive"

# Build directory for the power-fp32 tests
BUILD_DIR="${TOP_DIR}/spatz/hw/system/spatz_cluster/sw/build/power_fp32"
TEST_PREFIX="test-power-fp32-"

# All 16 kernels (8 SW + 8 HW)
KERNELS=(exp_sw exp_hw cosh_sw cosh_hw tanh_sw tanh_hw log_sw log_hw rec_sw rec_hw rsqrt_sw rsqrt_hw sin_sw sin_hw cos_sw cos_hw)
# All LMUL modes
LMULS=(1 2 4 8)

##########################
# GENERATE tests.list
##########################

TEST_LIST_FILE="${1:-}"

if [[ -z "${TEST_LIST_FILE}" ]]; then
    TEST_LIST_FILE="${GF12_DIR}/tests_power_fp32.list"
    echo "# Auto-generated power-fp32 test list" > "${TEST_LIST_FILE}"
    for kern in "${KERNELS[@]}"; do
        for lmul in "${LMULS[@]}"; do
            echo "${BUILD_DIR}/${TEST_PREFIX}${kern}_m${lmul}" >> "${TEST_LIST_FILE}"
        done
    done
    echo "Generated ${TEST_LIST_FILE} with $(wc -l < "${TEST_LIST_FILE}") entries"
fi

if [[ ! -f "${TEST_LIST_FILE}" ]]; then
    echo "Test list not found: ${TEST_LIST_FILE}"
    exit 1
fi

mkdir -p "${BACKUP_ROOT}"

##########################
# MAIN LOOP
##########################

TOTAL=0
PASS=0
FAIL=0

while IFS= read -r test_bin; do
    # Skip blanks and comments
    [[ -z "${test_bin}" ]] && continue
    [[ "${test_bin}" =~ ^# ]] && continue

    test_name="$(basename "${test_bin}")"
    echo ""
    echo "========================================================"
    echo "  TEST: ${test_name}"
    echo "========================================================"
    TOTAL=$((TOTAL + 1))

    if [[ ! -f "${test_bin}" ]]; then
        echo "WARNING: binary not found: ${test_bin}"
        FAIL=$((FAIL + 1))
        continue
    fi

    # --- Step 1: Generate simulation script with VCD ---
    echo "[1/4] Generating simulation script (VCD enabled)..."
    (
        cd "${SIM_DIR}"
        make spatz_pls_sim.sh spatz_vcd=1
    )

    # --- Step 2: Run ModelSim simulation ---
    echo "[2/4] Running simulation: ${test_name}..."
    (
        cd "${SIM_DIR}"
        source spatz_pls_sim.sh "${test_bin}"
    )

    # --- Step 3: Run PrimeTime power analysis ---
    echo "[3/4] Running PrimeTime power analysis..."
    (
        cd "${GF12_DIR}"
        make spatz_power
    )

    # --- Step 4: Archive reports ---
    echo "[4/4] Archiving reports for ${test_name}..."
    DEST="${BACKUP_ROOT}/${test_name}"
    mkdir -p "${DEST}"

    if [[ -d "${REPORTS_RUN_DIR}" ]]; then
        cp -r "${REPORTS_RUN_DIR}"/* "${DEST}/" 2>/dev/null || true
        echo "  Reports saved → ${DEST}"
        PASS=$((PASS + 1))
    else
        echo "  WARNING: Report directory not found: ${REPORTS_RUN_DIR}"
        FAIL=$((FAIL + 1))
    fi

    # --- Cleanup VCD to save disk ---
    if [[ -d "${VCD_DIR}" ]]; then
        rm -rf "${VCD_DIR}"
        echo "  VCD directory cleaned"
    fi

    du -sh "${DEST}" 2>/dev/null || true

done < "${TEST_LIST_FILE}"

echo ""
echo "========================================================"
echo "  SWEEP COMPLETE:  ${PASS}/${TOTAL} passed,  ${FAIL} failed"
echo "  Reports in: ${BACKUP_ROOT}"
echo "========================================================"
