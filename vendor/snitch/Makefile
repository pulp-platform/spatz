# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

BENDER ?= bender
REGGEN  = $(shell $(BENDER) path register_interface)/vendor/lowrisc_opentitan/util/regtool.py

GENERATED_DOCS_DIR = docs/generated
GENERATED_DOC_SRCS = $(GENERATED_DOCS_DIR)/peripherals.md

.PHONY: all doc-srcs docs
.PHONY: clean clean-docs

all: docs
clean: clean-docs

doc-srcs: $(GENERATED_DOC_SRCS)

docs: doc-srcs
	mkdocs build

clean-docs:
	rm -rf $(GENERATED_DOCS_DIR)

$(GENERATED_DOCS_DIR):
	mkdir -p $@

$(GENERATED_DOCS_DIR)/peripherals.md: hw/snitch_cluster/src/snitch_cluster_peripheral/snitch_cluster_peripheral_reg.hjson | $(GENERATED_DOCS_DIR)
	$(REGGEN) -d $< > $@
