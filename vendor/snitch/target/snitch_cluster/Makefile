# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Fabian Schuiki <fschuiki@iis.ee.ethz.ch>
# Florian Zaruba <zarubaf@iis.ee.ethz.ch>

#######################
# Makefile invocation #
#######################

DEBUG          ?= OFF  # ON to turn on debugging symbols and wave logging
CFG_OVERRIDE   ?=      # Override default config file
SELECT_RUNTIME ?=      # Select snRuntime implementation: "banshee" or "rtl" (default)

.DEFAULT_GOAL := help
.PHONY: all clean
all: sw
clean: clean-sw clean-work clean-vsim clean-vlt clean-vcs clean-logs clean-bender clean-generated

##########
# Common #
##########

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))
ROOT        := ${MKFILE_DIR}../..
SNITCH_ROOT := $(ROOT)

TARGET = snitch_cluster

include $(ROOT)/target/common/common.mk

############
# Programs #
############

REGGEN          ?= $(shell $(BENDER) path register_interface)/vendor/lowrisc_opentitan/util/regtool.py
CLUSTER_GEN     ?= $(ROOT)/util/clustergen.py
CLUSTER_GEN_SRC ?= $(wildcard $(ROOT)/util/clustergen/*.py)

#########################
# Files and directories #
#########################

SW_DIR             = sw
BIN_DIR           ?= bin
TARGET_C_HDRS_DIR ?= $(SW_DIR)/runtime/common
GENERATED_DIR     ?= $(MKFILE_DIR)generated

# Verilated and compiled snitch cluster
VLT_AR = ${VLT_BUILDDIR}/Vtestharness__ALL.a

# If the configuration file is overriden on the command-line (through
# CFG_OVERRIDE) and this file differs from the least recently used
# (LRU) config, all targets depending on the configuration file have
# to be rebuilt. This file is used to express this condition as a
# prerequisite for other rules.
DEFAULT_CFG = cfg/default.hjson
CFG         = cfg/lru.hjson

#####################
# Simulator options #
#####################

VSIM_BENDER += -t snitch_cluster
VLT_BENDER  += -t snitch_cluster
VCS_BENDER  += -t snitch_cluster

QUESTA_64BIT = -64
VLOG_64BIT   = -64

VSIM_FLAGS += ${QUESTA_64BIT}

VLOG_FLAGS += -svinputport=compat
VLOG_FLAGS += -override_timescale 1ns/1ps
VLOG_FLAGS += -suppress 2583
VLOG_FLAGS += -suppress 13314
VLOG_FLAGS += ${VLOG_64BIT}

VLT_FLAGS  += --trace

###############
# C testbench #
###############

TB_CC_SOURCES += \
	${TB_DIR}/bootrom.cc \
	${TB_DIR}/ipc.cc \
	${TB_DIR}/rtl_lib.cc \
	${TB_DIR}/common_lib.cc \
	$(GENERATED_DIR)/bootdata.cc

TB_CC_FLAGS += \
	-std=c++14 \
	-I${MKFILE_DIR}/test \
	-I${FESVR}/include \
	-I${TB_DIR}

# Required C sources for the verilator TB that are linked against the verilated model
VLT_COBJ += $(VLT_BUILDDIR)/tb/bootrom.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/ipc.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/common_lib.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/verilator_lib.o
VLT_COBJ += $(VLT_BUILDDIR)/tb/tb_bin.o
# Sources from verilator root
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_dpi.o
VLT_COBJ += $(VLT_BUILDDIR)/vlt/verilated_vcd_c.o
# Bootdata
VLT_COBJ += $(VLT_BUILDDIR)/generated/bootdata.o

#################
# Prerequisites #
#################

CLUSTER_GEN_PREREQ = ${CLUSTER_GEN} ${CLUSTER_GEN_SRC}

# This target is always evaluated and creates a symlink to the least
# recently used config file. Because it is a symlink, targets to which it is a
# prerequisite will only be updated if the symlink target is newer than the
# depending targets, regardless of the symlink timestamp itself. The symlink
# timestamp can be taken into account by using the `make -L` flag on the
# command-line, however for simplicity we touch the symlink targets so it can
# be used without.
$(CFG): FORCE
	@# If the LRU config file doesn't exist, we use the default config.
	@if [ ! -e "$@" ] ; then \
		echo "Using default config file: $(DEFAULT_CFG) $@"; \
		ln -s --relative $(DEFAULT_CFG) $@; \
		touch $(DEFAULT_CFG); \
	fi
	@# If a config file is provided on the command-line and the LRU
	@# config file doesn't point to it already, then we make it point to it
	@if [ $(CFG_OVERRIDE) ] ; then \
		echo "Overriding config file with: $(CFG_OVERRIDE)"; \
		target=$$(readlink -f $@); \
		if [ "$$target" = "$(abspath $(CFG_OVERRIDE))" ] ; then \
			echo "LRU config file already points to $(CFG_OVERRIDE). Nothing to be done."; \
		else \
			rm -f $@; \
			ln -s --relative $(CFG_OVERRIDE) $@; \
			touch $(CFG_OVERRIDE); \
		fi \
	fi
FORCE:

test/bootrom.elf test/bootrom.dump test/bootrom.bin: test/bootrom.S test/bootrom.ld
	riscv64-unknown-elf-gcc -mabi=ilp32d -march=rv32imafd -static -nostartfiles -Ttest/bootrom.ld $< -o test/bootrom.elf
	riscv64-unknown-elf-objdump -d test/bootrom.elf > test/bootrom.dump
	riscv64-unknown-elf-objcopy -j .text -O binary test/bootrom.elf test/bootrom.bin

############
# Software #
############

CLUSTER_GEN_HEADERS = snitch_cluster_cfg.h \
					  snitch_cluster_addrmap.h

REGGEN_HEADERS = snitch_cluster_peripheral.h

TARGET_C_HDRS = $(addprefix $(TARGET_C_HDRS_DIR)/,$(CLUSTER_GEN_HEADERS) $(REGGEN_HEADERS))

.PHONY: sw clean-headers clean-sw

sw: $(TARGET_C_HDRS)
	$(MAKE) -C $(SW_DIR)

clean-headers:
	rm -f $(TARGET_C_HDRS)

clean-sw: clean-headers
	$(MAKE) -C $(SW_DIR) TARGET=clean

# CLUSTERGEN headers
$(addprefix $(TARGET_C_HDRS_DIR)/,$(CLUSTER_GEN_HEADERS)): %.h: $(CFG) $(CLUSTER_GEN_PREREQ) %.h.tpl
	@echo "[CLUSTERGEN] Generate $@"
	$(CLUSTER_GEN) -c $< --outdir $(TARGET_C_HDRS_DIR) --template $@.tpl

# REGGEN headers
$(TARGET_C_HDRS_DIR)/snitch_cluster_peripheral.h: $(ROOT)/hw/snitch_cluster/src/snitch_cluster_peripheral/snitch_cluster_peripheral_reg.hjson $(REGGEN)
	$(call reggen_generate_header,$@,$<)

#######
# RTL #
#######

$(GENERATED_DIR):
	mkdir -p $@

$(GENERATED_DIR)/snitch_cluster_wrapper.sv: ${CFG} ${CLUSTER_GEN_PREREQ} | $(GENERATED_DIR)
	$(CLUSTER_GEN) -c $< -o $(GENERATED_DIR) --wrapper

$(GENERATED_DIR)/link.ld: ${CFG} ${CLUSTER_GEN_PREREQ} | $(GENERATED_DIR)
	$(CLUSTER_GEN) -c $< -o $(GENERATED_DIR) --linker

$(GENERATED_DIR)/memories.json: ${CFG} ${CLUSTER_GEN_PREREQ} | $(GENERATED_DIR)
	$(CLUSTER_GEN) -c $< -o $(GENERATED_DIR) --memories

$(GENERATED_DIR)/bootdata.cc: ${CFG} ${CLUSTER_GEN_PREREQ} | $(GENERATED_DIR)
	$(CLUSTER_GEN) -c $< -o $(GENERATED_DIR) --bootdata

#############
# Verilator #
#############

.PHONY: clean-vlt

# Clean all build directories and temporary files for Verilator simulation
clean-vlt: clean-work
	rm -rf $(BIN_DIR)/$(TARGET).vlt $(VLT_BUILDDIR)

$(VLT_AR): ${VLT_SOURCES} ${TB_SRCS}
	$(call VERILATE,testharness)
verilate: $(VLT_AR)

# Build targets for verilator TB
$(VLT_BUILDDIR)/tb/%.o: $(TB_DIR)/%.cc $(VLT_AR) ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@
$(VLT_BUILDDIR)/vlt/%.o: $(VLT_ROOT)/include/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@
$(VLT_BUILDDIR)/generated/%.o: $(GENERATED_DIR)/%.cc ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(VLT_CFLAGS) -c $< -o $@

# Build compilation script and compile all sources for Verilator simulation
# Link verilated archive with $(VLT_COBJ)
$(BIN_DIR)/$(TARGET).vlt: $(VLT_AR) $(VLT_COBJ) ${VLT_BUILDDIR}/lib/libfesvr.a
	mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -std=c++14 -L ${VLT_BUILDDIR}/lib -o $@ $(VLT_COBJ) $(VLT_AR) -lfesvr -lpthread

############
# Modelsim #
############

include $(ROOT)/target/common/vsim.mk

#######
# VCS #
#######

.PHONY: clean-vcs

# Clean all build directories and temporary files for VCS simulation
clean-vcs: clean-work
	rm -rf $(BIN_DIR)/$(TARGET).vcs $(VCS_BUILDDIR) vc_hdrs.h

# Build compilation script and compile all sources for VCS simulation
$(BIN_DIR)/$(TARGET).vcs: ${VCS_SOURCES} ${TB_SRCS} $(TB_CC_SOURCES) $(VCS_BUILDDIR)/compile.sh work/lib/libfesvr.a
	mkdir -p $(BIN_DIR)
	$(VCS) -Mlib=$(VCS_BUILDDIR) -Mdir=$(VCS_BUILDDIR) -o $(BIN_DIR)/$(TARGET).vcs -cc $(CC) -cpp $(CXX) \
		-assert disable_cover -override_timescale=1ns/1ps -full64 tb_bin $(TB_CC_SOURCES) \
		-CFLAGS "$(TB_CC_FLAGS)" -LDFLAGS "-L${FESVR}/lib" -lfesvr

########
# Util #
########

.PHONY: clean-work clean-bender clean-logs help

clean-work:
	rm -rf work

clean-bender:
	rm -rf $(ROOT)/Bender.lock $(ROOT)/.bender/

clean-logs:
	rm -rf $(LOGS_DIR)

clean-generated:
	rm -rf $(GENERATED_DIR)

# Help command
Blue=\033[1;34m
Black=\033[0m
help:
	@echo -e "${Blue}Makefile Targets${Black} for the ${Blue}Snitch Cluster${Black}"
	@echo -e "Use 'make <target>' where <target> is one of:"
	@echo -e ""
	@echo -e "${Blue}help           ${Black}Show an overview of all Makefile targets."
	@echo -e ""
	@echo -e "${Blue}$(BIN_DIR)/$(TARGET).vcs  ${Black}Build compilation script and compile all sources for VCS simulation."
	@echo -e "${Blue}$(BIN_DIR)/$(TARGET).vlt  ${Black}Build compilation script and compile all sources for Verilator simulation."
	@echo -e "${Blue}$(BIN_DIR)/$(TARGET).vsim ${Black}Build compilation script and compile all sources for Questasim simulation."
	@echo -e ""
	@echo -e "${Blue}sw               ${Black}Build all software."
	@echo -e ""
	@echo -e "${Blue}clean            ${Black}Clean everything except traces in logs directory."
	@echo -e "${Blue}clean-bender     ${Black}Clean Bender dependencies."
	@echo -e "${Blue}clean-sw         ${Black}Clean all software."
	@echo -e "${Blue}clean-generated  ${Black}Delete all generated files in the generated directory."
	@echo -e "${Blue}clean-logs       ${Black}Delete all traces in logs directory."
	@echo -e "${Blue}clean-vcs        ${Black}Clean all build directories and temporary files for VCS simulation."
	@echo -e "${Blue}clean-vlt        ${Black}Clean all build directories and temporary files for Verilator simulation."
	@echo -e "${Blue}clean-vsim       ${Black}Clean all build directories and temporary files for Questasim simulation."
	@echo -e ""
	@echo -e "Additional useful targets from the included Makefrag:"
	@echo -e "${Blue}traces           ${Black}Generate the better readable traces in .logs/trace_hart_<hart_id>.txt with spike-dasm."
	@echo -e "${Blue}annotate         ${Black}Annotate the better readable traces in .logs/trace_hart_<hart_id>.s with the source code related with the retired instructions."
