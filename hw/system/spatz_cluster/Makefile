# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# Author: Matheus Cavalcante, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
INSTALL_DIR := $(abspath $(SPATZ_DIR)/install)

# Include configuration
config_mk = $(abspath $(SPATZ_DIR)/cfg/config.mk)
include $(config_mk)

# build path
buildpath       ?= build
resultpath      ?= results
# questa library
library         ?= work
# dpi library
dpi_library     ?= work-dpi
# Top level module to compile
top_level       ?= testharness
# QuestaSim Version
questa_version  ?= 2021.3-kgf
# QuestaSim command
questa_cmd      ?= questa-$(questa_version)
# QuestaSim arguments
questa_args     ?=
# VCS Version
vcs_version     ?= 2020.03-kgf
# VCS command
vcs_cmd         ?= vcs-$(vcs_version)
# Path to the binaries
app_path        ?= $(abspath $(ROOT_DIR)/../software/bin)
# Bender
bender          ?= $(INSTALL_DIR)/bender/bender
# Verilator
verilator       ?= $(INSTALL_DIR)/verilator/bin/verilator
verilator_build ?= $(ROOT_DIR)/verilator_build
verilator_files ?= $(verilator_build)/files
verilator_top   ?= testharness
# Python
python          ?= python3
# Enable tracing
snitch_trace    ?= 0
# Enable spatz (automatically set when using spatz configuration)
spatz           ?= 0

# Check if the specified QuestaSim version exists
ifeq (, $(shell which $(questa_cmd)))
  # Spaces are needed for indentation here!
  $(warning "Specified QuestaSim version ($(questa_cmd)) not found in PATH $(PATH)")
  questa_cmd =
endif

QUESTASIM_HOME ?= /usr/pack/questa-$(questa_version)/questasim
VCS_HOME       ?= /usr/pack/vcs-$(vcs_version)

ifdef app
	preload ?= "$(app_path)/$(app)"
endif
ifdef preload
	questa_args += +PRELOAD=$(preload)
endif
questa_args += -sv_lib $(dpi_library)/spatz_dpi -work $(library)
questa_args += -suppress vsim-12070

# VCS options
vlogan_args += -assert svaext +v2k -override_timescale=1ns/1ps -kdb
ifdef preload
	vcs_args += +PRELOAD=$(preload)
endif

# DPI source files
dpi     := $(patsubst tb/dpi/%.cpp,$(buildpath)/$(dpi_library)/%.o,$(wildcard tb/dpi/*.cpp))
dpi_vcs := $(patsubst tb/dpi/%.cpp,$(buildpath)/$(dpi_library)/%_vcs.o,$(wildcard tb/dpi/*.cpp))

vlog_args += -suppress vlog-2583 -suppress vlog-13314 -suppress vlog-13233
vlog_args += -work $(library)
# Defines
vlog_defs += -DNUM_CORES=$(num_cores) -DNUM_BANKS=$(num_banks)
vlog_defs += -DVLEN=$(vlen)
vlog_defs += -DN_IPU=$(n_ipu) -DN_FPU=$(n_fpu)
vlog_defs += -DN_FU=$(shell awk 'BEGIN{print ($(n_ipu) > $(n_fpu)) ? $(n_ipu) : $(n_fpu)}')
vlog_defs += -DRVF=$(rvf) -DRVD=$(rvd)

.DEFAULT_GOAL := compile

# Build path
$(buildpath):
	mkdir -p $(buildpath)

# Bender
$(bender):
	make -C $(SPATZ_DIR) bender

################
# Modelsim     #
################

# Library
.PHONY: lib
lib: $(buildpath) $(buildpath)/$(library)
$(buildpath)/$(library):
	cd $(buildpath) && $(questa_cmd) vlib $(library) && chmod +w modelsim.ini; $(questa_cmd) vmap $(library) $(library)

# Compilation
.PHONY: compile
compile: lib $(buildpath) $(buildpath)/compile.tcl
$(buildpath)/compile.tcl: $(bender) $(config_mk) Makefile $(SPATZ_DIR)/Bender.yml $(shell find {src,tb} -type f)
	$(bender) script vsim --vlog-arg="$(vlog_args)" $(vlog_defs) -t rtl -t snitch_test -t spatz_test $(bender_defs) > $(buildpath)/compile.tcl
	echo "exit" >> $(buildpath)/compile.tcl
	cd $(buildpath) && $(questa_cmd) vsim -work $(library) -c -do compile.tcl

# Simulation
.PHONY: sim
sim: compile
	cd $(buildpath) && \
	$(questa_cmd) vsim -voptargs=+acc $(questa_args) $(library).$(top_level) -do ../scripts/questa/run.tcl
	./scripts/return_status.sh $(buildpath)/transcript

.PHONY: simc
simc: compile
	cd $(buildpath) && \
	$(questa_cmd) vsim -c $(questa_args) $(library).$(top_level) -do "run -a"
	./scripts/return_status.sh $(buildpath)/transcript

################
# Helper       #
################

# Clean targets
.PHONY: clean

clean:
	@rm -rf $(buildpath)
	@rm -rf $(verilator_build)
