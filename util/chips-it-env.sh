# Author: Riccardo Giunti, Fondazione Chips-IT

echo "Export Spatz toolchains for Chips-IT"
export LLVM_INSTALL_DIR=/opt/riscv/spatz-22-llvm
export GCC_INSTALL_DIR=/opt/riscv/spatz-gcc-7.1.1
export VERDI_HOME=/tools/synopsys/verdi/W-2024.09-SP1

QUESTA_VERSION="2025.3"
echo "Load Questa $QUESTA_VERSION environment for Chips-IT"
module load questa/$QUESTA_VERSION

XLM_VERSION="25.03"
echo "Load Xcelium $XLM_VERSION environment for Chips-IT"
module load xcelium/$XLM_VERSION

VCS_VERSION="2024.09"
echo "Load VCS $VCS_VERSION environment for Chips-IT"
module load vcs/$VCS_VERSION

BENDER_VERSION="0.31.0"
echo "Load Bender $BENDER_VERSION environment for Chips-IT"
module load bender/$BENDER_VERSION
export BENDER_INSTALL_DIR=/tools/utils/bender_$BENDER_VERSION

VLT_VERSION="5.050"
echo "Load Verilator $VLT_VERSION environment for Chips-IT"
GCC_TOOLSET=/opt/rh/gcc-toolset-12
if [ -f "$GCC_TOOLSET/enable" ]; then
    echo "Load GCC toolset 12 from $GCC_TOOLSET"
    source "$GCC_TOOLSET/enable"
else
    echo "WARNING: $GCC_TOOLSET/enable not found, using system g++"
fi
module load verilator/$VLT_VERSION
export VERILATOR_INSTALL_DIR=/tools/verilator/${VLT_VERSION}

# riscv-opcodes requires Python >= 3.9
export PYTHON=python3.12
if [[ ! -x ".venv/bin/${PYTHON}" ]]; then
  echo "Creating venv in .venv using ${PYTHON}..."
  rm -rf .venv
  "${PYTHON}" -m venv --prompt spatz .venv || { echo "ERROR: venv creation failed"; return 1; }
  echo "Installing Python deps from requirements.txt into .venv"
  .venv/bin/python -m pip install -r requirements.txt || { echo "ERROR: pip install failed"; return 1; }
fi

if [[ "${VIRTUAL_ENV}" != "${PWD}/.venv" ]]; then
  _ps1_before="${PS1}"
  . .venv/bin/activate
  PS1="(spatz) ${_ps1_before}"
  unset _ps1_before
fi