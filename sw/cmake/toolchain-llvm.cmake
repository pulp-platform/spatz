# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Look for the precompiled binaries
set(CMAKE_C_COMPILER ${LLVM_PATH}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_PATH}/bin/clang++)
set(CMAKE_OBJCOPY ${LLVM_PATH}/bin/llvm-objcopy)
set(CMAKE_OBJDUMP ${LLVM_PATH}/bin/llvm-objdump --mcpu=snitch --mattr=a --mattr=v --mattr=m --mattr=zfh)
set(CMAKE_AR ${LLVM_PATH}/bin/llvm-ar)
set(CMAKE_STRIP ${LLVM_PATH}/bin/llvm-strip)
set(CMAKE_RANLIB ${LLVM_PATH}/bin/llvm-ranlib)

##
## Compile options
##
add_compile_options(-mcpu=snitch -mcmodel=small -ffast-math -fno-builtin-printf -fno-common -falign-loops=16)
add_compile_options(-ffunction-sections)
add_compile_options(-Wextra)
add_compile_options(-static)
add_compile_options(-mllvm -misched-topdown)
# For smallfloat we need experimental extensions enabled (Zfh)
add_compile_options(-menable-experimental-extensions)
# LLD doesn't support relaxation for RISC-V yet
add_compile_options(-mno-relax)
# Set the ISA and ABI
add_compile_options(-march=rv32imafdvzfh_xdma_xfquarter -mabi=ilp32d)
# Set the GCC path
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${GCC_PATH}")

##
## Link options
##
add_link_options(-mcpu=snitch -static -mcmodel=small -fuse-ld=lld)
add_link_options(-nostartfiles)
add_link_options(-march=rv32imafdvzfh_xdma -mabi=ilp32d)
add_link_options(-ffast-math -fno-common -fno-builtin-printf)

link_libraries(-lm)
link_libraries(-lgcc)

# LLD defaults to -z relro which we don't want in a static ELF
add_link_options(-Wl,-z,norelro)
add_link_options(-Wl,--gc-sections)
add_link_options(-Wl,--no-relax)
#add_link_options(-Wl,--verbose)
