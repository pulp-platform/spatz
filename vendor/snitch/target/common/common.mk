# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Makefile invocation
DEBUG ?= OFF  # ON to turn on wave logging

# Directories
SIM_DIR  ?= $(shell pwd)
TB_DIR   ?= $(SNITCH_ROOT)/target/common/test
UTIL_DIR ?= $(SNITCH_ROOT)/util
LOGS_DIR  = $(SIM_DIR)/logs

# Files
BENDER_LOCK ?= $(ROOT)/Bender.lock

# SEPP packages
QUESTA_SEPP    ?=
VCS_SEPP       ?=
VERILATOR_SEPP ?=

# External executables
BENDER       ?= bender
DASM         ?= spike-dasm
VLT          ?= $(VERILATOR_SEPP) verilator
VCS          ?= $(VCS_SEPP) vcs
VERIBLE_FMT  ?= verible-verilog-format
CLANG_FORMAT ?= clang-format
VSIM         ?= $(QUESTA_SEPP) vsim
VOPT         ?= $(QUESTA_SEPP) vopt
VLOG         ?= $(QUESTA_SEPP) vlog
VLIB         ?= $(QUESTA_SEPP) vlib
ADDR2LINE    ?= $(LLVM_BINROOT)/llvm-addr2line

# Internal executables
GENTRACE_PY      ?= $(UTIL_DIR)/trace/gen_trace.py
ANNOTATE_PY      ?= $(UTIL_DIR)/trace/annotate.py
EVENTS_PY        ?= $(UTIL_DIR)/trace/events.py
PERF_CSV_PY      ?= $(UTIL_DIR)/trace/perf_csv.py
LAYOUT_EVENTS_PY ?= $(UTIL_DIR)/trace/layout_events.py
EVENTVIS_PY      ?= $(UTIL_DIR)/trace/eventvis.py

VERILATOR_ROOT ?= $(dir $(shell $(VERILATOR_SEPP) which verilator))..
VLT_ROOT       ?= ${VERILATOR_ROOT}

MATCH_END := '/+incdir+/ s/$$/\/*\/*/'
MATCH_BGN := 's/+incdir+//g'
SED_SRCS  := sed -e ${MATCH_END} -e ${MATCH_BGN}

VSIM_BENDER   += -t test -t rtl -t simulation -t vsim
VSIM_SOURCES   = $(shell ${BENDER} script flist ${VSIM_BENDER} | ${SED_SRCS})
VSIM_BUILDDIR ?= work-vsim
VSIM_FLAGS    += -t 1ps
ifeq ($(DEBUG), ON)
VSIM_FLAGS    += -do "log -r /*; run -a"
VOPT_FLAGS     = +acc
else
VSIM_FLAGS    += -do "run -a"
endif

# VCS_BUILDDIR should to be the same as the `DEFAULT : ./work-vcs`
# in target/snitch_cluster/synopsys_sim.setup
VCS_BENDER   += -t test -t rtl -t simulation -t vcs
VCS_SOURCES   = $(shell ${BENDER} script flist ${VCS_BENDER} | ${SED_SRCS})
VCS_BUILDDIR := work-vcs

# fesvr is being installed here
FESVR         ?= ${MKFILE_DIR}work
FESVR_VERSION ?= 35d50bc40e59ea1d5566fbd3d9226023821b1bb6

VLT_BENDER   += -t rtl
VLT_SOURCES   = $(shell ${BENDER} script flist ${VLT_BENDER} | ${SED_SRCS})
VLT_BUILDDIR := work-vlt
VLT_FESVR     = $(VLT_BUILDDIR)/riscv-isa-sim
VLT_FLAGS    += -Wno-BLKANDNBLK
VLT_FLAGS    += -Wno-LITENDIAN
VLT_FLAGS    += -Wno-CASEINCOMPLETE
VLT_FLAGS    += -Wno-CMPCONST
VLT_FLAGS    += -Wno-WIDTH
VLT_FLAGS    += -Wno-WIDTHCONCAT
VLT_FLAGS    += -Wno-UNSIGNED
VLT_FLAGS    += -Wno-UNOPTFLAT
VLT_FLAGS    += -Wno-fatal
VLT_FLAGS    += --unroll-count 1024
VLT_CFLAGS   += -std=c++14 -pthread
VLT_CFLAGS   +=-I ${VLT_BUILDDIR} -I $(VLT_ROOT)/include -I $(VLT_ROOT)/include/vltstd -I $(VLT_FESVR)/include -I $(TB_DIR) -I ${MKFILE_DIR}/test

ANNOTATE_FLAGS      ?= -q --keep-time --addr2line=$(ADDR2LINE)
LAYOUT_EVENTS_FLAGS ?= --cfg=$(CFG)

# We need a recent LLVM installation (>11) to compile Verilator.
# We also need to link the binaries with LLVM's libc++.
# Define CLANG_PATH to be the path of your Clang installation.

ifneq (${CLANG_PATH},)
    CLANG_CC       := $(CLANG_PATH)/bin/clang
    CLANG_CXX      := $(CLANG_PATH)/bin/clang++
    CLANG_CXXFLAGS := -nostdinc++ -isystem $(CLANG_PATH)/include/c++/v1
    CLANG_LDFLAGS  := -nostdlib++ -fuse-ld=lld -L ${CLANG_PATH}/lib -Wl,-rpath,${CLANG_PATH}/lib -lc++
else
    CLANG_CC       ?= clang
    CLANG_CXX      ?= clang++
    CLANG_CXXFLAGS := ""
    CLANG_LDFLAGS  := ""
endif

# If requested, build verilator with LLVM and add llvm c/ld flags
ifeq ($(VLT_USE_LLVM),ON)
    CC         = $(CLANG_CC)
    CXX        = $(CLANG_CXX)
    CFLAGS     = $(CLANG_CXXFLAGS)
    CXXFLAGS   = $(CLANG_CXXFLAGS)
    LDFLAGS    = $(CLANG_LDFLAGS)
    VLT_FLAGS += --compiler clang
    VLT_FLAGS += -CFLAGS "${CLANG_CXXFLAGS}"
    VLT_FLAGS += -LDFLAGS "${CLANG_LDFLAGS}"
endif

VLOGAN_FLAGS := -assert svaext
VLOGAN_FLAGS += -assert disable_cover
VLOGAN_FLAGS += -full64
VLOGAN_FLAGS += -kdb
VHDLAN_FLAGS := -full64
VHDLAN_FLAGS += -kdb

# default on target `all`
all:

#################
# Prerequisites #
#################
# Eventually it could be an option to package this statically using musl libc.
work/${FESVR_VERSION}_unzip:
	mkdir -p $(dir $@)
	wget -O $(dir $@)/${FESVR_VERSION} https://github.com/riscv/riscv-isa-sim/tarball/${FESVR_VERSION}
	tar xfm $(dir $@)${FESVR_VERSION} --strip-components=1 -C $(dir $@)
	touch $@

work/lib/libfesvr.a: work/${FESVR_VERSION}_unzip
	cd $(dir $<)/ && ./configure --prefix `pwd`
	make -C $(dir $<) install-config-hdrs install-hdrs libfesvr.a
	mkdir -p $(dir $@)
	cp $(dir $<)libfesvr.a $@

# Build fesvr seperately for verilator since this might use different compilers
# and libraries than modelsim/vcs and
$(VLT_FESVR)/${FESVR_VERSION}_unzip:
	mkdir -p $(dir $@)
	wget -O $(dir $@)/${FESVR_VERSION} https://github.com/riscv/riscv-isa-sim/tarball/${FESVR_VERSION}
	tar xfm $(dir $@)${FESVR_VERSION} --strip-components=1 -C $(dir $@)
	touch $@

$(VLT_BUILDDIR)/lib/libfesvr.a: $(VLT_FESVR)/${FESVR_VERSION}_unzip
	cd $(dir $<)/ && ./configure --prefix `pwd` \
        CC=${CC} CXX=${CXX} CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}"
	$(MAKE) -C $(dir $<) install-config-hdrs install-hdrs libfesvr.a
	mkdir -p $(dir $@)
	cp $(dir $<)libfesvr.a $@

#############
# Verilator #
#############
# Takes the top module name as an argument.
define VERILATE
	mkdir -p $(dir $@)
	$(BENDER) script verilator ${VLT_BENDER} > $(dir $@)files
	$(VLT) \
		--Mdir $(dir $@) -f $(dir $@)files $(VLT_FLAGS) \
		-j $(shell nproc) --cc --build --top-module $(1)
	touch $@
endef

#######
# VCS #
#######
$(VCS_BUILDDIR)/compile.sh:
	mkdir -p $(VCS_BUILDDIR)
	${BENDER} script vcs ${VCS_BENDER} --vlog-arg="${VLOGAN_FLAGS}" --vcom-arg="${VHDLAN_FLAGS}" > $@
	chmod +x $@
	$(VCS_SEPP) $@ > $(VCS_BUILDDIR)/compile.log

########
# Util #
########

# Common rule to generate C header with REGGEN
# $1: target name, $2: prerequisite (hjson description file)
define reggen_generate_header
	@echo "[REGGEN] Generating $1"
	@$(REGGEN) -D -o $1 $2
	@$(CLANG_FORMAT) -i $1
endef

# Arg 1: binary
# Arg 2: max size in bytes
define BINARY_SIZE_CHECK
  echo "Binary size: $$(stat -c %s $(1))B"
  @[ "$$(stat -c %s $(1))" -lt "$(2)" ] || (echo "Binary exceeds specified size of $(2)B"; exit 1)
endef

##########
# Traces #
##########

DASM_TRACES      = $(shell (ls $(LOGS_DIR)/trace_hart_*.dasm 2>/dev/null))
TXT_TRACES       = $(shell (echo $(DASM_TRACES) | sed 's/\.dasm/\.txt/g'))
PERF_TRACES      = $(shell (echo $(DASM_TRACES) | sed 's/trace_hart/hart/g' | sed 's/.dasm/_perf.json/g'))
ANNOTATED_TRACES = $(shell (echo $(DASM_TRACES) | sed 's/\.dasm/\.s/g'))
DIFF_TRACES      = $(shell (echo $(DASM_TRACES) | sed 's/\.dasm/\.diff/g'))

GENTRACE_OUTPUTS = $(TXT_TRACES) $(PERF_TRACES)
ANNOTATE_OUTPUTS = $(ANNOTATED_TRACES)
PERF_CSV         = $(LOGS_DIR)/perf.csv
EVENT_CSV        = $(LOGS_DIR)/event.csv
TRACE_CSV        = $(LOGS_DIR)/trace.csv
TRACE_JSON       = $(LOGS_DIR)/trace.json

.PHONY: traces annotate perf-csv event-csv layout
traces: $(GENTRACE_OUTPUTS)
annotate: $(ANNOTATE_OUTPUTS)
perf-csv: $(PERF_CSV)
event-csv: $(EVENT_CSV)
layout: $(TRACE_CSV) $(TRACE_JSON)

$(LOGS_DIR)/trace_hart_%.txt $(LOGS_DIR)/hart_%_perf.json: $(LOGS_DIR)/trace_hart_%.dasm $(GENTRACE_PY)
	$(DASM) < $< | $(PYTHON) $(GENTRACE_PY) --permissive -d $(LOGS_DIR)/hart_$*_perf.json > $(LOGS_DIR)/trace_hart_$*.txt

# Generate source-code interleaved traces for all harts. Reads the binary from
# the logs/.rtlbinary file that is written at start of simulation in the vsim script
BINARY ?= $(shell cat $(SIM_DIR)/.rtlbinary)
$(LOGS_DIR)/trace_hart_%.s: $(LOGS_DIR)/trace_hart_%.txt ${ANNOTATE_PY}
	$(PYTHON) ${ANNOTATE_PY} ${ANNOTATE_FLAGS} -o $@ $(BINARY) $<
$(LOGS_DIR)/trace_hart_%.diff: $(LOGS_DIR)/trace_hart_%.txt ${ANNOTATE_PY}
	$(PYTHON) ${ANNOTATE_PY} ${ANNOTATE_FLAGS} -o $@ $(BINARY) $< -d

$(PERF_CSV): $(PERF_TRACES) $(PERF_CSV_PY)
	$(PYTHON) $(PERF_CSV_PY) -o $@ -i $(PERF_TRACES)

$(EVENT_CSV): $(PERF_TRACES) $(PERF_CSV_PY)
	$(PYTHON) $(PERF_CSV_PY) -o $@ -i $(PERF_TRACES) --filter tstart tend

$(TRACE_CSV): $(EVENT_CSV) $(LAYOUT_FILE) $(LAYOUT_EVENTS_PY)
	$(PYTHON) $(LAYOUT_EVENTS_PY) $(LAYOUT_EVENTS_FLAGS) $(EVENT_CSV) $(LAYOUT_FILE) -o $@

$(TRACE_JSON): $(TRACE_CSV) $(EVENTVIS_PY)
	$(PYTHON) $(EVENTVIS_PY) -o $@ $(TRACE_CSV)
