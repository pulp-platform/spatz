# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)

INSTALL_PREFIX        ?= install
INSTALL_DIR           ?= ${ROOT_DIR}/${INSTALL_PREFIX}
GCC_INSTALL_DIR       ?= ${INSTALL_DIR}/riscv-gcc
ISA_SIM_INSTALL_DIR   ?= ${INSTALL_DIR}/riscv-isa-sim
LLVM_INSTALL_DIR      ?= ${INSTALL_DIR}/llvm
BENDER_INSTALL_DIR    ?= ${INSTALL_DIR}/bender
VERILATOR_INSTALL_DIR ?= ${INSTALL_DIR}/verilator
RISCV_TESTS_DIR       ?= ${ROOT_DIR}/${SOFTWARE_DIR}/riscv-tests

CMAKE ?= cmake
# CC and CXX are Makefile default variables that are always defined in a Makefile. Hence, overwrite
# the variable if it is only defined by the Makefile (its origin in the Makefile's default).
ifeq ($(origin CC),default)
  CC  = gcc
endif
ifeq ($(origin CXX),default)
  CXX = g++
endif
BENDER_VERSION = 0.27.1

# We need a recent LLVM installation (>11) to compile Verilator.
# We also need to link the binaries with LLVM's libc++.
# Define CLANG_PATH to be the path of your Clang installation.
# At IIS, check .gitlab/.gitlab-ci.yml for an example CLANG_PATH.

ifneq (${CLANG_PATH},)
  CLANG_CC       := $(CLANG_PATH)/bin/clang
  CLANG_CXX      := $(CLANG_PATH)/bin/clang++
  CLANG_CXXFLAGS := "-nostdinc++ -isystem $(CLANG_PATH)/include/c++/v1"
  CLANG_LDFLAGS  := "-L $(CLANG_PATH)/lib -Wl,-rpath,$(CLANG_PATH)/lib -lc++"
else
  CLANG_CC       ?= clang
  CLANG_CXX      ?= clang++
  CLANG_CXXFLAGS := ""
  CLANG_LDFLAGS  := ""
endif

# Do not include minifloat opcodes, since they conflict with the RVV opcodes!
OPCODES := "opcodes-rvv opcodes-rv32b_CUSTOM opcodes-ipu_CUSTOM opcodes-frep_CUSTOM opcodes-dma_CUSTOM opcodes-ssr_CUSTOM opcodes-smallfloat"

# Default target
all: bender verilator toolchain update_opcodes

# Toolchain
toolchain: tc-riscv-gcc tc-llvm

tc-riscv-gcc:
	mkdir -p $(GCC_INSTALL_DIR)
	cd $(CURDIR)/sw/toolchain/riscv-gnu-toolchain && rm -rf build && mkdir -p build && cd build && \
	../configure --prefix=$(GCC_INSTALL_DIR) --with-arch=rv32im --with-cmodel=medlow --enable-multilib && \
	$(MAKE) MAKEINFO=true -j4

tc-llvm:
	mkdir -p $(LLVM_INSTALL_DIR)
	cd $(CURDIR)/sw/toolchain/llvm-project && mkdir -p build && cd build; \
	$(CMAKE) \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_C_COMPILER=$(CC) \
		-DLLVM_OPTIMIZED_TABLEGEN=True \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_TARGETS_TO_BUILD="RISCV;host" \
		-DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-elf" \
		-DLLVM_ENABLE_LLD=False \
		-DLLVM_APPEND_VC_REV=ON \
		-DCMAKE_BUILD_TYPE=Release \
		../llvm && \
	make -j4 all && \
	make install

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

# Verilator
verilator: $(VERILATOR_INSTALL_DIR)/bin/verilator
$(VERILATOR_INSTALL_DIR)/bin/verilator: sw/toolchain/verilator Makefile
	cd $<; unset VERILATOR_ROOT; \
	autoconf && CC=$(CLANG_CC) CXX=$(CLANG_CXX) CXXFLAGS=$(CLANG_CXXFLAGS) LDFLAGS=$(CLANG_LDFLAGS) ./configure --prefix=$(VERILATOR_INSTALL_DIR) $(VERILATOR_CI) && \
	make -j4 && make install

update_opcodes: hw/snitch/src/riscv_instr.sv
hw/snitch/src/riscv_instr.sv: sw/toolchain/riscv-opcodes/*
	MY_OPCODES=$(OPCODES) make -C sw/toolchain/riscv-opcodes inst.sverilog
	mv sw/toolchain/riscv-opcodes/inst.sverilog $@
