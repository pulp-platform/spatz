# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Author: Matheus Cavalcante, ETH Zurich

SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
ROOT := ${SPATZ_DIR}
SPATZ_CLUSTER_DIR := ${SPATZ_DIR}/hw/system/spatz_cluster
MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))

# Debug variables
PRINT ?= ON
TRACE ?= OFF

# Configuration file
CFG_STAMP := .spatz_cluster_cfg

#if the cfg is not passed as a command line argument, reuse the last one used by `make sw`.
ifeq ($(origin SPATZ_CLUSTER_CFG),undefined)
SPATZ_CLUSTER_CFG := $(shell cat $(CFG_STAMP) 2>/dev/null || echo default)
endif

.PHONY: FORCE
FORCE:

$(CFG_STAMP): FORCE
	@if [ ! -f $@ ] || [ "$$(cat $@)" != "$(SPATZ_CLUSTER_CFG)" ]; then \
		echo ">>> config: $(SPATZ_CLUSTER_CFG) (changed: regenerating wrapper, bootrom and RTL)"; \
		printf '%s' "$(SPATZ_CLUSTER_CFG)" > $@; \
	fi

SPATZ_CLUSTER_CFG_PATH    := $(SPATZ_CLUSTER_DIR)/cfg/spatz_cluster.$(SPATZ_CLUSTER_CFG).dram.hjson
SPATZ_CLUSTER_CFG_DEFINES += -DMEM_DRAM_ORIGIN=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['dram']['address'])")
SPATZ_CLUSTER_CFG_DEFINES += -DMEM_DRAM_SIZE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['dram']['length'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_BASE_HARTID=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['cluster']['cluster_base_hartid'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_CLUSTER_CORE_NUM=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(len(jstyleson.load(f)['cluster']['cores']))")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_TCDM_START_ADDR=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['cluster']['cluster_base_addr'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_CLUSTER_OFFSET=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['cluster']['cluster_base_offset'])")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_TCDM_SIZE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['cluster']['tcdm']['size'] * 1024)")
SPATZ_CLUSTER_CFG_DEFINES += -DSNRT_NFPU_PER_CORE=$(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(jstyleson.load(f)['cluster']['n_fpu'])")

RISCV_EXT := $(shell python3 -c "import jstyleson; print(jstyleson.load(open('$(SPATZ_CLUSTER_CFG_PATH)'))['cluster']['cores'][0].get('isa', 'rv32'))")
ifneq ($(findstring d,$(RISCV_EXT)),)
    SPATZ_CLUSTER_CFG_DEFINES += -DELEN=64
else
    SPATZ_CLUSTER_CFG_DEFINES += -DELEN=32
endif

# Enable additional uA configurations for the spatz core
DOUBLE_BW := $(shell python3 -c "import jstyleson; print(jstyleson.load(open('$(SPATZ_CLUSTER_CFG_PATH)'))['cluster'].get('double_bw', 0))")
BUF_FPU := $(shell python3 -c "import jstyleson; print(jstyleson.load(open('$(SPATZ_CLUSTER_CFG_PATH)'))['cluster'].get('buf_fpu', 0))")
# Ventaglio (VTL) compile-time gate. If the cluster has `ventaglio: true` in the
# hjson (a Spatz/cluster-level extension), define VENTAGLIO so the RTL `ifdef
# VENTAGLIO ... `endif blocks pull in the VTL integration.
SPATZ_CLUSTER_VENTAGLIO := $(shell python3 -c "import jstyleson; f = open('$(SPATZ_CLUSTER_CFG_PATH)'); print(1 if jstyleson.load(f)['cluster'].get('ventaglio', False) else 0)")

ifeq ($(DOUBLE_BW),1)
	DEFS += -DDOUBLE_BW
	SPATZ_CLUSTER_CFG_DEFINES += -DUNROLL=1
endif

ifeq ($(BUF_FPU),1)
	DEFS += -DBUF_FPU
endif

ifeq ($(SPATZ_CLUSTER_VENTAGLIO),1)
	DEFS += -DVENTAGLIO
	BENDER_CFG_TARGETS += -t ventaglio
	SPATZ_CLUSTER_CFG_DEFINES += -DSPATZ_CLUSTER_VENTAGLIO=1
endif

# Include Makefrag
include $(ROOT)/util/Makefrag

##################################################
#####           Tool Flags                   #####
##################################################

#############
# QuestaSim #
#############

VSIM      ?= vsim
VLOG      ?= vlog
VSIM_HOME ?=

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

#######
# VCS #
#######

VCS       ?= vcs
VERDI     ?= verdi
VERDI_GUI ?= ultra
FSDB_FILE ?= inter.fsdb

VCS_BUILDDIR ?= work-vcs
VCS_SIMV     := bin/spatz_cluster.vcs.simv

VCS_COMMON_FLAGS += -Mlib=$(VCS_BUILDDIR)
VCS_COMMON_FLAGS += -Mdir=$(VCS_BUILDDIR)
VCS_COMMON_FLAGS += -full64
VCS_COMMON_FLAGS += -sverilog
VCS_COMMON_FLAGS += -override_timescale=1ns/1ps
VCS_COMMON_FLAGS += -assert disable_cover
VCS_COMMON_FLAGS += -lca
VCS_COMMON_FLAGS += -j4
VCS_COMMON_FLAGS += -cc $(CC)
VCS_COMMON_FLAGS += -cpp $(CXX)

VCS_DEBUG ?= 1

ifeq ($(VCS_DEBUG),1)
VCS_DEBUG_FLAGS += -debug_access+all
VCS_DEBUG_FLAGS += -kdb=common_elab
VCS_DEBUG_FLAGS += +vcs+fsdbon
VLOGAN_FLAGS += +define+FSDB_DUMP
else
VCS_DEBUG_FLAGS += -O2
endif

########
# XRUN #
########
XRUN_BUILDDIR = work-xrun
XRUN_SNAPSHOT_DIR  := $(abspath ${XRUN_BUILDDIR})
XRUN_HOME ?= $(shell xmroot)
XRUN_CXXFLAGS += -Wcxx,-std=c++17,-I${ROOT}/hw/system/spatz_cluster/test,-I${ROOT}/hw/system/spatz_cluster/work/include,-I${TB_DIR}
XRUN_CFLAGS += -Wcc,-I${ROOT}/hw/system/spatz_cluster/test
XRUN_LDFLAGS += -Wld,-l:libfesvr_xrun.a,-L${FESVR}/lib

XRUN_FLAGS += -64bit
XRUN_FLAGS += -elaborate
XRUN_FLAGS += -allowredefinition
XRUN_FLAGS += -glsperf
XRUN_FLAGS += -timescale 1ns/1ns
XRUN_FLAGS += -vtimescale 1ns/1ns
XRUN_FLAGS += -enable_inout_out_def_arg
XRUN_FLAGS += -access +r
XRUN_FLAGS += -enable_archive_search
XRUN_FLAGS += -top tb_bin
XRUN_FLAGS += ${XRUN_LDFLAGS}
XRUN_FLAGS += ${XRUN_CXXFLAGS}
XRUN_FLAGS += ${XRUN_CFLAGS}

TB_CCSRC += ${TB_DIR}/rtl_lib.cc
TB_CCSRC += ${TB_DIR}/common_lib.cc
TB_CCSRC += ${ROOT}/hw/system/spatz_cluster/test/bootdata.cc
TB_CCSRC += ${ROOT}/hw/system/spatz_cluster/test/uartdpi/uartdpi.c

#############
# Verilator #
#############

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

#3 different targets instead of a cumulative one to avoid unnecessary rebuilds

test/bootrom.elf: src/generated/spatz_cluster_wrapper.sv test/bootrom.S test/bootrom.ld test/bootdata_bootrom.cc Makefile
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-gcc -mabi=ilp32 -march=rv32imaf -static -nostartfiles -Ttest/bootrom.ld test/bootrom.S test/bootdata_bootrom.cc -I../../ip/snitch_test/src -I../../../sw/snRuntime/include -I../../../sw/toolchain/riscv-opcodes/ -o test/bootrom.elf

test/bootrom.dump: test/bootrom.elf
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-objdump -D test/bootrom.elf > test/bootrom.dump

test/bootrom.bin: test/bootrom.elf
	${GCC_INSTALL_DIR}/bin/riscv32-unknown-elf-objcopy -O binary test/bootrom.elf test/bootrom.bin

.PHONY: bootrom
bootrom: $(CURDIR)/src/generated/bootrom.sv
$(CURDIR)/src/generated/bootrom.sv: test/bootrom.bin
	${PYTHON} $(SPATZ_DIR)/util/generate_bootrom.py test/bootrom.bin -c ${SPATZ_CLUSTER_CFG_PATH} --output src/generated/bootrom.sv

test/bootdata.cc test/bootdata_bootrom.cc: src/generated/spatz_cluster_wrapper.sv

.PHONY: generate
generate: src/generated/spatz_cluster_wrapper.sv

CLUSTER_TPL_SRCS := $(shell find src/tpl -type f 2>/dev/null)
CLUSTER_TPL_SRCS += $(shell find test -name '*.tpl' 2>/dev/null)

src/generated/spatz_cluster_wrapper.sv: $(CFG_STAMP) ${SPATZ_CLUSTER_CFG_PATH} $(CLUSTER_TPL_SRCS)
	${PYTHON} ${SPATZ_DIR}/util/clustergen.py -c ${SPATZ_CLUSTER_CFG_PATH} -o src/
## Generate data for all spatz benchmarks
.PHONY: spatz.gendata
spatz.gendata:
	@for benchmark_dir in $(ROOT)/sw/spatzBenchmarks/*/; do \
		script_dir="$$benchmark_dir/script"; \
		data_dir="$$benchmark_dir/data"; \
		if [ -d "$$script_dir" ]; then \
			json_count=$$(find "$$script_dir" -name '*.json' -type f | wc -l); \
			data_count=$$(find "$$data_dir" -name 'data*.h' -type f 2>/dev/null | wc -l); \
			if [ "$$json_count" -ne "$$data_count" ]; then \
				echo "Generating data for benchmark $$benchmark_dir ($$json_count json configs, $$data_count data files found)"; \
				for config in "$$script_dir"/*.json; do \
					if [ -f "$$config" ]; then \
						config_path="script/$$(basename $$config)"; \
						(cd "$$benchmark_dir" && ${PYTHON} script/gen_data.py -c "$$config_path") || true; \
					fi \
				done \
			else \
				echo "Data already present for benchmark $$benchmark_dir ($$json_count data files)"; \
			fi \
		fi \
	done

.PHONY: spatz.cleandata
spatz.cleandata:
	@for benchmark_dir in $(ROOT)/sw/spatzBenchmarks/*/; do \
		data_dir="$$benchmark_dir/data"; \
		if [ -d "$$data_dir" ]; then \
			data_count=$$(find "$$data_dir" -name 'data*.h' -type f 2>/dev/null | wc -l); \
			if [ "$$data_count" -gt 0 ]; then \
				echo "Cleaning $$data_count data file(s) from $$data_dir"; \
				rm -f "$$data_dir"/data*.h; \
			fi \
		fi \
	done
	@echo "All benchmark data cleaned."

###############################################################################
########## Simulation launch: make sim.<tool> TEST=<path/to/elf>  #############
###############################################################################

TEST ?=

define check_test
	@test -n "$(strip ${TEST})" || { echo "ERROR: specify TEST=<path/to/elf>, ex. make $@ TEST=./sw/build/riscvTests/test-riscvTests-vadd"; exit 1; }
	@test -f "${TEST}" || { echo "ERROR: TEST='${TEST}' not found (did you run 'make sw'?)"; exit 1; }
endef

#############
# Verilator #
#############

${VLT_AR}: ${CFG_STAMP} $(CURDIR)/src/generated/bootrom.sv ${VLT_SOURCES} ${TB_SRCS}
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

# Run a test with Verilator
sim.vlt: bin/spatz_cluster.vlt
	$(check_test)
	./bin/spatz_cluster.vlt $$(realpath ${TEST})

# Clean all build directories and temporary files for Verilator simulation
clean.vlt:
	rm -rf work-vlt
	rm -f bin/spatz_cluster.vlt

sw.test.vlt: sw.vlt
	cd sw/build && make test

.PHONY: clean.vlt sim.vlt sw.test.vlt
#############
# Questasim #
#############

${VSIM_BUILDDIR}/compile.vsim.tcl: $(CFG_STAMP) $(CURDIR)/src/generated/bootrom.sv test/bootrom.bin $(VSIM_SOURCES) ${TB_SRCS} ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc
	vlib $(dir $@)
	${BENDER} script vsim ${VSIM_BENDER} ${DEFS} --vlog-arg="${VLOG_FLAGS} -work $(dir $@) " > $@
	echo '${VLOG} -work $(dir $@) ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc -ccflags "-std=c++17 -I${MKFILE_DIR}/test -I${MKFILE_DIR}/work/include -I${TB_DIR}"' >> $@
	echo '${VLOG} -work $(dir $@) test/uartdpi/uartdpi.c -ccflags "-Itest/uartdpi"' >> $@
	echo 'return 0' >> $@

bin/spatz_cluster.vsim: ${VSIM_BUILDDIR}/compile.vsim.tcl work/lib/libfesvr_vsim.a
	mkdir -p bin/logs
	$(call QUESTASIM,tb_bin)

# Run a test with QuestaSim
sim.vsim: bin/spatz_cluster.vsim
	$(check_test)
	./bin/spatz_cluster.vsim $$(realpath ${TEST})

# Run a test with QuestaSim (GUI)
sim.vsim.gui: bin/spatz_cluster.vsim
	$(check_test)
	./bin/spatz_cluster.vsim.gui $$(realpath ${TEST})

clean.vsim:
	rm -rf bin/spatz_cluster.vsim bin/spatz_cluster.vsim.gui work-vsim work vsim.wlf vish_stacktrace.vstf transcript

sw.test.vsim: sw.vsim
	cd sw/build && make test

.PHONY: clean.vsim sim.vsim sim.vsim.gui sw.test.vsim

########
# XRUN #
########

${XRUN_BUILDDIR}/flist: $(CFG_STAMP) ${BENDER}
	mkdir -p ${XRUN_BUILDDIR}
	# these sed commands remove some files wich would cause errors during elaboration
	${BENDER} script flist-plus ${VSIM_BENDER} ${DEFS} | sed /tb_apb/d | sed /tb_axi_iw/d > $@

${XRUN_BUILDDIR}/xcelium.d: $(CURDIR)/src/generated/bootrom.sv test/bootrom.bin ${XRUN_BUILDDIR}/flist work/lib/libfesvr_xrun.a ${TB_CCSRC}
	@cd ${XRUN_BUILDDIR}; xrun ${XRUN_FLAGS} -sv -f flist ${TB_CCSRC}

sim.xrun: bin/spatz_cluster.xrun
	$(check_test)
	./bin/spatz_cluster.xrun $$(realpath ${TEST})

sim.xrun.gui: bin/spatz_cluster.xrun.gui
	$(check_test)
	./bin/spatz_cluster.xrun.gui $$(realpath ${TEST})

clean.xrun:
	rm -rf work-xrun work bin/spatz_cluster.xrun* work-xrun-runs

sw.test.xrun: sw.xrun
	cd sw/build && make test

bin/spatz_cluster.xrun: ${XRUN_BUILDDIR}/xcelium.d
	mkdir -p bin bin/logs
	@echo '#!/bin/bash' > $@
	@echo 'set -e' >> $@
	@echo 'if [ "$$#" -lt 1 ]; then echo "Usage: $$0 <elf> [xrun_args...]"; exit 1; fi' >> $@
	@echo 'ELF=$$(realpath $$1); shift' >> $@
	@echo 'RUNDIR=${MKFILE_DIR}work-xrun-runs/$$(basename $$ELF)' >> $@
	@echo 'rm -rf $$RUNDIR && mkdir -p $$RUNDIR' >> $@
	@echo 'ln -sfn ${MKFILE_DIR}bin $$RUNDIR/bin' >> $@
	@echo 'echo $$ELF > ${MKFILE_DIR}bin/logs/.rtlbinary' >> $@
	@echo 'cd $$RUNDIR' >> $@
	@echo 'exec xrun +permissive -64 -R -xmlibdirpath ${XRUN_SNAPSHOT_DIR} +permissive-off ++$$ELF "$$@"' >> $@
	@chmod +x $@

bin/spatz_cluster.xrun.gui: bin/spatz_cluster.xrun
	@sed 's/-R -xmlibdirpath/-R -gui -xmlibdirpath/' $< > $@
	@chmod +x $@

.PHONY: clean.xrun sim.xrun sim.xrun.gui sw.test.xrun

#######
# VCS #
#######

work-vcs/compile.sh: $(CFG_STAMP) $(CURDIR)/src/generated/bootrom.sv

bin/spatz_cluster.vcs: $(CFG_STAMP) test/bootrom.bin work-vcs/compile.sh work/lib/libfesvr_vcs.a ${TB_DIR}/common_lib.cc test/bootdata.cc test/uartdpi/uartdpi.c
	mkdir -p bin bin/logs
	$(VCS) $(VCS_COMMON_FLAGS) $(VCS_DEBUG_FLAGS) \
	    -o $@.simv \
	    tb_bin ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc test/uartdpi/uartdpi.c \
	    -CFLAGS "-I${MKFILE_DIR} -I${MKFILE_DIR}/test -I${FESVR}/include -I${TB_DIR} -Itest/uartdpi" \
	    -LDFLAGS "-L${FESVR}/lib" \
	    -lfesvr_vcs -lutil
	@echo '#!/bin/bash' > $@
	@echo 'mkdir -p ${MKFILE_DIR}bin/logs' >> $@
	@echo 'echo `realpath $$1` > ${MKFILE_DIR}bin/logs/.rtlbinary' >> $@
	@echo 'exec ${MKFILE_DIR}$@.simv "$$@" < /dev/null' >> $@
	@chmod +x $@

bin/spatz_cluster.vcs.gui: bin/spatz_cluster.vcs
	@echo '#!/bin/bash' > $@
	@echo 'set -e' >> $@
	@echo 'mkdir -p ${MKFILE_DIR}bin/logs' >> $@
	@echo 'if [ "$$#" -lt 1 ]; then' >> $@
	@echo '  echo "Usage: $$0 <elf/binary> [simv_args...]"' >> $@
	@echo '  exit 1' >> $@
	@echo 'fi' >> $@
	@echo 'echo `realpath $$1` > ${MKFILE_DIR}bin/logs/.rtlbinary' >> $@
	@echo 'UCLI_CMD=${MKFILE_DIR}bin/logs/vcs_gui_auto_run.ucli' >> $@
	@echo 'echo "run" > $$UCLI_CMD' >> $@
	@echo 'exec ${MKFILE_DIR}bin/spatz_cluster.vcs.simv -gui=$(VERDI_GUI) -ucli -do $$UCLI_CMD "$$@" -l ${MKFILE_DIR}bin/logs/vcs_gui.log < /dev/null' >> $@
	@chmod +x $@

bin/spatz_cluster.verdi.vcs: bin/spatz_cluster.vcs
	$(VERDI) -dbdir $(VCS_BUILDDIR) -ssf $(FSDB_FILE)

# Run a test with VCS
sim.vcs: bin/spatz_cluster.vcs
	$(check_test)
	./bin/spatz_cluster.vcs $$(realpath ${TEST})

# Run a test with VCS/Verdi (GUI)
sim.vcs.gui: bin/spatz_cluster.vcs.gui
	$(check_test)
	./bin/spatz_cluster.vcs.gui $$(realpath ${TEST})

## Clean all build directories and temporary files for VCS simulation
clean.vcs:
	rm -rf bin/spatz_cluster.vcs bin/spatz_cluster.vcs.gui bin/spatz_cluster.vcs.simv work-vcs work.lib++ AN.DB work vc_hdrs.h

sw.test.vcs: sw.vcs
	cd sw/build && make test

.PHONY: clean.vcs sim.vcs sim.vcs.gui sw.test.vcs

############
# SPYGLASS #
############

SNPS_SG ?= spyglass-2022.06

.PHONY: lint lint/tmp/files
lint: generate lint/tmp/files lint/sdc/func.sdc lint/script/lint.tcl
	cd lint && $(SNPS_SG) sg_shell -tcl script/lint.tcl

lint/tmp/files: ${BENDER}
	mkdir -p lint/tmp
	${BENDER} script verilator -t rtl -t spatz ${BENDER_CFG_TARGETS} ${DEFS} > lint/tmp/files

######
# SW #
######

## Build SW into sw/build with the LLVM toolchain
sw: $(CFG_STAMP) clean.sw spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DBUILD_TESTS=ON -DRUNTIME_PRINT=${PRINT} -DRUNTIME_TRACE=${TRACE} -DSPATZ_CLUSTER_CFG=spatz_cluster.${SPATZ_CLUSTER_CFG}.dram.hjson ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

# VSIM
## Build SW into sw/build with the LLVM toolchain (including tests) for Questasim simulator
sw.vsim: clean.sw bin/spatz_cluster.vsim spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vsim -DBUILD_TESTS=ON -DRUNTIME_PRINT=${PRINT} -DRUNTIME_TRACE=${TRACE} -DSPATZ_CLUSTER_CFG=spatz_cluster.${SPATZ_CLUSTER_CFG}.dram.hjson ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

# XRUN
## Build SW into sw/build with the LLVM toolchain (including tests) for Xcelium simulator
sw.xrun: clean.sw bin/spatz_cluster.xrun spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.xrun -DBUILD_TESTS=ON -DRUNTIME_PRINT=${PRINT} -DRUNTIME_TRACE=${TRACE} -DSPATZ_CLUSTER_CFG=spatz_cluster.${SPATZ_CLUSTER_CFG}.dram.hjson ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

# VCS
## Build SW into sw/build with the LLVM toolchain (including tests) for VCS simulator
sw.vcs: clean.sw bin/spatz_cluster.vcs bin/spatz_cluster.vcs.gui spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vcs -DBUILD_TESTS=ON -DRUNTIME_PRINT=${PRINT} -DRUNTIME_TRACE=${TRACE} -DSPATZ_CLUSTER_CFG=spatz_cluster.${SPATZ_CLUSTER_CFG}.dram.hjson ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

# VLT
## Build SW into sw/build with the LLVM toolchain (including tests) for Verilator simulator
sw.vlt: clean.sw bin/spatz_cluster.vlt spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vlt -DBUILD_TESTS=ON -DRUNTIME_PRINT=${PRINT} -DRUNTIME_TRACE=${TRACE} -DSPATZ_CLUSTER_CFG=spatz_cluster.${SPATZ_CLUSTER_CFG}.dram.hjson ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

## Delete sw/build
clean.sw:
	rm -rf sw/build

########
# Util #
########

## Clean everything except logs
clean: clean.vcs clean.vlt clean.vsim clean.sw clean.xrun
	rm -rf Bender.lock .bender/ work/
	rm -f $(CFG_STAMP)

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
	@echo -e "${Blue}Typical Workflow${Black}"
	@echo -e "  make sw SPATZ_CLUSTER_CFG=default"
	@echo -e "  make sim.vsim TEST=./sw/build/riscvTests/test-riscvTests-vadd"
	@echo -e "  make sim.xrun.gui TEST=./sw/build/riscvTests/test-riscvTests-vand"
	@echo -e ""
	@echo -e "${Blue}help           ${Black}Show an overview of all Makefile targets."
	@echo -e ""
	@echo -e "${Blue}sw             ${Black}Build only the SW into sw/build with the LLVM toolchain."
	@echo -e "${Blue}sw.vsim        ${Black}Build SW + RTL for QuestaSim and register the ctest suite."
	@echo -e "${Blue}sw.vcs         ${Black}Build SW + RTL for VCS and register the ctest suite."
	@echo -e "${Blue}sw.xrun        ${Black}Build SW + RTL for Xcelium and register the ctest suite."
	@echo -e "${Blue}sw.vlt         ${Black}Build SW + RTL for Verilator and register the ctest suite."
	@echo -e ""
	@echo -e "${Blue}sw.test.vsim   ${Black}Build SW and run all tests with QuestaSim."
	@echo -e "${Blue}sw.test.vcs    ${Black}Build SW and run all tests with VCS."
	@echo -e "${Blue}sw.test.xrun   ${Black}Build SW and run all tests with Xcelium."
	@echo -e "${Blue}sw.test.vlt    ${Black}Build SW and run all tests with Verilator."
	@echo -e ""
	@echo -e "${Blue}sim.vsim       ${Black}Build the RTL if needed and run TEST=<elf> with QuestaSim."
	@echo -e "${Blue}sim.vsim.gui   ${Black}Same, in the QuestaSim GUI."
	@echo -e "${Blue}sim.vcs        ${Black}Build the RTL if needed and run TEST=<elf> with VCS."
	@echo -e "${Blue}sim.vcs.gui    ${Black}Same, in the Verdi GUI."
	@echo -e "${Blue}sim.xrun       ${Black}Elaborate the snapshot if needed and run TEST=<elf> with Xcelium."
	@echo -e "${Blue}sim.xrun.gui   ${Black}Same, in the Xcelium GUI."
	@echo -e "${Blue}sim.vlt        ${Black}Build the RTL if needed and run TEST=<elf> with Verilator."
	@echo -e ""
	@echo -e "${Blue}bin/spatz_cluster.vsim ${Black}Compile all sources for QuestaSim simulation."
	@echo -e "${Blue}bin/spatz_cluster.vcs  ${Black}Compile all sources for VCS simulation. @IIS: vcs-2022.06 make bin/spatz_cluster.vcs"
	@echo -e "${Blue}bin/spatz_cluster.xrun ${Black}Elaborate the Xcelium snapshot and generate the run wrapper."
	@echo -e "${Blue}bin/spatz_cluster.vlt  ${Black}Compile all sources for Verilator simulation."
	@echo -e ""
	@echo -e "${Blue}clean          ${Black}Clean everything except traces in logs directory."
	@echo -e "${Blue}clean.sw       ${Black}Delete sw/build."
	@echo -e "${Blue}clean.logs     ${Black}Delete all traces in logs directory."
	@echo -e "${Blue}clean.vsim     ${Black}Clean all build directories and temporary files for QuestaSim."
	@echo -e "${Blue}clean.vcs      ${Black}Clean all build directories and temporary files for VCS."
	@echo -e "${Blue}clean.xrun     ${Black}Clean all build directories and temporary files for Xcelium."
	@echo -e "${Blue}clean.vlt      ${Black}Clean all build directories and temporary files for Verilator."
	@echo -e ""
	@echo -e "Additional useful targets from the included Makefrag:"
	@echo -e "${Blue}traces         ${Black}Generate the better readable traces in .logs/trace_hart_<hart_id>.txt with spike-dasm."
	@echo -e ""
	@echo -e "${Blue}spatz.gendata  ${Black}Generate data for all spatz benchmarks using gen_data.py with all config files."