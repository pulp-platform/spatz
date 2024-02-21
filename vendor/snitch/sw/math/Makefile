# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>
# Viviane Potocnik, ETH Zurich <vivianep@iis.ee.ethz.ch>

# Usage of absolute paths is required to externally include
# this Makefile from multiple different locations
MK_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

###############
# Directories #
###############

BUILDDIR ?= $(abspath build)
SRC_DIR   = $(MK_DIR)/src/math
BITS_DIR  = $(MK_DIR)/include/bits

###################
# Build variables #
###################

INCDIRS += $(MK_DIR)/arch/riscv64/
INCDIRS += $(MK_DIR)/arch/generic
INCDIRS += $(MK_DIR)/src/include
INCDIRS += $(MK_DIR)/src/internal
INCDIRS += $(MK_DIR)/include/bits
INCDIRS += $(MK_DIR)/include

SRCS = $(abspath $(wildcard $(SRC_DIR)/*.c))

###########
# Outputs #
###########

ALLTYPES_H = $(BITS_DIR)/alltypes.h

OBJS        = $(addprefix $(BUILDDIR)/,$(addsuffix .o,$(basename $(notdir $(SRCS)))))
DEPS        = $(addprefix $(BUILDDIR)/,$(addsuffix .d,$(basename $(notdir $(SRCS)))))
LIB         = $(BUILDDIR)/libmath.a
DUMP        = $(BUILDDIR)/libmath.dump
ALL_OUTPUTS = $(LIB) $(DUMP)

#########
# Rules #
#########

.PHONY: all
all: $(ALL_OUTPUTS)

.PHONY: clean
clean:
	rm -rf $(BITS_DIR)
	rm -f $(ALLTYPES_H)
	rm -rf $(BUILDDIR)

$(BITS_DIR):
	mkdir -p $@

$(ALLTYPES_H): | $(BITS_DIR)
	sed -f $(MK_DIR)/tools/mkalltypes.sed $(MK_DIR)/arch/riscv64/bits/alltypes.h.in $(MK_DIR)/include/alltypes.h.in > $@

$(DEPS): $(ALLTYPES_H)

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/%.o: $(SRC_DIR)/%.S | $(BUILDDIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRC_DIR)/%.c | $(BUILDDIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -c $< -o $@

$(BUILDDIR)/%.d: $(SRC_DIR)/%.c | $(BUILDDIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -MM -MT '$(@:.d=.o)' $< > $@

$(LIB): $(OBJS) | $(BUILDDIR)
	$(RISCV_AR) $(RISCV_ARFLAGS) $@ $^

$(DUMP): $(LIB) | $(BUILDDIR)
	$(RISCV_OBJDUMP) -D $< > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
