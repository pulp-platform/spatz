#!/usr/bin/env bash
# power_sweep.sh – Automated VCD → PrimeTime power analysis for all BF16 kernels
#
# Usage:  bash power_sweep.sh [tests.list]
#   If no argument, generates tests.list from all 64 binaries, then runs the sweep.
#
# Prerequisites:
#   1) BF16 power binaries built (cmake --build ... in spatz_cluster/sw/build)
#   2) Gate-level netlist compiled once:
#        cd $SIM_DIR && make spatz_pls_sim.sh spatz_vcd=1
#
set -euo pipefail

##########################
# CONFIGURATION
##########################

TOP_DIR="/usr/scratch2/pisoc1/msc25h16/spatz-cluster-gf12"
SIM_DIR="${TOP_DIR}/gf12/modelsim"
GF12_DIR="${TOP_DIR}/gf12"
VCD_DIR="${GF12_DIR}/modelsim/vcd"
REPORTS_DIR="${GF12_DIR}/fusion/reports/spatz_cluster_wrapper_1.05ns"
BACKUP_ROOT="${TOP_DIR}/reports_archive"

# Build directory for the power-bf16 tests
BUILD_DIR="${TOP_DIR}/spatz/hw/system/spatz_cluster/sw/build/power_bf16"
TEST_PREFIX="test-power-bf16-"

# All 16 kernels (8 SW + 8 HW)
KERNELS=(exp_sw exp_hw cosh_sw cosh_hw tanh_sw tanh_hw log_sw log_hw rec_sw rec_hw rsqrt_sw rsqrt_hw sin_sw sin_hw cos_sw cos_hw)
# All LMUL modes
LMULS=(1 2 4 8)

##########################
# GENERATE tests.list
##########################

TEST_LIST_FILE="${1:-}"

if [[ -z "${TEST_LIST_FILE}" ]]; then
    TEST_LIST_FILE="${GF12_DIR}/tests_power_bf16.list"
    echo "# Auto-generated power-bf16 test list" > "${TEST_LIST_FILE}"
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
# GATE-LEVEL COMPILE
# (only needed once)
##########################

if [[ ! -f "${SIM_DIR}/spatz_pls_sim.sh" ]]; then
    echo "===== Compiling gate-level netlist (one-time) ====="
    (cd "${SIM_DIR}" && make spatz_pls_sim.sh spatz_vcd=1)
fi

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

    # --- Step 1: Run ModelSim post-layout simulation (produces VCD) ---
    echo "[1/3] Running post-layout simulation: ${test_name}..."
    if ! (
        cd "${SIM_DIR}"
        mkdir -p vcd
        bash spatz_pls_sim.sh "${test_bin}"
    ); then
        echo "  ERROR: simulation failed for ${test_name}"
        FAIL=$((FAIL + 1))
        continue
    fi

    # --- Step 2: Run PrimeTime power analysis ---
    echo "[2/3] Running PrimeTime power analysis (NAME=${test_name})..."
    if ! (
        cd "${GF12_DIR}"
        make spatz_power NAME="${test_name}" VCD=dump.vcd
    ); then
        echo "  ERROR: PrimeTime failed for ${test_name}"
        FAIL=$((FAIL + 1))
        rm -f "${VCD_DIR}/dump.vcd"
        continue
    fi

    # --- Step 3: Archive reports ---
    echo "[3/3] Archiving reports for ${test_name}..."
    DEST="${BACKUP_ROOT}/${test_name}"
    mkdir -p "${DEST}"

    # power.tcl writes: signoff_{power,timing}_${NAME}.rpt + signoff_setup_${NAME}.log
    for f in "${REPORTS_DIR}/signoff_power_${test_name}.rpt" \
             "${REPORTS_DIR}/signoff_timing_${test_name}.rpt" \
             "${REPORTS_DIR}/signoff_setup_${test_name}.log"; do
        if [[ -f "${f}" ]]; then
            cp "${f}" "${DEST}/"
        fi
    done

    if [[ -n "$(ls -A "${DEST}" 2>/dev/null)" ]]; then
        echo "  Reports saved → ${DEST}"
        PASS=$((PASS + 1))
    else
        echo "  WARNING: No report files found for ${test_name}"
        FAIL=$((FAIL + 1))
    fi

    # --- Cleanup VCD to save disk ---
    rm -f "${VCD_DIR}/dump.vcd"
    echo "  VCD cleaned"

    du -sh "${DEST}" 2>/dev/null || true

done < "${TEST_LIST_FILE}"

echo ""
echo "========================================================"
echo "  SWEEP COMPLETE:  ${PASS}/${TOTAL} passed,  ${FAIL} failed"
echo "  Reports in: ${BACKUP_ROOT}"
echo "========================================================"
