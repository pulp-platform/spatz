# Copyright 2022 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Author: Mattia Sinigaglia, Universisty of Bologna

echo "export Spatz toolchains"
# LLVM 22 toolchain built in-repo by `make toolchain` (see root Makefile
# tc-llvm); the old spatz-llvm-2023.08.10 pack predates the LLVM realign
# and does not know the current custom instructions (incl. VLXBLK).
SPATZ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
export LLVM_INSTALL_DIR="${SPATZ_ROOT}/install/llvm"
export GCC_INSTALL_DIR=/usr/pack/riscv-1.0-kgf/spatz-gcc-7.1.1
