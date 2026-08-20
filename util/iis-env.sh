# Copyright 2022 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Author: Mattia Sinigaglia, Universisty of Bologna

echo "export Spatz toolchains"

export TOOLS_DIR=/usr/scratch2/pisoc10/spatz/tools

export LLVM_INSTALL_DIR=$TOOLS_DIR/riscv-llvm
export GCC_INSTALL_DIR=/usr/pack/riscv-1.0-kgf/spatz-gcc-7.1.1

export VERILATOR_INSTALL_DIR=$TOOLS_DIR/verilator
export VERILATOR_ROOT=$VERILATOR_INSTALL_DIR/share/verilator
export LD_PRELOAD=$VERILATOR_INSTALL_DIR/libstdc++.so.6

export CC=gcc-11.2.0
export CXX=g++-11.2.0

export CMAKE=/usr/bin/cmake

python3 -m venv spatz_env
source spatz_env/bin/activate
python3 -m pip install --quiet --upgrade pip
python3 -m pip install --quiet -r requirements.txt