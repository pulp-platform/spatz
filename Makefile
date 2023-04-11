# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante, ETH Zurich

# Include Makefrag
include util/Makefrag

# Bender version
BENDER_VERSION = 0.27.1

# Do not include minifloat opcodes, since they conflict with the RVV opcodes!
OPCODES := "opcodes-rvv opcodes-rv32b_CUSTOM opcodes-ipu_CUSTOM opcodes-frep_CUSTOM opcodes-dma_CUSTOM opcodes-ssr_CUSTOM opcodes-smallfloat"

# Default target
all: bender toolchain update_opcodes

###############
#  Toolchain  #
###############

toolchain: download tc-llvm tc-riscv-gcc verilator

.PHONY: download
download: $(CURDIR)/sw/toolchain/riscv-gnu-toolchain $(CURDIR)/sw/toolchain/llvm-project $(CURDIR)/sw/toolchain/riscv-opcodes $(CURDIR)/sw/toolchain/verilator $(CURDIR)/sw/toolchain/riscv-isa-sim

$(CURDIR)/sw/toolchain/riscv-gnu-toolchain:
	mkdir -p $(CURDIR)/sw/toolchain
	cd $(CURDIR)/sw/toolchain && git clone https://github.com/pulp-platform/pulp-riscv-gnu-toolchain.git riscv-gnu-toolchain
	cd $(CURDIR)/sw/toolchain/riscv-gnu-toolchain &&           \
		git checkout 70acebe256fc49114b5f068fa79f03eb9affed09 && \
		git submodule update --init --recursive --jobs=8 .

$(CURDIR)/sw/toolchain/llvm-project:
	mkdir -p $(CURDIR)/sw/toolchain
	cd $(CURDIR)/sw/toolchain && git clone git@github.com:pulp-platform/llvm-project.git
	cd $(CURDIR)/sw/toolchain/llvm-project &&                  \
		git checkout fe1298fc0c84a23dde8c5e22d3cc84defad724d0 && \
		git submodule update --init --recursive --jobs=8 .

$(CURDIR)/sw/toolchain/riscv-opcodes:
	mkdir -p $(CURDIR)/sw/toolchain
	cd $(CURDIR)/sw/toolchain && git clone https://github.com/pulp-platform/riscv-opcodes.git
	cd $(CURDIR)/sw/toolchain/riscv-opcodes &&                 \
		git checkout e46a55a13117db225749a6064f9308eae9ae541d && \
		git submodule update --init --recursive --jobs=8 .

$(CURDIR)/sw/toolchain/verilator:
	mkdir -p $(CURDIR)/sw/toolchain
	cd $(CURDIR)/sw/toolchain && git clone https://github.com/verilator/verilator.git
	cd $(CURDIR)/sw/toolchain/verilator &&                     \
		git checkout fff0eb5d88c851496f05e6368e164dfbc9c2f5ed && \
		git submodule update --init --recursive --jobs=8 .

$(CURDIR)/sw/toolchain/riscv-isa-sim:
	mkdir -p $(CURDIR)/sw/toolchain
	cd $(CURDIR)/sw/toolchain && git clone https://github.com/riscv-software-src/riscv-isa-sim.git
	cd $(CURDIR)/sw/toolchain/riscv-isa-sim &&                 \
		git checkout a0972c82d022f6f7c337b06b27c89a60af52202a && \
		git submodule update --init --recursive --jobs=8 .

tc-riscv-gcc:
	mkdir -p $(GCC_INSTALL_DIR)
	cd $(CURDIR)/sw/toolchain/riscv-gnu-toolchain && rm -rf build && mkdir -p build && cd build && \
	../configure --prefix=$(GCC_INSTALL_DIR) --with-arch=rv32imafd --with-abi=ilp32d --with-cmodel=medlow --enable-multilib && \
	$(MAKE) MAKEINFO=true -j4

tc-llvm:
	mkdir -p $(LLVM_INSTALL_DIR)
	cd $(CURDIR)/sw/toolchain/llvm-project && mkdir -p build && cd build; \
	$(CMAKE) \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DCMAKE_C_COMPILER=clang \
		-DLLVM_OPTIMIZED_TABLEGEN=True \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_TARGETS_TO_BUILD="RISCV" \
		-DLLVM_DEFAULT_TARGET_TRIPLE=riscv32-unknown-elf \
		-DLLVM_ENABLE_LLD=False \
		-DLLVM_APPEND_VC_REV=ON \
		-DCMAKE_BUILD_TYPE=Release \
		../llvm && \
	make -j8 all && \
	make install

############
#  Bender  #
############

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

###############
#  Verilator  #
###############

verilator: $(VERILATOR_INSTALL_DIR)/bin/verilator
$(VERILATOR_INSTALL_DIR)/bin/verilator: sw/toolchain/verilator Makefile
	cd $<; unset VERILATOR_ROOT; \
	autoconf && CC=$(CLANG_CC) CXX=$(CLANG_CXX) CXXFLAGS=$(CLANG_CXXFLAGS) LDFLAGS=$(CLANG_LDFLAGS) ./configure --prefix=$(VERILATOR_INSTALL_DIR) $(VERILATOR_CI) && \
	make -j4 && make install

#############
#  Opcodes  #
#############

update_opcodes: sw/snRuntime/vendor/riscv-opcodes/encoding.h hw/ip/snitch/src/riscv_instr.sv
hw/ip/snitch/src/riscv_instr.sv: sw/toolchain/riscv-opcodes/*
	MY_OPCODES=$(OPCODES) make -C sw/toolchain/riscv-opcodes inst.sverilog
	mv sw/toolchain/riscv-opcodes/inst.sverilog $@

sw/snRuntime/vendor/riscv-opcodes/encoding.h: sw/toolchain/riscv-opcodes/*
	MY_OPCODES=$(OPCODES) make -C sw/toolchain/riscv-opcodes all
	cp sw/toolchain/riscv-opcodes/encoding_out.h $@
