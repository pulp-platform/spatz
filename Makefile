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

# Standard opcodes
OPCODES := "rv_i" "rv64_i" "rv_m" "rv64_m" "rv_a" "rv_f" "rv_d" \
           "rv_zfh" "rv_zfhmin" "rv_d_zfhmin" "rv_zicsr" "rv_zifencei" \
           "rv_s" "rv_sdext" "rv_system" "rv_v"

# Custom extensions
OPCODES += "unratified/rv_xdma" "unratified/rv_xrrpost" \
           "unratified/rv_xsmallfloat_h" "unratified/rv_xsmallfloat_b" \
           "unratified/rv_xvfx" "unratified/rv_xvfwdotp"


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
	cd sw/toolchain && git clone https://github.com/pulp-platform/llvm-project.git
	cd sw/toolchain/llvm-project &&                  \
		git checkout `cat ../llvm-project.version` && \
		git submodule update --init --recursive --jobs=8 .

# Fetched as a release tarball from the kernel.org mirror: sourceware.org
# rate-limits git clones from CI (HTTP 429).
sw/toolchain/newlib:
	mkdir -p sw/toolchain/newlib
	cd sw/toolchain && curl -fL https://mirrors.kernel.org/sourceware/newlib/newlib-4.4.0.20231231.tar.gz | tar xz --strip-components=1 -C newlib

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
	cd sw/toolchain/help2man && curl -fLO https://ftp.gnu.org/gnu/help2man/help2man-1.49.3.tar.xz
	cd sw/toolchain/help2man && tar xf help2man-1.49.3.tar.xz

sw/toolchain/dtc:
	mkdir -p sw/toolchain/dtc
	cd sw/toolchain/dtc && curl -fLO https://git.kernel.org/pub/scm/utils/dtc/dtc.git/snapshot/dtc-1.7.0.tar.gz
	cd sw/toolchain/dtc && tar xf dtc-1.7.0.tar.gz

tc-riscv-gcc: sw/toolchain/riscv-gnu-toolchain
	mkdir -p $(GCC_INSTALL_DIR)
	cd sw/toolchain/riscv-gnu-toolchain && rm -rf build && mkdir -p build && cd build && \
	unset LIBRARY_PATH && \
	../configure --prefix=$(GCC_INSTALL_DIR) --with-arch=rv32imafd --with-abi=ilp32d --with-cmodel=medlow --enable-multilib --disable-werror && \
	sed -i 's/--disable-werror/--disable-werror --disable-gdb/g' Makefile && \
	sed -i 's/type wget/false/' ../riscv-gcc/contrib/download_prerequisites && \
	$(MAKE) MAKEINFO=true WERROR_CFLAGS="" -j4

# Builds the self-contained LLVM 22 toolchain: clang + lld, then newlib
# (libc/libm) and compiler-rt builtins for riscv32/ilp32d, all installed
# into $(LLVM_INSTALL_DIR). Mirrors llvm-project's
# .github/pulp/scripts/build-riscv32-llvm.sh — the sw build links with
# --rtlib=compiler-rt against these libraries (no external GCC toolchain).
tc-llvm: sw/toolchain/llvm-project sw/toolchain/newlib
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
	rm -rf sw/toolchain/build-newlib32 && mkdir -p sw/toolchain/build-newlib32
	cd sw/toolchain/build-newlib32 && \
	../newlib/configure \
		--target=riscv32-unknown-elf \
		--prefix=$(LLVM_INSTALL_DIR) \
		AR_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ar \
		AS_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-as \
		LD_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ld \
		RANLIB_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ranlib \
		CC_FOR_TARGET="$(LLVM_INSTALL_DIR)/bin/clang --target=riscv32 -march=rv32imafd" && \
	$(MAKE) -j8 && $(MAKE) install
	rm -rf sw/toolchain/build-compiler-rt32 && mkdir -p sw/toolchain/build-compiler-rt32
	cd sw/toolchain/build-compiler-rt32 && \
	$(CMAKE) -G"Unix Makefiles" \
		-DCMAKE_SYSTEM_NAME=Linux \
		-DCMAKE_INSTALL_PREFIX=$$($(LLVM_INSTALL_DIR)/bin/clang -print-resource-dir) \
		-DCMAKE_C_COMPILER=$(LLVM_INSTALL_DIR)/bin/clang \
		-DCMAKE_CXX_COMPILER=$(LLVM_INSTALL_DIR)/bin/clang \
		-DCMAKE_AR=$(LLVM_INSTALL_DIR)/bin/llvm-ar \
		-DCMAKE_NM=$(LLVM_INSTALL_DIR)/bin/llvm-nm \
		-DCMAKE_RANLIB=$(LLVM_INSTALL_DIR)/bin/llvm-ranlib \
		-DCMAKE_C_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_CXX_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_ASM_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_C_FLAGS="-march=rv32imafd -mabi=ilp32d" \
		-DCMAKE_CXX_FLAGS="-march=rv32imafd -mabi=ilp32d" \
		-DCMAKE_ASM_FLAGS="-march=rv32imafd -mabi=ilp32d" \
		-DCMAKE_EXE_LINKER_FLAGS="-nostartfiles -nostdlib -fuse-ld=lld" \
		-DCOMPILER_RT_BAREMETAL_BUILD=ON \
		-DCOMPILER_RT_BUILD_BUILTINS=ON \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_BUILD_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
		-DCOMPILER_RT_OS_DIR="riscv32-unknown-unknown-elf" \
		-DLLVM_CONFIG_PATH=$(LLVM_INSTALL_DIR)/bin/llvm-config \
		../llvm-project/compiler-rt && \
	$(MAKE) -j8 && $(MAKE) install
	cp "$$($(LLVM_INSTALL_DIR)/bin/clang --target=riscv32-unknown-elf -print-runtime-dir)/libclang_rt.builtins-riscv32.a" \
	   "$$($(LLVM_INSTALL_DIR)/bin/clang --target=riscv32-unknown-elf -print-runtime-dir)/libclang_rt.builtins.a"
	# --- ilp32f flavor for ELEN=32 (FLEN=32) cluster configs ---------------
	# The default flavor above is ilp32d. FLEN=32 configs (e.g.
	# spatz_cluster.32b.dram) must build D-free code with the ilp32f ABI and
	# lld refuses to mix float ABIs, so we also install ilp32f newlib +
	# compiler-rt under $(LLVM_INSTALL_DIR)/ilp32f. sw/cmake/toolchain-llvm.cmake
	# points ELEN=32 builds at these.
	rm -rf sw/toolchain/build-newlib32-ilp32f && mkdir -p sw/toolchain/build-newlib32-ilp32f
	cd sw/toolchain/build-newlib32-ilp32f && \
	../newlib/configure \
		--target=riscv32-unknown-elf \
		--prefix=$(LLVM_INSTALL_DIR)/ilp32f \
		AR_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ar \
		AS_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-as \
		LD_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ld \
		RANLIB_FOR_TARGET=$(LLVM_INSTALL_DIR)/bin/llvm-ranlib \
		CC_FOR_TARGET="$(LLVM_INSTALL_DIR)/bin/clang --target=riscv32 -march=rv32imaf -mabi=ilp32f" && \
	$(MAKE) -j8 && $(MAKE) install
	rm -rf sw/toolchain/build-compiler-rt32-ilp32f && mkdir -p sw/toolchain/build-compiler-rt32-ilp32f
	cd sw/toolchain/build-compiler-rt32-ilp32f && \
	$(CMAKE) -G"Unix Makefiles" \
		-DCMAKE_SYSTEM_NAME=Linux \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR)/ilp32f/compiler-rt \
		-DCMAKE_C_COMPILER=$(LLVM_INSTALL_DIR)/bin/clang \
		-DCMAKE_CXX_COMPILER=$(LLVM_INSTALL_DIR)/bin/clang \
		-DCMAKE_AR=$(LLVM_INSTALL_DIR)/bin/llvm-ar \
		-DCMAKE_NM=$(LLVM_INSTALL_DIR)/bin/llvm-nm \
		-DCMAKE_RANLIB=$(LLVM_INSTALL_DIR)/bin/llvm-ranlib \
		-DCMAKE_C_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_CXX_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_ASM_COMPILER_TARGET="riscv32-unknown-elf" \
		-DCMAKE_C_FLAGS="-march=rv32imaf -mabi=ilp32f" \
		-DCMAKE_CXX_FLAGS="-march=rv32imaf -mabi=ilp32f" \
		-DCMAKE_ASM_FLAGS="-march=rv32imaf -mabi=ilp32f" \
		-DCMAKE_EXE_LINKER_FLAGS="-nostartfiles -nostdlib -fuse-ld=lld" \
		-DCOMPILER_RT_BAREMETAL_BUILD=ON \
		-DCOMPILER_RT_BUILD_BUILTINS=ON \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
		-DCOMPILER_RT_BUILD_PROFILE=OFF \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF \
		-DCOMPILER_RT_BUILD_XRAY=OFF \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
		-DCOMPILER_RT_OS_DIR="riscv32-unknown-unknown-elf" \
		-DLLVM_CONFIG_PATH=$(LLVM_INSTALL_DIR)/bin/llvm-config \
		../llvm-project/compiler-rt && \
	$(MAKE) -j8 && $(MAKE) install
	mkdir -p $(LLVM_INSTALL_DIR)/ilp32f/lib
	cp "$(LLVM_INSTALL_DIR)/ilp32f/compiler-rt/lib/riscv32-unknown-unknown-elf/libclang_rt.builtins-riscv32.a" \
	   "$(LLVM_INSTALL_DIR)/ilp32f/lib/libclang_rt.builtins.a"

tc-riscv-isa-sim: sw/toolchain/riscv-isa-sim sw/toolchain/dtc
	mkdir -p $(SPIKE_INSTALL_DIR)
	cd sw/toolchain/dtc/dtc-1.7.0 && make NO_PYTHON=1 install PREFIX=$(SPIKE_INSTALL_DIR)
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
	make -C sw/toolchain/riscv-opcodes inst.sverilog EXTENSIONS='$(OPCODES)'
	mv sw/toolchain/riscv-opcodes/inst.sverilog $@

sw/toolchain/riscv-opcodes/encoding.h:
	make -C sw/toolchain/riscv-opcodes encoding.out.h EXTENSIONS='$(OPCODES)'
	cp sw/toolchain/riscv-opcodes/encoding.out.h $@
