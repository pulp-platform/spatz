# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Author: Matheus Cavalcante, ETH Zurich

SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
ROOT := ${SPATZ_DIR}
SPATZ_CLUSTER_DIR := ${SPATZ_DIR}/hw/system/spatz_cluster
MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))

# Configuration file
SPATZ_CLUSTER_CFG         ?= $(SPATZ_CLUSTER_DIR)/cfg/spatz_cluster.default.hjson
SPATZ_CLUSTER_CFG_DEFINES += -DMEM_DRAM_ORIGIN=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['dram']['address'])")
SPATZ_CLUSTER_CFG_DEFINES += -DMEM_DRAM_SIZE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['dram']['length'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_BASE_HARTID=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['cluster']['cluster_base_hartid'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_CLUSTER_CORE_NUM=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(len(jstyleson.load(f)['cluster']['cores']))")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_TCDM_START_ADDR=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['cluster']['cluster_base_addr'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_CLUSTER_OFFSET=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['cluster']['cluster_base_offset'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_TCDM_SIZE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['cluster']['tcdm']['size'] * 1024)")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_NFPU_PER_CORE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG)'); print(jstyleson.load(f)['cluster']['n_fpu'])")

# Include Makefrag
include $(ROOT)/util/Makefrag

# QuestaSim
VSIM      = questa-2021.3-kgf vsim
VLOG      = questa-2021.3-kgf vlog
VSIM_HOME = /usr/pack/questa-2021.3-kgf/questasim

VSIM_FLAGS += -t 1ps
VSIM_FLAGS += -do "log -r /*; source ${SPATZ_CLUSTER_DIR}/script/vsim/wave.tcl; run -a"

# Fast simulation for command line simulation
VSIM_FLAGS_SHELL = $(VSIM_FLAGS) -voptargs=-O5
# Full signal visibility for GUI simulation
VSIM_FLAGS_GUI   = $(VSIM_FLAGS) -voptargs=+acc

VLOG_FLAGS += -svinputport=compat
VLOG_FLAGS += -override_timescale 1ns/1ps
VLOG_FLAGS += -suppress 2583
VLOG_FLAGS += -suppress 13314
VLOG_FLAGS += -64

# Verilated and compiled Spatz system
VLT_ROOT = ${VERILATOR_INSTALL_DIR}/share/verilator
VLT_AR   = ${VLT_BUILDDIR}/Vtestharness__ALL.a

all:

test: ${VSIM_SOURCES} ${TB_SRCS}
	echo ${VSIM_SOURCES}
	touch test

###############
# C testbench #
###############

# Required C sources for the verilator TB that are linked against the verilated model
VLT_COBJ  = $(VLT_BUILDDIR)/tb/common_lib.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/verilator_lib.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/tb_bin.o
VLT_COBJ += $(VLT_BUILDDIR)/test/uartdpi/uartdpi.o
VLT_COBJ += $(VLT_BUILDDIR)/test/bootdata.o
# Sources from verilator root
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_timing.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_threads.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_dpi.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_vcd_c.o

#################
# Prerequisites #
#################

test/bootrom.elf test/bootrom.dump test/bootrom.bin: src/generated/spatz_cluster_wrapper.sv test/bootrom.S test/bootrom.ld test/bootdata_bootrom.cc Makefile
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-gcc -mabi=ilp32 -march=rv32imaf -static -nostartfiles -Ttest/bootrom.ld test/bootrom.S test/bootdata_bootrom.cc -I../../ip/snitch_test/src -I../../../sw/snRuntime/include -I../../../sw/toolchain/riscv-opcodes/ -o test/bootrom.elf
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-objdump -D test/bootrom.elf > test/bootrom.dump
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-objcopy -O binary test/bootrom.elf test/bootrom.bin

.PHONY: bootrom
bootrom: $(CURDIR)/src/generated/bootrom.sv
$(CURDIR)/src/generated/bootrom.sv: test/bootrom.bin
	${PYTHON} $(SPATZ_DIR)/util/generate_bootrom.py test/bootrom.bin -c ${SPATZ_CLUSTER_CFG} --output src/generated/bootrom.sv

.PHONY: generate
generate: src/generated/spatz_cluster_wrapper.sv
src/generated/spatz_cluster_wrapper.sv: ${SPATZ_CLUSTER_CFG} $(find src/tpl) $(find test/*tpl)
	${PYTHON} ${SPATZ_DIR}/util/clustergen.py -c ${SPATZ_CLUSTER_CFG} -o src/

#############
# Verilator #
#############

${VLT_AR}: ${VLT_SOURCES} ${TB_SRCS}
	$(call VERILATE,testharness)

# Quick sanity check, not really meant for simulation.
verilate: ${VLT_AR}

# Build targets for verilator TB
$(VLT_BUILDDIR)/tb/%.o: $(TB_DIR)/%.cc $(VLT_AR) ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	${CXX} $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@
$(VLT_BUILDDIR)/vlt/%.o: $(VLT_ROOT)/include/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@
$(VLT_BUILDDIR)/test/%.o: test/%.cc ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@

$(VLT_BUILDDIR)/test/uartdpi/uartdpi.o: test/uartdpi/uartdpi.c
	mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@

# Link verilated archive wich $(VLT_COBJ)
bin/spatz_cluster.vlt: $(VLT_AR) $(VLT_COBJ) ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -L ${VLT_BUILDDIR}/lib -o $@ $(VLT_COBJ) $(VLT_AR) -lpthread -lfesvr -lutil -latomic

# Clean all build directories and temporary files for Verilator simulation
.PHONY: clean.vlt
clean.vlt:
	rm -rf work-vlt
	rm -f bin/spatz_cluster.vlt

############
# Modelsim #
############

${VSIM_BUILDDIR}/compile.vsim.tcl: test/bootrom.bin $(VSIM_SOURCES) ${TB_SRCS} ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc test/bootrom.bin
	vlib $(dir $@)
	${BENDER} script vsim ${VSIM_BENDER} ${DEFS} --vlog-arg="${VLOG_FLAGS} -work $(dir $@) " > $@
	echo '${VLOG} -work $(dir $@) ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc -ccflags "-std=c++17 -I${MKFILE_DIR}/test -I${MKFILE_DIR}/work/include -I${TB_DIR}"' >> $@
	echo '${VLOG} -work $(dir $@) test/uartdpi/uartdpi.c -ccflags "-Itest/uartdpi"' >> $@
	echo 'return 0' >> $@

bin/spatz_cluster.vsim: ${VSIM_BUILDDIR}/compile.vsim.tcl work/lib/libfesvr_vsim.a
	mkdir -p bin/logs
	$(call QUESTASIM,tb_bin)

clean.vsim:
	rm -rf bin/spatz_cluster.vsim bin/spatz_cluster.vsim.gui work-vsim work vsim.wlf vish_stacktrace.vstf transcript

#######
# VCS #
#######
# @IIS: vcs-2020.12 make bin/spatz_cluster.vcs
## Build compilation script and compile all sources for VCS simulation
bin/spatz_cluster.vcs: test/bootrom.bin work-vcs/compile.sh work/lib/libfesvr_vcs.a ${TB_DIR}/common_lib.cc test/bootdata.cc test/bootrom.bin test/uartdpi/uartdpi.c
	mkdir -p bin
	# Default to fast simulation flags, use `-debug_access+all +vcs+fsdbon` for waveform debugging
	vcs -Mlib=work-vcs -Mdir=work-vcs -O2 -debug_access=r -debug_region=1,tb_bin -kdb -o $@ -j4 -cc $(CC) -cpp $(CXX) \
		-assert disable_cover -override_timescale=1ns/1ps -full64 tb_bin ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc test/uartdpi/uartdpi.c \
		-CFLAGS "-I${MKFILE_DIR} -I${MKFILE_DIR}/test -I${FESVR}/include -I${TB_DIR} -Itest/uartdpi" -LDFLAGS "-L${FESVR}/lib" -lfesvr_vcs -lutil

## Clean all build directories and temporary files for VCS simulation
clean.vcs:
	rm -rf bin/spatz_cluster.vcs work-vcs work.lib++ AN.DB work vc_hdrs.h

############
# SPYGLASS #
############

SNPS_SG ?= spyglass-2022.06

.PHONY: lint lint/tmp/files
lint: generate lint/tmp/files lint/sdc/func.sdc lint/script/lint.tcl
	cd lint && $(SNPS_SG) sg_shell -tcl script/lint.tcl

lint/tmp/files: ${BENDER}
	mkdir -p lint/tmp
	${BENDER} script verilator -t rtl -t spatz > lint/tmp/files

######
# SW #
######

## Build SW into sw/build with the LLVM toolchain
sw: clean.sw
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} ${SPATZ_CLUSTER_CFG_DEFINES} .. && make

# VSIM
## Build SW into sw/build with the LLVM toolchain (including tests) for Questasim simulator
sw.vsim: clean.sw bin/spatz_cluster.vsim
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vsim -DBUILD_TESTS=ON ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

## Build SW and run all tests with Questasim simulator
sw.test.vsim: sw.vsim
	cd sw/build && make test

# VCS
## Build SW into sw/build with the LLVM toolchain (including tests) for VCS simulator
sw.vcs: clean.sw bin/spatz_cluster.vcs
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vcs -DBUILD_TESTS=ON ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

## Build SW and run all tests with VCS simulator
sw.test.vcs: sw.vcs
	cd sw/build && make test

# VLT
## Build SW into sw/build with the LLVM toolchain (including tests) for Verilator simulator
sw.vlt: clean.sw bin/spatz_cluster.vlt
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vlt -DBUILD_TESTS=ON ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

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

## Help command
.DEFAULT_GOAL := help
Blue=\033[1;34m
Black=\033[0m
help:
	@echo -e "${Blue}Makefile Targets${Black} for the ${Blue}Spatz Cluster${Black}"
	@echo -e "Use 'make <target>' where <target> is one of:"
	@echo -e ""
	@echo -e "${Blue}help           ${Black}Show an overview of all Makefile targets."
	@echo -e ""
	@echo -e "${Blue}bin/spatz_cluster.vcs  ${Black}Build compilation script and compile all sources for VCS simulation. @IIS: vcs-2022.06 make bin/spatz_cluster.vcs"
	@echo -e "${Blue}bin/spatz_cluster.vlt  ${Black}Build compilation script and compile all sources for Verilator simulation."
	@echo -e "${Blue}bin/spatz_cluster.vsim ${Black}Build compilation script and compile all sources for Questasim simulation."
	@echo -e ""
	@echo -e "${Blue}all            ${Black}Update all SW and HW related sources (by, e.g., re-generating the RegGen registers and their c-header files)."
	@echo -e ""
	@echo -e "${Blue}clean          ${Black}Clean everything except traces in logs directory."
	@echo -e "${Blue}clean.sw       ${Black}Delete sw/build."
	@echo -e "${Blue}clean.logs     ${Black}Delete all traces in logs directory."
	@echo -e "${Blue}clean.vcs      ${Black}Clean all build directories and temporary files for VCS simulation."
	@echo -e "${Blue}clean.vlt      ${Black}Clean all build directories and temporary files for Verilator simulation."
	@echo -e "${Blue}clean.vsim     ${Black}Clean all build directories and temporary files for Questasim simulation."
	@echo -e ""
	@echo -e "${Blue}sw             ${Black}Build SW into sw/build with the LLVM."
	@echo -e "${Blue}sw.vcs         ${Black}Build SW into sw/build with the LLVM (incl. tests) for VCS simulator."
	@echo -e "${Blue}sw.vlt         ${Black}Build SW into sw/build with the LLVM (incl. tests) for Verilator simulator."
	@echo -e "${Blue}sw.vsim        ${Black}Build SW into sw/build with the LLVM (incl. tests) for Questasim simulator."
	@echo -e ""
	@echo -e "${Blue}sw.test.vcs    ${Black}Build SW and run all tests with VCS simulator."
	@echo -e "${Blue}sw.test.vlt    ${Black}Build SW and run all tests with Verilator simulator."
	@echo -e "${Blue}sw.test.vsim   ${Black}Build SW and run all tests with Questasim simulator."
	@echo -e ""
	@echo -e "Additional useful targets from the included Makefrag:"
	@echo -e "${Blue}traces         ${Black}Generate the better readable traces in .logs/trace_hart_<hart_id>.txt with spike-dasm."
