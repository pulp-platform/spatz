# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Author: Matheus Cavalcante, ETH Zurich

SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
ROOT := ${SPATZ_DIR}
BENDER ?= bender
SPATZ_CLUSTER_DIR := ${SPATZ_DIR}/hw/system/spatz_cluster
MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))
# DRAMSys
DRAMSYS_DIR      := $(shell $(BENDER) path dram_rtl_sim)
DRAMSYS_PATH     := ${DRAMSYS_DIR}/dramsys_lib/DRAMSys
DRAMSYS_LIB_PATH := ${DRAMSYS_PATH}/build/lib
DRAMSYS_RES_PATH := ${DRAMSYS_PATH}/configs
USE_DRAMSYS ?= 1

# Configuration file
SPATZ_CLUSTER_CFG         ?= spatz_cluster.default.dram.hjson
SPATZ_CLUSTER_CFG_PATH    := $(SPATZ_CLUSTER_DIR)/cfg/$(SPATZ_CLUSTER_CFG)
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

# QuestaSim
QUESTA_VER ?= questa-2025.3
VSIM        = ${QUESTA_VER} vsim
VLOG        = ${QUESTA_VER} vlog
VSIM_HOME   = /usr/pack/${QUESTA_VER}-dz/questasim

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

ifeq ($(USE_DRAMSYS),1)
  VSIM_BENDER += -t DRAMSYS
  VSIM_FLAGS  += +DRAMSYS_RES=$(DRAMSYS_RES_PATH)
  VSIM_FLAGS  += -sv_lib $(DRAMSYS_LIB_PATH)/libsystemc
  VSIM_FLAGS  += -sv_lib $(DRAMSYS_LIB_PATH)/libDRAMSys_Simulator
  VSIM_FLAGS  += -sv_lib ${MKFILE_DIR}${DPI_LIB}/spatz_dpi
endif

# Verilated and compiled Spatz system
VLT_ROOT = ${VERILATOR_INSTALL_DIR}/share/verilator
VLT_AR   = ${VLT_BUILDDIR}/Vtestharness__ALL.a

all:

test: ${VSIM_SOURCES} ${TB_SRCS}
	echo ${VSIM_SOURCES}
	touch test

#############
# DPI Build #
#############
SIM_DIR        ?= .
DPI_PATH       ?= ../../ip/snitch_test/src
DPI_LIB        ?= work-dpi

dpi_target := $(patsubst ${DPI_PATH}/%.cpp,${SIM_DIR}/${DPI_LIB}/%.o,$(wildcard ${DPI_PATH}/*.cpp))

.PHONY: dpi
dpi: ${SIM_DIR}/${DPI_LIB}/spatz_dpi.so

${SIM_DIR}/${DPI_LIB}/%.o: ${DPI_PATH}/%.cpp
	mkdir -p ${SIM_DIR}/${DPI_LIB}
	$(CXX) -shared -fPIC -std=c++11 -Bsymbolic -c $< -I$(VSIM_HOME)/include -o $@

${SIM_DIR}/${DPI_LIB}/spatz_dpi.so: ${dpi_target}
	mkdir -p ${SIM_DIR}/${DPI_LIB}
	$(CXX) -shared -m64 -o ${SIM_DIR}/${DPI_LIB}/spatz_dpi.so $^

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
	${PYTHON} $(SPATZ_DIR)/util/generate_bootrom.py test/bootrom.bin -c ${SPATZ_CLUSTER_CFG_PATH} --output src/generated/bootrom.sv

.PHONY: generate
generate: src/generated/spatz_cluster_wrapper.sv
src/generated/spatz_cluster_wrapper.sv: ${SPATZ_CLUSTER_CFG_PATH} $(find src/tpl) $(find test/*tpl)
	${PYTHON} ${SPATZ_DIR}/util/clustergen.py -c ${SPATZ_CLUSTER_CFG_PATH} -o src/

## Generate data for all spatz benchmarks
.PHONY: spatz.gendata
spatz.gendata:
	@for benchmark_dir in $(ROOT)/sw/spatzBenchmarks/*/; do \
		script_dir="$$benchmark_dir/script"; \
		data_dir="$$benchmark_dir/data"; \
		if [ -d "$$script_dir" ]; then \
			json_count=$$(find "$$script_dir" -name '*.json' -type f | wc -l); \
			if [ "$$json_count" -gt 0 ]; then \
				echo "Generating data for benchmark $$benchmark_dir ($$json_count json configs)"; \
				for config in "$$script_dir"/*.json; do \
					if [ -f "$$config" ]; then \
						config_path="script/$$(basename $$config)"; \
						(cd "$$benchmark_dir" && ${PYTHON} script/gen_data.py -c "$$config_path") || true; \
					fi \
				done \
			else \
				echo "No json data configs for benchmark $$benchmark_dir"; \
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

## DramSys: download/first build of dram_rtl_sim (run once after clone)
.PHONY: dram-init
dram-init:
	make -C ${DRAMSYS_DIR} -j8 dramsys CXX=$(CXX) CC=$(CC)

## Install the customized DRAM configurations into the DRAMSys config tree
.PHONY: dram-config
dram-config:
	cp tb/dram_config/am_hbm2e_16Gb_pc_brc.json $(DRAMSYS_PATH)/configs/addressmapping/.
	cp tb/dram_config/mc_hbm2e_fr_fcfs_grp.json $(DRAMSYS_PATH)/configs/mcconfig/.
	cp tb/dram_config/ms_hbm2e_16Gb_3600.json $(DRAMSYS_PATH)/configs/memspec/.
	cp tb/dram_config/simconfig_hbm2e.json $(DRAMSYS_PATH)/configs/simconfig/.
	@if [ -f $(DRAMSYS_PATH)/configs/hbm2-example.json ] && [ ! -f $(DRAMSYS_PATH)/configs/hbm2-example.json.ori ]; then \
		mv $(DRAMSYS_PATH)/configs/hbm2-example.json \
		   $(DRAMSYS_PATH)/configs/hbm2-example.json.ori; \
	fi
	cp tb/dram_config/HBM2E-3600.json $(DRAMSYS_PATH)/configs/hbm2-example.json
	cp tb/dram_config/am_ddr4.json $(DRAMSYS_PATH)/configs/addressmapping/.
	cp tb/dram_config/simconfig_ddr4.json $(DRAMSYS_PATH)/configs/simconfig/.
	@if [ -f $(DRAMSYS_PATH)/configs/ddr4-example.json ] && [ ! -f $(DRAMSYS_PATH)/configs/ddr4-example.json.ori ]; then \
		mv $(DRAMSYS_PATH)/configs/ddr4-example.json \
		   $(DRAMSYS_PATH)/configs/ddr4-example.json.ori; \
	fi
	cp tb/dram_config/DDR4-2400.json $(DRAMSYS_PATH)/configs/ddr4-example.json

## Build DRAMSys with the customized configurations
.PHONY: dram-build
dram-build: dram-config
	cd $(DRAMSYS_PATH) && \
	if [ ! -d "build" ]; then \
		mkdir build && cd build; \
		$(CMAKE) -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_C_FLAGS=-fPIC -D DRAMSYS_WITH_DRAMPOWER=ON .. ; \
		make -j; \
	fi

.PHONY: dram-clean
dram-clean:
	if [ -d "$(DRAMSYS_PATH)/build" ]; then \
		rm -rf $(DRAMSYS_PATH)/build; \
	fi

####################
# DRAM traffic plot #
####################
# Run a test on the vsim model and plot its DRAM channel traffic.
#   make TEST=<binary-name> plot
# TEST is the basename of a built binary under sw/build (e.g.
# test-spatzBenchmarks-dp-faxpy_M1024). Produces plots/${TEST}_dram_plot.png
# together with the captured transcript and DRAMSys trace snapshot.
PLOT_DIR  ?= plots
PLOT_TOOL := $(ROOT)/util/dram_traffic_plot.py
# DRAMSys names the trace after the memory (DDR4 -> ..._ddr4_..., HBM2E ->
# ..._hbm2e_...); pick the freshest one written by the run just done.
TRACE_TDB  = $$(ls -t DRAMSysRecordable0_*.tdb 2>/dev/null | head -1)
TEST      ?= test-spatzBenchmarks-dp-faxpy_M1024
TEST_BIN   = $(shell find sw/build -type f -name '$(TEST)' 2>/dev/null | head -1)
# Optional time window (ns) for the plot; full first->last-burst span if unset.
PLOT_WIN   =
ifneq ($(START),)
  PLOT_WIN += --tstart $(START)
endif
ifneq ($(END),)
  PLOT_WIN += --tend $(END)
endif

.PHONY: plot
plot:
	@test -n "$(TEST_BIN)" || { \
		echo "ERROR: TEST='$(TEST)' not found under sw/build (build SW first: make sw)"; \
		exit 1; }
	@test -x bin/spatz_cluster.vsim || { \
		echo "ERROR: bin/spatz_cluster.vsim missing (build HW first: make bin/spatz_cluster.vsim)"; \
		exit 1; }
	@mkdir -p $(PLOT_DIR)
	./bin/spatz_cluster.vsim $(TEST_BIN) | tee $(PLOT_DIR)/$(TEST).log
	cp $(TRACE_TDB) $(PLOT_DIR)/$(TEST).tdb
	$(PYTHON) $(PLOT_TOOL) $(PLOT_DIR)/$(TEST).tdb \
		--transcript $(PLOT_DIR)/$(TEST).log \
		--elf $(TEST_BIN) $(PLOT_WIN) \
		-o $(PLOT_DIR)/$(TEST)_dram_plot.png
	@echo "Wrote $(PLOT_DIR)/$(TEST)_dram_plot.png"

.PHONY: clean-plot clean_plot
clean-plot:
	rm -rf $(PLOT_DIR)
clean_plot: clean-plot   # backwards-compatible alias

.PHONY: help-plot
help-plot:
	@echo "DRAM traffic plotting"
	@echo "====================="
	@echo ""
	@echo "make [TEST=<bin>] [START=<ns>] [END=<ns>] plot"
	@echo "    Run a test binary on bin/spatz_cluster.vsim, capture its DRAM"
	@echo "    trace, and write $(PLOT_DIR)/<TEST>_dram_plot.png (+ .log/.tdb"
	@echo "    snapshots). Requires 'make sw' and 'make bin/spatz_cluster.vsim'."
	@echo ""
	@echo "    TEST   basename of a binary under sw/build"
	@echo "           (default: $(TEST))"
	@echo "    START  start of the plotted time window in ns (default: first burst)"
	@echo "    END    end of the plotted time window in ns   (default: last burst)"
	@echo "           avg / active-avg lines are always over the full run; only"
	@echo "           the view is windowed."
	@echo ""
	@echo "    Examples:"
	@echo "      make TEST=test-spatzBenchmarks-dp-faxpy_M1024 plot"
	@echo "      make TEST=test-spatzBenchmarks-dp-faxpy_M1024 START=4000 END=5300 plot"
	@echo ""
	@echo "make clean-plot"
	@echo "    Remove the $(PLOT_DIR)/ directory (generated plots, logs, trace"
	@echo "    snapshots). These are gitignored."
	@echo ""
	@echo "See util/dram_traffic_plot.README.md for the tool, subplots, and how"
	@echo "the numbers relate to the DRAMSys end-of-run summary."

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
	echo '${VLOG} -work $(dir $@) ${TB_DIR}/rtl_lib.cc ${TB_DIR}/common_lib.cc test/bootdata.cc -ccflags "-std=c++17 -I${MKFILE_DIR}/test -I${MKFILE_DIR}/work/include -I${TB_DIR}"  -cpppath "${CXX}"' >> $@
	echo '${VLOG} -work $(dir $@) test/uartdpi/uartdpi.c -ccflags "-Itest/uartdpi" -cpppath "${CXX}"' >> $@
	echo 'return 0' >> $@

ifeq ($(USE_DRAMSYS),1)
bin/spatz_cluster.vsim: dpi
endif
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
	${BENDER} script verilator -t rtl -t spatz ${BENDER_CFG_TARGETS} ${DEFS} > lint/tmp/files

######
# SW #
######

## Build SW into sw/build with the LLVM toolchain
sw: clean.sw spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DBUILD_TESTS=ON -DSPATZ_CLUSTER_CFG=${SPATZ_CLUSTER_CFG} ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

# VSIM
## Build SW into sw/build with the LLVM toolchain (including tests) for Questasim simulator
sw.vsim: clean.sw bin/spatz_cluster.vsim spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vsim -DBUILD_TESTS=ON -DSPATZ_CLUSTER_CFG=${SPATZ_CLUSTER_CFG} ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

## Build SW and run all tests with Questasim simulator
sw.test.vsim: sw.vsim
	cd sw/build && make test

# VCS
## Build SW into sw/build with the LLVM toolchain (including tests) for VCS simulator
sw.vcs: clean.sw bin/spatz_cluster.vcs spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vcs -DBUILD_TESTS=ON -DSPATZ_CLUSTER_CFG=${SPATZ_CLUSTER_CFG} ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

## Build SW and run all tests with VCS simulator
sw.test.vcs: sw.vcs
	cd sw/build && make test

# VLT
## Build SW into sw/build with the LLVM toolchain (including tests) for Verilator simulator
sw.vlt: clean.sw bin/spatz_cluster.vlt spatz.gendata
	mkdir -p sw/build
	cd sw/build && ${CMAKE} -DLLVM_PATH=${LLVM_INSTALL_DIR} -DGCC_PATH=${GCC_INSTALL_DIR} -DPYTHON=${PYTHON} -DSNITCH_SIMULATOR=../../../../../hw/system/spatz_cluster/bin/spatz_cluster.vlt -DBUILD_TESTS=ON -DSPATZ_CLUSTER_CFG=${SPATZ_CLUSTER_CFG} ${SPATZ_CLUSTER_CFG_DEFINES} .. && make -j8

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
	@echo -e ""
	@echo -e "${Blue}spatz.gendata  ${Black}Generate data for all spatz benchmarks using gen_data.py with all config files."