# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Author: Matheus Cavalcante, ETH Zurich

SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
ROOT := ${SPATZ_DIR}

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))

# Include Makefrag
include $(ROOT)/util/Makefrag

# QuestaSim
VSIM      = questa-2021.3-kgf vsim
VLOG      = questa-2021.3-kgf vlog
VSIM_HOME = /usr/pack/questa--2021.3-kgf/questasim

VSIM_FLAGS += -t 1ps
VSIM_FLAGS += -voptargs=+acc
VSIM_FLAGS += -do "log -r /*; source ../script/vsim/wave.tcl; run -a"

VLOG_FLAGS += -svinputport=compat
VLOG_FLAGS += -override_timescale 1ns/1ps
VLOG_FLAGS += -suppress 2583
VLOG_FLAGS += -suppress 13314
VLOG_FLAGS += -64

all:

test: ${VSIM_SOURCES} ${TB_SRCS}
	echo ${VSIM_SOURCES}
	touch test

#################
# Prerequisites #
#################

test/bootrom.elf test/bootrom.dump test/bootrom.bin: test/bootrom.S test/bootrom.ld Makefile
	riscv -riscv64-gcc-9.5.0 riscv64-unknown-elf-gcc -mabi=ilp32d -march=rv32imafd -static -nostartfiles -Ttest/bootrom.ld $< -o test/bootrom.elf
	riscv -riscv64-gcc-9.5.0 riscv64-unknown-elf-objdump -D test/bootrom.elf > test/bootrom.dump
	riscv -riscv64-gcc-9.5.0 riscv64-unknown-elf-objcopy -O binary test/bootrom.elf test/bootrom.bin

#############
# Verilator #
#############

${VLT_BUILDDIR}/verilate: test/bootrom.bin ${VLT_SOURCES} ${TB_SRCS}
	$(call VERILATE,tb_bin)

# Quick sanity check, not really meant for simulation.
verilate: ${VLT_BUILDDIR}/verilate

## Clean all build directories and temporary files for Verilator simulation
clean.vlt:
	rm -rf ${VLT_BUILDDIR} work

############
# Modelsim #
############

${VSIM_BUILDDIR}/compile.vsim.tcl: test/bootrom.bin $(VSIM_SOURCES) ${TB_SRCS} ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc test/bootrom.bin
	vlib $(dir $@)
	${BENDER} script vsim ${VSIM_BENDER} ${DEFS} --vlog-arg="${VLOG_FLAGS} -work $(dir $@) " > $@
	echo '${VLOG} -work $(dir $@) ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc -ccflags "-std=c++14 -I${MKFILE_DIR}/test -I${MKFILE_DIR}/work/include -I${TB_DIR}"' >> $@
	echo '${VLOG} -work $(dir $@) test/uartdpi/uartdpi.c -ccflags "-Itest/uartdpi"' >> $@
	echo 'return 0' >> $@

bin/spatz_cluster.vsim: ${VSIM_BUILDDIR}/compile.vsim.tcl work/lib/libfesvr.a
	$(call QUESTASIM,tb_bin)

clean.vsim:
	rm -rf bin/spatz_cluster.vsim bin/spatz_cluster.vsim.gui work-vsim work vsim.wlf

#######
# VCS #
#######
# @IIS: vcs-2020.12 make bin/spatz_cluster.vcs
## Build compilation script and compile all sources for VCS simulation
bin/spatz_cluster.vcs: test/bootrom.bin work-vcs/compile.sh work/lib/libfesvr.a ${TB_DIR}/common_lib.cc test/bootdata.cc test/bootrom.bin test/uartdpi/uartdpi.c
	mkdir -p bin
	vcs -Mlib=work-vcs -Mdir=work-vcs -debug_access+all -fgp -kdb +vcs+fsdbon -o bin/spatz_cluster.vcs -cc $(CC) -cpp $(CXX) \
		-assert disable_cover -override_timescale=1ns/1ps -full64 tb_bin ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc test/uartdpi/uartdpi.c \
		-CFLAGS "-std=c++14 -I${MKFILE_DIR} -I${MKFILE_DIR}/test -I${FESVR}/include -I${TB_DIR} -Itest/uartdpi" -LDFLAGS "-L${FESVR}/lib" -lfesvr -lutil

## Clean all build directories and temporary files for VCS simulation
clean.vcs:
	rm -rf bin/spatz_cluster.vcs work-vcs work

######
# SW #
######

## Build SW into sw/build with the LLVM toolchain
sw: clean.sw
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DCMAKE_TOOLCHAIN_FILE=toolchain-llvm .. -DLLVM_DIR=${LLVM_INSTALL_DIR} && make

# VSIM
## Build SW into sw/build with the LLVM toolchain (including tests) for Questasim simulator
sw.vsim: clean.sw bin/spatz_cluster.vsim
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DCMAKE_TOOLCHAIN_FILE=toolchain-llvm -DLLVM_DIR=${LLVM_INSTALL_DIR} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vsim -DBUILD_TESTS=ON .. && make

## Build SW and run all tests with Questasim simulator
sw.test.vsim: sw.vsim
	cd sw/build && make test

# VCS
## Build SW into sw/build with the LLVM toolchain (including tests) for VCS simulator
sw.vcs: clean.sw bin/spatz_cluster.vcs
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DCMAKE_TOOLCHAIN_FILE=toolchain-llvm -DLLVM_DIR=${LLVM_INSTALL_DIR} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vcs -DBUILD_TESTS=ON .. && make

## Build SW and run all tests with VCS simulator
sw.test.vcs: sw.vcs
	cd sw/build && make test

# VLT
## Build SW into sw/build with the LLVM toolchain (including tests) for Verilator simulator
sw.vlt: clean.sw bin/spatz_cluster.vlt
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DCMAKE_TOOLCHAIN_FILE=toolchain-llvm -DLLVM_DIR=${LLVM_INSTALL_DIR} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vlt -DBUILD_TESTS=ON .. && make

## Build SW and run all tests with Verilator simulator
sw.test.vlt: sw.vlt
	cd sw/build && make test

## Delete sw/build
clean.sw:
	rm -rf sw/build

########
# Util #
########

## Clean everything except logs
clean: clean.vcs clean.vlt clean.vsim clean.sw
	rm -rf Bender.lock .bender/ work/

clean.logs:
	rm -rf logs/
