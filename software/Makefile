# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Domenic Wüthrich, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
APPS_DIR := $(ROOT_DIR)/apps
RUNTIME_DIR := $(ROOT_DIR)/runtime
TOOLCHAIN_DIR := $(abspath $(ROOT_DIR)/../toolchain)
# This will overwrite the ROOT_DIR variable from the included makefile
include $(RUNTIME_DIR)/runtime.mk

# Applications
.PHONY: apps
apps:
	make -C $(APPS_DIR) all

.PHONY: clean-apps
clean-apps:
	make -C $(APPS_DIR) clean

.PHONY: clean
clean: clean-apps
