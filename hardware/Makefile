# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Domenic Wüthrich, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
INSTALL_DIR := $(abspath $(ROOT_DIR)/../install)

# Include configuration
config_file := $(ROOT_DIR)/../config/config.mk
include $(abspath $(ROOT_DIR)/../config/config.mk)

# build path
buildpath      ?= build
resultpath     ?= results
# questa library
library        ?= work
# dpi library
#dpi_library    ?= work-dpi
# Top level module to compile
top_level      ?= spatz_tb
# QuestaSim Version
questa_version ?= 2021.2
# QuestaSim command
questa_cmd     ?= questa-$(questa_version)
# QuestaSim arguments
questa_args    ?=
# Path to the binaries
app_path       ?= $(abspath $(ROOT_DIR)/../apps/bin)
# Bender
bender         ?= $(INSTALL_DIR)/bender/bender

# Check if the specified QuestaSim version exists
ifeq (, $(shell which $(questa_cmd)))
  # Spaces are needed for indentation here!
  $(warning "Specified QuestaSim version ($(questa_cmd)) not found in PATH $(PATH)")
  questa_cmd =
endif

questa_args += +UVM_NO_RELNOTES
ifdef app
	preload ?= "$(app_path)/$(app)"
endif
ifdef preload
	questa_args += +PRELOAD=$(preload)
endif
#questa_args += -sv_lib $(dpi_library)/spatz_dpi 
questa_args += -work $(library) -voptargs=+acc
questa_args += -suppress vsim-3009 -suppress vopt-7033

# DPI source files
dpi   := $(patsubst tb/dpi/%.cc,$(buildpath)/$(dpi_library)/%.o,$(wildcard tb/dpi/*.cc))

vlog_args += -suppress vlog-2583 -suppress vlog-13314 -suppress vlog-13233
vlog_args += -work $(library)

# Defines
bender_defs += --define VLEN=$(vlen)

# Default target
all: compile

# Build path
$(buildpath):
	mkdir -p $(buildpath)

# Bender
bender:
	@[ -x ./bender ] && echo "Bender already exists." || \
	(cat $(SPATZ_DIR)/scripts/bender-init | bash -s -- 0.21.0)
	@echo "$$(./bender --version) available."

# Library
.PHONY: lib
lib: $(buildpath) $(buildpath)/$(library)
$(buildpath)/$(library):
	cd $(buildpath) && $(questa_cmd) vlib $(library) && $(questa_cmd) vmap $(library) $(library)

# Compilation
.PHONY: compile
compile: lib $(buildpath) bender $(buildpath)/compile_$(config).tcl
$(buildpath)/compile_$(config).tcl: $(config_file) Makefile ../Bender.yml $(shell find src -type f) $(shell find ../config -type f) $(shell find include -type f) $(shell find tb -type f) $(shell find deps -type f)
	$(bender) script vsim --vlog-arg="$(vlog_args)" -t rtl -t asic -t spatz_test $(bender_defs) > $(buildpath)/compile_$(config).tcl
	echo "exit" >> $(buildpath)/compile_$(config).tcl
	cd $(buildpath) && $(questa_cmd) vsim -work $(library) -c -do compile_$(config).tcl
	# Remove the file if compilation did not succeed
	if [ `cat $(buildpath)/transcript | grep "\*\* Error" | wc -l` -ne 0 ]; then rm $(buildpath)/compile_$(config).tcl; fi

# Simulation
.PHONY: sim
sim: compile
	cd $(buildpath) && \
	$(questa_cmd) vsim $(questa_args) $(library).$(top_level) -do ../scripts/run.tcl
	./scripts/return_status.sh $(buildpath)/transcript

.PHONY: simc
simc: compile
	cd $(buildpath) && \
	$(questa_cmd) vsim -c $(questa_args) $(library).$(top_level) -do "run -a"
	./scripts/return_status.sh $(buildpath)/transcript

# DPIs
.PHONY: dpi
dpi: $(buildpath)/$(dpi_library)/spatz_dpi.so

$(buildpath)/$(dpi_library)/%.o: tb/dpi/%.cpp
	mkdir -p $(buildpath)/$(dpi_library)
	$(CXX) -shared -fPIC -std=c++11 -Bsymbolic -c $< -I$(QUESTASIM_HOME)/include -o $@

$(buildpath)/$(dpi_library)/spatz_dpi.so: $(dpi)
	mkdir -p $(buildpath)/$(dpi_library)
	$(CXX) -shared -m64 -o $(buildpath)/$(dpi_library)/spatz_dpi.so $^

# Clean targets
.PHONY: clean
clean:
	rm -rf $(buildpath)
