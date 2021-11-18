# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Domenic Wüthrich, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SOFTWARE_DIR := $(abspath $(ROOT_DIR)/..)
RUNTIME_DIR := $(abspath $(SOFTWARE_DIR)/runtime)
BIN_DIR := $(abspath $(SOFTWARE_DIR)/bin)
APPS_DIR := $(ROOT_DIR)
TESTS_DIR := $(abspath $(ROOT_DIR)/riscv-tests/isa)
# This will overwrite the ROOT_DIR variable from the included makefile
include $(RUNTIME_DIR)/runtime.mk
include $(RUNTIME_DIR)/riscv_tests.mk

APPS := $(patsubst $(APPS_DIR)/%/main.c,%,$(shell find $(APPS_DIR) -name "main.c"))
BINARIES := $(addprefix $(BIN_DIR)/,$(APPS))
SPATZ_BINARIES := $(addprefix $(BIN_DIR)/rv32uiv/, $(spatz_tests))

# Make all applications
all: $(BINARIES)

$(APPS): % : $(BIN_DIR)/% $(APPS_DIR)/Makefile $(shell find $(RUNTIME_DIR)/**.{S,c,h,ld} -type f)

.PHONY: $(BINARIES)
$(BINARIES): $(BIN_DIR)/%: %/main.c.o $(RUNTIME) $(LINKER_SCRIPT)
	mkdir -p $(dir $@)
	$(RISCV_CC) -Iinclude -I$(RUNTIME_DIR) $(RISCV_CCFLAGS) $(RISCV_LDFLAGS) -o $@ $< $(RUNTIME) -T$(RUNTIME_DIR)/arch.link.ld
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) -D $@ > $@.dump
	$(RISCV_STRIP) $@ -S --strip-unneeded

riscv_tests: $(SPATZ_BINARIES)

TESTS_rv32uiv := $(addprefix $(BIN_DIR)/rv32uiv, $(rv_32uiv_spatz_tests))

$(BIN_DIR)/rv32uiv/rv32uiv-spatz-%: $(TESTS_DIR)/rv32uiv/%.c $(RUNTIME) $(LINKER_SCRIPT)
	mkdir -p $(BIN_DIR)/rv32uiv
	$(RISCV_CC) -Iinclude -I$(RUNTIME_DIR) -I$(TESTS_DIR)/macros/scalar -I$(TESTS_DIR)/macros/vector $(RISCV_CCFLAGS) $(RISCV_LDFLAGS) -o $@ $< $(RUNTIME) -T$(RUNTIME_DIR)/arch.link.ld
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) -D $@ > $@.dump
	$(RISCV_STRIP) $@ -S --strip-unneeded

.PHONY: clean
clean:
	rm -vf $(BINARIES)
	rm -vf $(addsuffix .dump,$(BINARIES))
	rm -vf $(addsuffix /main.c.o,$(APPS))
	rm -vf $(RUNTIME)
	rm -vf $(LINKER_SCRIPT)

.INTERMEDIATE: $(addsuffix /main.c.o,$(APPS))
