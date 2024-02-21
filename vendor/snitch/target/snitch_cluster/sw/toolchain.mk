# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>
# Viviane Potocnik <vivianep@iis.ee.ethz.ch>

######################
# Invocation options #
######################

DEBUG ?= OFF # ON to turn on debugging symbols

###################
# Build variables #
###################

# Compiler toolchain
LLVM_BINROOT    ?= $(dir $(shell which riscv32-unknown-elf-clang))
LLVM_VER        ?= $(shell $(LLVM_BINROOT)/llvm-config --version | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')
RISCV_CC        ?= $(LLVM_BINROOT)/clang
RISCV_LD        ?= $(LLVM_BINROOT)/ld.lld
RISCV_AR        ?= $(LLVM_BINROOT)/llvm-ar
RISCV_OBJCOPY   ?= $(LLVM_BINROOT)/llvm-objcopy
RISCV_OBJDUMP   ?= $(LLVM_BINROOT)/llvm-objdump
RISCV_DWARFDUMP ?= $(LLVM_BINROOT)/llvm-dwarfdump

# Compiler flags
RISCV_CFLAGS += $(addprefix -I,$(INCDIRS))
RISCV_CFLAGS += -mcpu=snitch
RISCV_CFLAGS += -menable-experimental-extensions
RISCV_CFLAGS += -mabi=ilp32d
RISCV_CFLAGS += -mcmodel=medany
# RISCV_CFLAGS += -mno-fdiv # Not supported by Clang
# RISCV_CFLAGS += -ffast-math
RISCV_CFLAGS += -fno-builtin-printf
RISCV_CFLAGS += -fno-builtin-sqrtf
RISCV_CFLAGS += -fno-common
RISCV_CFLAGS += -fopenmp
RISCV_CFLAGS += -ftls-model=local-exec
RISCV_CFLAGS += -O3
ifeq ($(DEBUG), ON)
RISCV_CFLAGS += -g
endif
# Required by math library to avoid conflict with stdint definition
RISCV_CFLAGS += -D__DEFINED_uint64_t

# Linker flags
RISCV_LDFLAGS += -fuse-ld=$(RISCV_LD)
RISCV_LDFLAGS += -nostartfiles
RISCV_LDFLAGS += -nostdlib
RISCV_LDFLAGS += -lc
RISCV_LDFLAGS += -L$(LLVM_BINROOT)/../lib/clang/$(LLVM_VER)/lib/
RISCV_LDFLAGS += -lclang_rt.builtins-riscv32

# Archiver flags
RISCV_ARFLAGS = rcs

# Objdump flags
RISCV_OBJDUMP_FLAGS += --mcpu=snitch
RISCV_OBJDUMP_FLAGS += -D
