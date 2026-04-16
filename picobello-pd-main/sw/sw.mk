# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

PB_PD_SW_DIR = $(PB_ROOT)/pd/sw

CHS_SW_INCLUDES += -I$(PB_PD_SW_DIR)/include

###############
##  Library  ##
###############

PB_PD_SW_LIB_SRCS_C = $(wildcard $(PB_PD_SW_DIR)/lib/*.c)
PB_PD_SW_LIB_SRCS_O = $(PB_PD_SW_LIB_SRCS_C:.c=.o)

PB_PD_SW_LIB = $(PB_PD_SW_DIR)/lib/libpicobellopd.a
CHS_SW_LIBS += $(PB_PD_SW_LIB)

$(PB_PD_SW_LIB_SRCS_O): $(PB_GEN_DIR)/pb_addrmap.h
$(PB_PD_SW_LIB): $(PB_PD_SW_LIB_SRCS_O)
	rm -f $@
	$(CHS_SW_AR) $(CHS_SW_ARFLAGS) -rcsv $@ $^

.PHONY: pb-pd-sw-lib clean-pb-pd-sw-lib
pb-pd-sw-lib: $(PB_PD_SW_LIB)
clean-pb-pd-sw-lib:
	rm -f $(PB_PD_SW_LIB)
	rm -f $(PB_PD_SW_LIB_SRCS_O)

#############
##  Tests  ##
#############

PB_PD_CHS_SW_TEST_SRC = $(wildcard $(PB_PD_SW_DIR)/tests/*.c)
# We add it to the general list of tests to be built
PB_CHS_SW_TEST_SRC += $(PB_PD_CHS_SW_TEST_SRC)

# Add library as a prerequisite for the cheshire tests
$(PB_CHS_SW_TEST_SRC:.c=.spm.elf): $(PB_PD_SW_LIB)
$(PB_CHS_SW_TEST_SRC:.c=.spm.dump): $(PB_PD_SW_LIB)
