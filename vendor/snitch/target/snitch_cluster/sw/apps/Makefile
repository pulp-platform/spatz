# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>
# Viviane Potocnik <vivianep@iis.ee.ethz.ch>

SUBDIRS  = lto
SUBDIRS += nop
SUBDIRS += blas/axpy
SUBDIRS += blas/gemm
SUBDIRS += dnn/batchnorm
SUBDIRS += dnn/conv2d
SUBDIRS += dnn/fusedconv
SUBDIRS += dnn/gelu
SUBDIRS += dnn/layernorm
SUBDIRS += dnn/maxpool
SUBDIRS += dnn/softmax
SUBDIRS += dnn/flashattention_2
SUBDIRS += dnn/concat
SUBDIRS += dnn/fused_concat_linear
SUBDIRS += montecarlo/pi_estimation

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(TARGET)
