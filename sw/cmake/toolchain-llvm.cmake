# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Look for the precompiled binaries
set(CMAKE_C_COMPILER ${LLVM_DIR}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_DIR}/bin/clang++)
set(CMAKE_OBJCOPY ${LLVM_DIR}/bin/llvm-objcopy)
set(CMAKE_OBJDUMP ${LLVM_DIR}/bin/llvm-objdump --mcpu=snitch)
set(CMAKE_AR ${LLVM_DIR}/bin/llvm-ar)
set(CMAKE_STRIP ${LLVM_DIR}/bin/llvm-strip)
set(CMAKE_RANLIB ${LLVM_DIR}/bin/llvm-ranlib)

##
## Compile options
##
add_compile_options(-mcmodel=small -ffast-math -fno-builtin-printf -fno-common)
add_compile_options(-ffunction-sections)
add_compile_options(-Wextra)
#add_compile_options(-static)
add_compile_options(-mllvm -misched-topdown)
# For smallfloat we need experimental extensions enabled (Zfh)
add_compile_options(-menable-experimental-extensions)
# LLD doesn't support relaxation for RISC-V yet
add_compile_options(-mno-relax)
# Set the ISA and ABI
add_compile_options(-march=rv32imafdvzfh_xdma -mabi=ilp32d)
# GCC path
add_compile_options(--gcc-toolchain=/scratch/matheusd/spatz-cluster/install/riscv-gcc)

##
## Link options
##
add_link_options(-static -mcmodel=small -fuse-ld=lld -nostdlib)
add_link_options(-nostartfiles)
add_link_options(-march=rv32imafdvzfh_xdma -mabi=ilp32d)
add_link_options(-ffast-math -fno-common -fno-builtin-printf)

link_libraries(-lm)
link_libraries(-lgcc)

# GCC path
add_link_options(--gcc-toolchain=/scratch/matheusd/spatz-cluster/install/riscv-gcc)

# LLD defaults to -z relro which we don't want in a static ELF
add_link_options(-Wl,--gc-sections)
#add_link_options(-Wl,--verbose)
