# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>

# Usage of absolute paths is required to externally include
# this Makefile from multiple different locations
MK_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(MK_DIR)/../toolchain.mk

###############
# Directories #
###############

# Fixed paths in repository tree
ROOT     := $(abspath $(MK_DIR)/../../../..)
SNRT_DIR := $(ROOT)/sw/snRuntime
ifeq ($(SELECT_RUNTIME), banshee)
RUNTIME_DIR  := $(ROOT)/target/snitch_cluster/sw/runtime/banshee
RISCV_CFLAGS += -DBIST
else
RUNTIME_DIR := $(ROOT)/target/snitch_cluster/sw/runtime/rtl
endif
MATH_DIR := $(ROOT)/target/snitch_cluster/sw/math

# Paths relative to the app including this Makefile
BUILDDIR = $(abspath build)

###################
# Build variables #
###################

INCDIRS += $(RUNTIME_DIR)/src
INCDIRS += $(RUNTIME_DIR)/../common
INCDIRS += $(SNRT_DIR)/api
INCDIRS += $(SNRT_DIR)/api/omp
INCDIRS += $(SNRT_DIR)/src
INCDIRS += $(SNRT_DIR)/src/omp
INCDIRS += $(ROOT)/sw/blas
INCDIRS += $(ROOT)/sw/deps/riscv-opcodes
INCDIRS += $(ROOT)/sw/math/include

LIBS  = $(MATH_DIR)/build/libmath.a
LIBS += $(RUNTIME_DIR)/build/libsnRuntime.a

LIBDIRS  = $(dir $(LIBS))
LIBNAMES = $(patsubst lib%,%,$(notdir $(basename $(LIBS))))

RISCV_LDFLAGS += -L$(abspath $(RUNTIME_DIR))
RISCV_LDFLAGS += -T$(abspath $(SNRT_DIR)/base.ld)
RISCV_LDFLAGS += $(addprefix -L,$(LIBDIRS))
RISCV_LDFLAGS += $(addprefix -l,$(LIBNAMES))

###########
# Outputs #
###########

ELF         = $(abspath $(addprefix $(BUILDDIR)/,$(addsuffix .elf,$(APP))))
DEP         = $(abspath $(addprefix $(BUILDDIR)/,$(addsuffix .d,$(APP))))
DUMP        = $(abspath $(addprefix $(BUILDDIR)/,$(addsuffix .dump,$(APP))))
DWARF       = $(abspath $(addprefix $(BUILDDIR)/,$(addsuffix .dwarf,$(APP))))
ALL_OUTPUTS = $(ELF) $(DEP) $(DUMP) $(DWARF)

#########
# Rules #
#########

.PHONY: all
all: $(ALL_OUTPUTS)

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $@

$(DEP): $(SRCS) | $(BUILDDIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -MM -MT '$(ELF)' $< > $@

$(ELF): $(SRCS) $(DEP) $(LIBS) | $(BUILDDIR)
	$(RISCV_CC) $(RISCV_CFLAGS) $(RISCV_LDFLAGS) $(SRCS) -o $@

$(DUMP): $(ELF) | $(BUILDDIR)
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) $< > $@

$(DWARF): $(ELF) | $(BUILDDIR)
	$(RISCV_DWARFDUMP) $< > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif
