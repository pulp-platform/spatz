# Copyright 2026 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# The Spatz ISA: RV32 IMAFD + Zfh + V, plus the Spatz vendor extensions
# generated from riscv-opcodes (see pulp-llvm-extensions.txt).
#
# 'v' is DELIBERATELY absent from the default C/C++ march: Spatz does not
# implement every RVV instruction, and LLVM 22's backend uses RVV in many
# lowerings (store merging, f64-from-parts via vslide1down, ...) that no
# vectorizer flag controls. Without 'v' the compiler cannot emit vector
# code at all. Components whose C files contain hand-written vector inline
# asm (spatzBenchmarks kernels, riscvTests) opt into SPATZ_MARCH_VECTOR in
# their own CMakeLists (the last -march on the command line wins), guarded
# by the vectorizer/store-merging/vector-combine kill switches below.
# Assembly files (.S) always get the full march: the assembler only
# encodes what is written, it never generates vector code on its own.
# ELEN=32 configs (e.g. spatz_cluster.32b.dram: FLEN=32, no D in hardware)
# must not see 'd' in the march at all: under ilp32d LLVM 22 saves
# callee-saved FPRs with fsd even in float-only functions (old compilers
# happened not to allocate them), which traps on FLEN=32 cores. So these
# configs build D-free code with the ilp32f ABI and link against the
# toolchain's ilp32f newlib/compiler-rt flavor (installed under
# <llvm>/ilp32f by tc-llvm; lld refuses to mix float ABIs at link time).
# 'v' implies zve64d (needs D), so the ASM march uses zve32f instead;
# mnemonic acceptance for the EEW<=32 kernels is identical.
if (ELEN EQUAL 32)
  set(SPATZ_MARCH_SCALAR rv32imaf_zfh_xdma_xsmallfloatb_xsmallfloath_xrrpost_xvfx_xvfwdotp)
  set(SPATZ_MARCH_VECTOR rv32imaf_zve32f_zfh_xdma_xsmallfloatb_xsmallfloath_xrrpost_xvfx_xvfwdotp)
  set(SPATZ_MABI ilp32f)
else()
  set(SPATZ_MARCH_SCALAR rv32imafd_zfh_xdma_xsmallfloatb_xsmallfloath_xrrpost_xvfx_xvfwdotp)
  set(SPATZ_MARCH_VECTOR rv32imafdv_zfh_xdma_xsmallfloatb_xsmallfloath_xrrpost_xvfx_xvfwdotp)
  set(SPATZ_MABI ilp32d)
endif()
# Encode-everything march for riscvTests. Their rv64uv sources contain
# hand-written EEW=64 vector asm, not all of it behind `#if ELEN==64`, so
# the restricted default march (v-less, or zve32f on ELEN=32) rejects it at
# assembly time. The pre-bump toolchain gave EVERY config the full
# rv32imafdvzfh march by default, so these assembled on 32b too and CI runs
# riscvTests on 32b; this simply restores that ENCODING capability for
# riscvTests. The ABI still follows SPATZ_MABI (ilp32f on ELEN=32), so no
# fsd is emitted for compiler-generated code, and the tests' own
# `#if ELEN==64` guards keep per-config execution identical to main.
set(SPATZ_MARCH_ENCODE_ALL rv32imafdv_zfh_xdma_xsmallfloatb_xsmallfloath_xrrpost_xvfx_xvfwdotp)

# Look for the precompiled binaries
set(CMAKE_C_COMPILER ${LLVM_PATH}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_PATH}/bin/clang++)
set(CMAKE_OBJCOPY ${LLVM_PATH}/bin/llvm-objcopy)
set(CMAKE_OBJDUMP ${LLVM_PATH}/bin/llvm-objdump --mattr=+m,+a,+f,+d,+v,+zfh,+xdma,+xsmallfloatb,+xsmallfloath,+xrrpost,+xvfx,+xvfwdotp)
set(CMAKE_AR ${LLVM_PATH}/bin/llvm-ar)
set(CMAKE_STRIP ${LLVM_PATH}/bin/llvm-strip)
set(CMAKE_RANLIB ${LLVM_PATH}/bin/llvm-ranlib)

##
## Compile options
##
# -mcpu=snitch and -mllvm -misched-topdown were options of the LLVM 12
# based toolchain and no longer exist in LLVM 22.
add_compile_options(-mcmodel=small -ffast-math -fno-builtin-printf -fno-common -falign-loops=16)
add_compile_options(-ffunction-sections)
add_compile_options(-Wextra)
add_compile_options(-static)
# LLD doesn't support relaxation for RISC-V yet
add_compile_options(-mno-relax)
# The compiler must never use FP/vector registers the code did not ask
# for. This is the load-bearing switch for the V-march components: without
# it LLVM 22 expands memcpy/memset inline with RVV (vsetivli/vle/vse and
# whole-register spills), clobbering the vector state that hand-written
# kernels and the riscvTests set up around it. Explicit FP arithmetic is
# unaffected.
add_compile_options(-mno-implicit-float)
# Belt and braces on top of the v-less march: keep the vectorizers off too
# (same rationale as snitch_cluster commit f6a81529). SHELL: keeps the
# "-mllvm <option>" pair atomic; cmake would otherwise de-duplicate a
# repeated -mllvm and pass a bare backend option to clang.
add_compile_options(-fno-vectorize -fno-slp-vectorize)
add_compile_options("SHELL:-mllvm -scalable-vectorization=off")
# For the V-march components: stop the DAG combiner and VectorCombine from
# forming vector code out of scalar sources (observed: vsetivli/vsext.vf4/
# vse32.v materializing stack arguments via consecutive-store merging).
add_compile_options("SHELL:-mllvm -combiner-store-merging=false")
add_compile_options("SHELL:-mllvm -disable-vector-combine")
# Set the ISA and ABI (see SPATZ_MARCH_* above for the C-vs-ASM split)
add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-march=${SPATZ_MARCH_SCALAR}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:-march=${SPATZ_MARCH_VECTOR}>")
add_compile_options(-mabi=${SPATZ_MABI})
# The LLVM 22 toolchain is self-contained (own newlib and compiler-rt built
# for ilp32d); an external GCC toolchain is no longer needed. The gcc pack's
# libgcc multilib selection also cannot match a vendor-extended -march.

##
## Link options
##
add_link_options(-static -mcmodel=small -fuse-ld=lld)
add_link_options(-nostartfiles)
add_link_options(-march=${SPATZ_MARCH_SCALAR} -mabi=${SPATZ_MABI})
add_link_options(-ffast-math -fno-common -fno-builtin-printf)
# Builtins (division, clz, ...) come from compiler-rt instead of libgcc.
add_link_options(--rtlib=compiler-rt)

link_libraries(-lm)

# ELEN=32: resolve libc/libm from the ilp32f newlib flavor (-L order beats
# the driver's default ilp32d search path) and satisfy builtins from the
# ilp32f compiler-rt archive. The driver's own --rtlib path still appears
# at the end of the link line, but lazy archive extraction means it
# contributes nothing once every builtin is already resolved, so the
# float-ABI check never sees an ilp32d member.
if (ELEN EQUAL 32)
  add_link_options(-L${LLVM_PATH}/ilp32f/riscv32-unknown-elf/lib)
  link_libraries(${LLVM_PATH}/ilp32f/lib/libclang_rt.builtins.a)
endif()

# LLD defaults to -z relro which we don't want in a static ELF
add_link_options(-Wl,-z,norelro)
add_link_options(-Wl,--gc-sections)
add_link_options(-Wl,--no-relax)
#add_link_options(-Wl,--verbose)
