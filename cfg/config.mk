# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante, ETH Zurich

####################
##  Spatz flavor  ##
####################

# Choose a Spatz flavor, either "spatz_int_cluster" or "spatz_fpu_cluster".
# Check the README for more details
ifndef config
  ifdef SPATZ_CONFIGURATION
    config := $(SPATZ_CONFIGURATION)
  else
    # Default configuration, if neither `config` nor `SPATZ_CONFIGURATION` was found
    config := spatz_fpu_cluster
  endif
endif
include $(SPATZ_DIR)/cfg/$(config).mk

######################
##  Default values  ##
######################

# AXI width
axi_dw ?= 512
axi_aw ?= 32
axi_iw ?= 2
axi_uw ?= 1

# Deactivate the FPU
n_fpu ?= 0

# Number of words in each TCDM bank
tcdm_bank_depth ?= 2048

# Boot address (in dec)
boot_addr ?= 4096

# Deactivate single-precision (rvf) and double-precision (rvd) floating-point
rvf ?= 0
rvd ?= 0
