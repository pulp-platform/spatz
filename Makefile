# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Domenic Wüthrich, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)

INSTALL_PREFIX      ?= install
INSTALL_DIR         ?= ${ROOT_DIR}/${INSTALL_PREFIX}
GCC_INSTALL_DIR     ?= ${INSTALL_DIR}/riscv-gcc
LLVM_INSTALL_DIR    ?= ${INSTALL_DIR}/riscv-llvm
ISA_SIM_INSTALL_DIR ?= ${INSTALL_DIR}/riscv-isa-sim
BENDER_INSTALL_DIR  ?= ${INSTALL_DIR}/bender

BENDER_VERSION = 0.22.0

CMAKE ?= cmake-3.18.1

# CC and CXX are Makefile default variables that are always defined in a Makefile. Hence, overwrite
# the variable if it is only defined by the Makefile (its origin in the Makefile's default).
ifeq ($(origin CC),default)
CC     = gcc-8.2.0
endif
ifeq ($(origin CXX),default)
CXX    = g++-8.2.0
endif

# We need a recent LLVM to compile Verilator
CLANG_CC  ?= clang
CLANG_CXX ?= clang++
ifneq (${CLANG_PATH},)
	CLANG_CXXFLAGS := "-nostdinc++ -isystem $(CLANG_PATH)/include/c++/v1"
	CLANG_LDFLAGS  := "-L $(CLANG_PATH)/lib -Wl,-rpath,$(CLANG_PATH)/lib -lc++ -nostdlib++"
else
	CLANG_CXXFLAGS := ""
	CLANG_LDFLAGS  := ""
endif

# Default target
all: toolchain riscv-isa-sim

# GCC and LLVM Toolchains
.PHONY: toolchain toolchain-gcc toolchain-llvm toolchain-llvm-main toolchain-llvm-newlib toolchain-llvm-rt
toolchain:  toolchain-gcc toolchain-llvm

toolchain-llvm: toolchain-llvm-main toolchain-llvm-newlib toolchain-llvm-rt

toolchain-gcc: Makefile
	mkdir -p $(GCC_INSTALL_DIR)
	# Apply patch on riscv-binutils
	cd $(CURDIR)/toolchain/riscv-gnu-toolchain/riscv-binutils
	cd $(CURDIR)/toolchain/riscv-gnu-toolchain && rm -rf build && mkdir -p build && cd build && \
	CC=$(CC) CXX=$(CXX) ../configure --prefix=$(GCC_INSTALL_DIR) --with-arch=rv32im --with-cmodel=medlow --enable-multilib && \
	$(MAKE) MAKEINFO=true -j4

toolchain-llvm-main: Makefile
	mkdir -p $(LLVM_INSTALL_DIR)
	cd $(ROOT_DIR)/toolchain/riscv-llvm && rm -rf build && mkdir -p build && cd build && \
	$(CMAKE) -G Ninja \
	-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
	-DLLVM_ENABLE_PROJECTS="clang;lld" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=$(CC) \
	-DCMAKE_CXX_COMPILER=$(CXX) \
	-DLLVM_DEFAULT_TARGET_TRIPLE=riscv32-unknown-elf \
	-DLLVM_TARGETS_TO_BUILD="RISCV" \
	../llvm
	cd $(ROOT_DIR)/toolchain/riscv-llvm && \
	$(CMAKE) --build build --target install

toolchain-llvm-newlib: Makefile
	cd ${ROOT_DIR}/toolchain/newlib && rm -rf build && mkdir -p build && cd build && \
	../configure --prefix=${LLVM_INSTALL_DIR} \
	--target=riscv32-unknown-elf \
	CC_FOR_TARGET="${LLVM_INSTALL_DIR}/bin/clang -march=rv32im -mabi=ilp32 -mno-relax -mcmodel=medlow" \
	AS_FOR_TARGET=${LLVM_INSTALL_DIR}/bin/llvm-as \
	AR_FOR_TARGET=${LLVM_INSTALL_DIR}/bin/llvm-ar \
	LD_FOR_TARGET=${LLVM_INSTALL_DIR}/bin/llvm-ld \
	RANLIB_FOR_TARGET=${LLVM_INSTALL_DIR}/bin/llvm-ranlib && \
	make && \
	make install

toolchain-llvm-rt: Makefile toolchain-llvm-main toolchain-llvm-newlib
	cd $(ROOT_DIR)/toolchain/riscv-llvm/compiler-rt && rm -rf build && mkdir -p build && cd build && \
	$(CMAKE) $(ROOT_DIR)/toolchain/riscv-llvm/compiler-rt -G Ninja \
	-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
	-DCMAKE_C_COMPILER_TARGET="riscv32-unknown-elf" \
	-DCMAKE_ASM_COMPILER_TARGET="riscv32-unknown-elf" \
	-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
	-DCOMPILER_RT_BAREMETAL_BUILD=ON \
	-DCOMPILER_RT_BUILD_BUILTINS=ON \
	-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
	-DCOMPILER_RT_BUILD_MEMPROF=OFF \
	-DCOMPILER_RT_BUILD_PROFILE=OFF \
	-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
	-DCOMPILER_RT_BUILD_XRAY=OFF \
	-DCMAKE_C_COMPILER_WORKS=1 \
	-DCMAKE_CXX_COMPILER_WORKS=1 \
	-DCMAKE_SIZEOF_VOID_P=4 \
	-DCMAKE_C_COMPILER="$(LLVM_INSTALL_DIR)/bin/clang" \
	-DCMAKE_C_FLAGS="-march=rv32im -mabi=ilp32 -mno-relax -mcmodel=medany" \
	-DCMAKE_ASM_FLAGS="-march=rv32im -mabi=ilp32 -mno-relax -mcmodel=medany" \
	-DCMAKE_AR=$(LLVM_INSTALL_DIR)/bin/llvm-ar \
	-DCMAKE_NM=$(LLVM_INSTALL_DIR)/bin/llvm-nm \
	-DCMAKE_RANLIB=$(LLVM_INSTALL_DIR)/bin/llvm-ranlib \
	-DLLVM_CONFIG_PATH=$(LLVM_INSTALL_DIR)/bin/llvm-config
	cd $(ROOT_DIR)/toolchain/riscv-llvm/compiler-rt && \
	$(CMAKE) --build build --target install && \
	ln -s $(LLVM_INSTALL_DIR)/lib/linux $(LLVM_INSTALL_DIR)/lib/clang/13.0.0/lib

.PHONY: riscv-isa-sim
riscv-isa-sim: ${ISA_SIM_INSTALL_DIR}

${ISA_SIM_INSTALL_DIR}: Makefile
	# There are linking issues with the standard libraries when using newer CC/CXX versions to compile Spike.
	# Therefore, here we resort to older versions of the compilers.
	cd toolchain/riscv-isa-sim && mkdir -p build && cd build; \
	[ -d dtc ] || git clone https://git.kernel.org/pub/scm/utils/dtc/dtc.git && cd dtc; \
	make install SETUP_PREFIX=$(ISA_SIM_INSTALL_DIR) PREFIX=$(ISA_SIM_INSTALL_DIR) && \
	PATH=$(ISA_SIM_INSTALL_DIR)/bin:$$PATH; cd ..; \
	../configure --prefix=$(ISA_SIM_INSTALL_DIR) && make -j4 && make install

# Bender
bender: check-bender
check-bender:
	@if [ -x $(BENDER_INSTALL_DIR)/bender ]; then \
		req="bender $(BENDER_VERSION)"; \
		current="$$($(BENDER_INSTALL_DIR)/bender --version)"; \
		if [ "$$(printf '%s\n' "$${req}" "$${current}" | sort -V | head -n1)" != "$${req}" ]; then \
			rm -rf $(BENDER_INSTALL_DIR); \
		fi \
	fi
	@$(MAKE) -C $(ROOT_DIR) $(BENDER_INSTALL_DIR)/bender

$(BENDER_INSTALL_DIR)/bender:
	mkdir -p $(BENDER_INSTALL_DIR) && cd $(BENDER_INSTALL_DIR) && \
	curl --proto '=https' --tlsv1.2 https://pulp-platform.github.io/bender/init -sSf | sh -s -- $(BENDER_VERSION)

# Helper targets
.PHONY: clean

clean:
	rm -rf $(INSTALL_DIR)
