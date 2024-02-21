# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>

TARGET ?= all
SELECT_RUNTIME ?= rtl

ifeq ($(SELECT_RUNTIME), banshee)
RUNTIME = runtime/banshee
else
RUNTIME = runtime/rtl
endif

SUBDIRS = runtime/banshee runtime/rtl math apps tests

.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

# Explicit dependency of apps on runtime
apps: $(RUNTIME) math
	$(MAKE) -C $@ TARGET=$(TARGET)

# Explicit dependency of tests on runtime
tests: $(RUNTIME) math
	$(MAKE) -C $@ $(TARGET)

runtime/rtl runtime/banshee math:
	$(MAKE) -C $@ $(TARGET)
