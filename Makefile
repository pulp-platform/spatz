# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Authors:
# Matheus Cavalcante, ETH Zurich
# Mattia Sinigaglia, University of Bologna

# Include Makefrag
include util/Makefrag

# Bender version
BENDER_VERSION = 0.29.1

# Do not include minifloat opcodes, since they conflict with the RVV opcodes!
OPCODES := "opcodes-rvv opcodes-rv32b_CUSTOM opcodes-ipu_CUSTOM opcodes-frep_CUSTOM opcodes-dma_CUSTOM opcodes-ssr_CUSTOM opcodes-smallfloat"


# Default target
all: bender toolchain update_opcodes

# Target for IIS users
init: bender update_opcodes

###############
#  Toolchain  #
###############

toolchain: download tc-llvm tc-riscv-gcc tc-riscv-isa-sim verilator

.PHONY: download
download: sw/toolchain/riscv-gnu-toolchain sw/toolchain/llvm-project sw/toolchain/riscv-opcodes sw/toolchain/verilator sw/toolchain/riscv-isa-sim sw/toolchain/dtc

sw/toolchain/riscv-gnu-toolchain: sw/toolchain/riscv-gnu-toolchain.version
	mkdir -p sw/toolchain
	cd sw/toolchain && git clone https://github.com/pulp-platform/pulp-riscv-gnu-toolchain.git riscv-gnu-toolchain
	cd sw/toolchain/riscv-gnu-toolchain &&           \
		git checkout `cat ../riscv-gnu-toolchain.version` && \
		git submodule update --init --recursive --jobs=8 .

sw/toolchain/llvm-project: sw/toolchain/llvm-project.version
	mkdir -p sw/toolchain
	cd sw/toolchain && git clone git@github.com:pulp-platform/llvm-project.git
	cd sw/toolchain/llvm-project &&                  \
		git checkout `cat ../llvm-project.version` && \
		git submodule update --init --recursive --jobs=8 .

sw/toolchain/riscv-opcodes: sw/toolchain/riscv-opcodes.version
	mkdir -p sw/toolchain
	cd sw/toolchain && git clone https://github.com/pulp-platform/riscv-opcodes.git
	cd sw/toolchain/riscv-opcodes &&                 \
		git checkout `cat ../riscv-opcodes.version` && \
		git submodule update --init --recursive --jobs=8 .

sw/toolchain/verilator: sw/toolchain/verilator.version
	mkdir -p sw/toolchain
	cd sw/toolchain && git clone https://github.com/verilator/verilator.git
	cd sw/toolchain/verilator &&                     \
		git checkout `cat ../verilator.version` && \
		git submodule update --init --recursive --jobs=8 .

sw/toolchain/riscv-isa-sim: sw/toolchain/riscv-isa-sim.version
	mkdir -p sw/toolchain
	cd sw/toolchain && git clone https://github.com/riscv-software-src/riscv-isa-sim.git
	cd sw/toolchain/riscv-isa-sim &&                 \
		git checkout `cat ../riscv-isa-sim.version` && \
		git submodule update --init --recursive --jobs=8 .

sw/toolchain/help2man:
	mkdir -p sw/toolchain/help2man
	cd sw/toolchain/help2man && wget -c https://ftp.gnu.org/gnu/help2man/help2man-1.49.3.tar.xz
	cd sw/toolchain/help2man && tar xf help2man-1.49.3.tar.xz

sw/toolchain/dtc:
	mkdir -p sw/toolchain/dtc
	cd sw/toolchain/dtc && wget -c https://git.kernel.org/pub/scm/utils/dtc/dtc.git/snapshot/dtc-1.7.0.tar.gz
	cd sw/toolchain/dtc && tar xf dtc-1.7.0.tar.gz

tc-riscv-gcc: sw/toolchain/riscv-gnu-toolchain
	mkdir -p $(GCC_INSTALL_DIR)
	cd sw/toolchain/riscv-gnu-toolchain && rm -rf build && mkdir -p build && cd build && \
	../configure --prefix=$(GCC_INSTALL_DIR) --with-arch=rv32imafd --with-abi=ilp32d --with-cmodel=medlow --enable-multilib && \
	$(MAKE) MAKEINFO=true -j4

tc-llvm: sw/toolchain/llvm-project
	mkdir -p $(LLVM_INSTALL_DIR)
	cd sw/toolchain/llvm-project && mkdir -p build && cd build; \
	$(CMAKE) \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
		-DCMAKE_CXX_COMPILER=${CXX} \
		-DCMAKE_C_COMPILER=${CC} \
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

tc-riscv-isa-sim: sw/toolchain/riscv-isa-sim sw/toolchain/dtc
	mkdir -p $(SPIKE_INSTALL_DIR)
	cd sw/toolchain/dtc/dtc-1.7.0 && make install PREFIX=$(SPIKE_INSTALL_DIR)
	cd sw/toolchain/riscv-isa-sim && rm -rf build && mkdir -p build && cd build && \
	PATH=$(SPIKE_INSTALL_DIR)/bin:$(PATH) ../configure --prefix=$(SPIKE_INSTALL_DIR) && \
	$(MAKE) MAKEINFO=true -j4 install

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
$(VERILATOR_INSTALL_DIR)/bin/verilator: sw/toolchain/verilator sw/toolchain/help2man Makefile
	cd sw/toolchain/help2man/help2man-1.49.3 && ./configure --prefix=$(VERILATOR_INSTALL_DIR) && make && make install
	cd $<; unset VERILATOR_ROOT; \
	autoconf && CC=$(CC) CXX=$(CXX) ./configure --prefix=$(VERILATOR_INSTALL_DIR) $(VERILATOR_CI) && \
	PATH=$(PATH):$(VERILATOR_INSTALL_DIR)/bin make -j4 && make install

#############
#  Opcodes  #
#############

update_opcodes: sw/toolchain/riscv-opcodes sw/toolchain/riscv-opcodes/encoding.h hw/ip/snitch/src/riscv_instr.sv
hw/ip/snitch/src/riscv_instr.sv: sw/toolchain/riscv-opcodes
	MY_OPCODES=$(OPCODES) make -C sw/toolchain/riscv-opcodes inst.sverilog
	mv sw/toolchain/riscv-opcodes/inst.sverilog $@

sw/toolchain/riscv-opcodes/encoding.h:
	MY_OPCODES=$(OPCODES) make -C sw/toolchain/riscv-opcodes all
	cp sw/toolchain/riscv-opcodes/encoding_out.h $@
