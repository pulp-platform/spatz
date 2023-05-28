# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Noah Huetter <huettern@iis.ee.ethz.ch>

REGTOOL=`../../../../../install/bender/bender path register_interface`/vendor/lowrisc_opentitan/util/regtool.py
PYTHON3=$(shell which python)
CLANG_FORMAT=$(shell which clang-format-10.0.1)

SCHEMA=spatz_cluster_peripheral_reg.hjson

PKG=spatz_cluster_peripheral_reg_pkg.sv
TOP=spatz_cluster_peripheral_reg_top.sv
CDEF=../../../../../sw/snRuntime/include/spatz_cluster_peripheral.h

all: $(PKG) $(TOP) $(CDEF)

$(PKG): $(SCHEMA)
	$(PYTHON3) $(REGTOOL) -r -t $(dir $@) $<
$(TOP): $(SCHEMA)
	$(PYTHON3) $(REGTOOL) -r -t $(dir $@) $<

%.h: $(SCHEMA)
	$(PYTHON3) $(REGTOOL) -D -o $@ $<
	$(CLANG_FORMAT) -i $@
