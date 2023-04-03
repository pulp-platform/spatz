# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante, ETH Zurich

#####################
##  Spatz Cluster  ##
#####################

# Number of cores
num_cores ?= 2

# Number of banks
num_banks ?= 16

# Length of single vector register
vlen ?= 512

# Number of IPUs per Spatz
n_ipu ?= 4

# Number of FPUs per Spatz
n_fpu ?= 0
