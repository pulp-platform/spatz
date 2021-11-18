# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Domenic Wüthrich, ETH Zurich

SHELL = /usr/bin/env bash

ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SPATZ_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$SPATZ_DIR)
# Include configuration
include $(SPATZ_DIR)/config/config.mk

INSTALL_DIR        ?= $(SPATZ_DIR)/install
GCC_INSTALL_DIR    ?= $(INSTALL_DIR)/riscv-gcc
LLVM_INSTALL_DIR   ?= $(INSTALL_DIR)/riscv-llvm

RISCV_XLEN    ?= 32

RISCV_ABI     ?= ilp32
RISCV_TARGET  ?= riscv$(RISCV_XLEN)-unknown-elf
# LLVM compiler -march
RISCV_ARCH    ?= rv$(RISCV_XLEN)ima
RISCV_PREFIX  ?= $(LLVM_INSTALL_DIR)/bin/
RISCV_CC      ?= $(RISCV_PREFIX)clang
RISCV_CXX     ?= $(RISCV_PREFIX)clang++
RISCV_OBJDUMP ?= $(RISCV_PREFIX)llvm-objdump
RISCV_OBJCOPY ?= $(RISCV_PREFIX)llvm-objcopy
RISCV_AS      ?= $(RISCV_PREFIX)llvm-as
RISCV_AR      ?= $(RISCV_PREFIX)llvm-ar
RISCV_LD      ?= $(RISCV_PREFIX)ld.lld
RISCV_STRIP   ?= $(RISCV_PREFIX)llvm-strip

# Defines
DEFINES += -DPRINTF_DISABLE_SUPPORT_FLOAT -DPRINTF_DISABLE_SUPPORT_LONG_LONG -DPRINTF_DISABLE_SUPPORT_PTRDIFF_T
DEFINES += -DVLEN=$(vlen) -DN_IPU=$(n_ipu)

# Common flags
RISCV_WARNINGS += -Wunused-variable -Wall -Wextra -Wno-unused-command-line-argument # -Werror

# LLVM Flags
LLVM_FLAGS     ?= -march=rv32imv0p10 -mabi=$(RISCV_ABI) -menable-experimental-extensions -mno-relax -fuse-ld=lld
RISCV_FLAGS    ?= $(LLVM_FLAGS) -mcmodel=medany -I$(ROOT_DIR) -std=gnu99 -O3 -ffast-math -fno-common -fno-builtin-printf $(DEFINES) $(RISCV_WARNINGS)
RISCV_CCFLAGS  ?= $(RISCV_FLAGS)
RISCV_CXXFLAGS ?= $(RISCV_FLAGS)
RISCV_LDFLAGS  ?= -static -nostartfiles -lm #-L$(ROOT_DIR) #--target=$(RISCV_TARGET) --sysroot=$(GCC_INSTALL_DIR)/$(RISCV_TARGET) --gcc-toolchain=$(GCC_INSTALL_DIR)

RISCV_OBJDUMP_FLAGS ?= --mattr=+experimental-v

LINKER_SCRIPT ?= $(ROOT_DIR)/arch.link.ld
RUNTIME ?= $(ROOT_DIR)/crt0.S.o $(ROOT_DIR)/printf.c.o $(ROOT_DIR)/string.c.o $(ROOT_DIR)/serial.c.o

%.h.pch: %.h
	$(RISCV_CC) $(RISCV_CCFLAGS) -x c-header $< -o $@

%.S.o: %.S
	$(RISCV_CC) $(RISCV_CCFLAGS) -c $< -o $@

%.c.o: %.c
	$(RISCV_CC) $(RISCV_CCFLAGS) -c $< -o $@

%.cpp.o: %.cpp
	$(RISCV_CXX) $(RISCV_CXXFLAGS) -c $< -o $@

%.ld: %.ld.c
	$(RISCV_CC) -P -E $(DEFINES) $< -o $@
