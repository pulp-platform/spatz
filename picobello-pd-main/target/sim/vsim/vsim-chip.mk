# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Author: Tim Fischer <fischeti@iis.ee.ethz.ch>
# Edited by: Cyrill Durrer <cdurrer@iis.ee.ethz.ch>
# Edited by: Andrea Belano <andrea.belano2@unibo.it>
# Edited by: Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

VSIM ?= vsim
VOPT ?= vopt
VLIB ?= vlib
VSIM_DIR_CHIP  = $(PB_PD_DIR)/target/sim/vsim
VSIM_WORK_CHIP = $(VSIM_DIR_CHIP)/work

NETLISTS_DIR   = $(PB_PD_DIR)/tsmc7/src/netlist
SDF_DIR        = $(PB_PD_DIR)/tsmc7/sdf
VCD_DIR        = $(PB_PD_DIR)/tsmc7/vcd
HANDOFF_DIR    = /home/fischeti/picobello_handoff/handoff/latest
DELIVERY_DIR   = /home/fischeti/picobello_handoff/deliverables
FUSION_OUT_DIR = $(PB_PD_DIR)/tsmc7/fusion/out
CONFIG_CHIP    = tb_picobello_chip_config

FUSION_RUN     ?= DELIVERY
NETLISTS       ?= OFF
NETLISTS_FILES ?= 
SDF            ?= OFF
SDF_FILES      ?= 
VCD            ?= OFF 
VCD_START      ?= 0
VCD_DURATION   ?= -1
ACC            ?= ON
N_NET_CLUSTER  ?= 1
N_NET_MEM      ?= 1

NETLISTS_DIR   := $(NETLISTS_DIR)/$(FUSION_RUN)
SDF_DIR        := $(SDF_DIR)/$(FUSION_RUN)
VCD_DIR        := $(VCD_DIR)/$(FUSION_RUN)
DO_CHIP         = log -r /*
DO_BATCH_CHIP   = run -all; quit
VCD_ADD         = 

ifeq ($(FUSION_RUN), HANDOFF)
override SDF = OFF
endif

ifeq ($(NETLISTS), OFF)
override SDF = OFF
endif

ifeq ($(SDF), ON)
override ACC = ON
endif

ifeq ($(VCD), ON)
override ACC = ON
DO_BATCH_CHIP  =
ifneq ($(VCD_START), 0)
DO_BATCH_CHIP += set VCD_START $(VCD_START);
endif
ifneq ($(VCD_DURATION), -1)
DO_BATCH_CHIP += set VCD_DURATION $(VCD_DURATION);
endif
DO_BATCH_CHIP += source $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl; source $(VSIM_DIR_CHIP)/vcd_dump.tcl;
DO_CHIP       := log -r /*; $(DO_BATCH_CHIP)
DO_BATCH_CHIP += quit;
endif

ifneq ($(NETLISTS), OFF)
	override N_NET_CLUSTER := $(shell echo $$(($(N_NET_CLUSTER) - 1)))
	override N_NET_MEM     := $(shell echo $$(($(N_NET_MEM) - 1)))
	N_NET_CLUSTER_SEQ       = $(shell seq 0 $(N_NET_CLUSTER))
	N_NET_MEM_SEQ           = $(shell seq 0 $(N_NET_MEM))

	NETLISTS_LENGTH     = $(words $(NETLISTS))
	NETLISTS_LENGTH_SEQ = $(shell seq 1 $(NETLISTS_LENGTH))
	PICOBELLO_NET_INDEX = $(firstword  $(foreach n, $(NETLISTS_LENGTH_SEQ),$(if $(filter picobello_chip,$(word $(n),$(NETLISTS))),$(n),)))
	CLUSTER_NET_INDEX   = $(firstword  $(foreach n, $(NETLISTS_LENGTH_SEQ),$(if $(filter cluster_tile,$(word $(n),$(NETLISTS))),$(n),)))
	CHESHIRE_NET_INDEX  = $(firstword  $(foreach n, $(NETLISTS_LENGTH_SEQ),$(if $(filter cheshire_tile,$(word $(n),$(NETLISTS))),$(n),)))
	MEM_NET_INDEX       = $(firstword  $(foreach n, $(NETLISTS_LENGTH_SEQ),$(if $(filter mem_tile,$(word $(n),$(NETLISTS))),$(n),)))
	FHG_SPU_NET_INDEX   = $(firstword  $(foreach n, $(NETLISTS_LENGTH_SEQ),$(if $(filter fhg_spu_tile,$(word $(n),$(NETLISTS))),$(n),)))

	ifneq ($(PICOBELLO_NET_INDEX),)
	PICOBELLO_INSTANCE = tb_picobello_chip.fix.dut
	CLUSTER_INSTANCE   = tb_picobello_chip.fix.dut.i_picobello_top_gen_clusters_*__i_cluster_tile
	CHESHIRE_INSTANCE  = tb_picobello_chip.fix.dut.i_picobello_top_i_cheshire_tile
	MEM_INSTANCE       = tb_picobello_chip.fix.dut.i_picobello_top_gen_memtile_*__i_mem_tile
	FHG_SPU_INSTANCE   = tb_picobello_chip.fix.dut.i_picobello_top_i_fhg_spu_tile
	else
	CLUSTER_INSTANCE   = tb_picobello_chip.fix.dut.i_picobello_top.gen_clusters[*].i_cluster_tile
	CHESHIRE_INSTANCE  = tb_picobello_chip.fix.dut.i_picobello_top.i_cheshire_tile
	MEM_INSTANCE       = tb_picobello_chip.fix.dut.i_picobello_top.gen_memtile[*].i_mem_tile
	FHG_SPU_INSTANCE   = tb_picobello_chip.fix.dut.i_picobello_top.i_fhg_spu_tile
	endif

	NET_DEFINES =
	NET_FILES   =
	REQ_FILES   =
	ifneq ($(PICOBELLO_NET_INDEX),)
		VCD_ADD     += vcd add -r /$(PICOBELLO_INSTANCE)/*;
		NET_DEFINES += +define+PICOBELLO_CHIP_NET=$(PICOBELLO_INSTANCE)
		ifneq ($(subst .,,$(word $(PICOBELLO_NET_INDEX),$(NETLISTS_FILES))),)
		NET_FILES   += $(word $(PICOBELLO_NET_INDEX),$(NETLISTS_FILES))
		else
		NET_FILES   += $(NETLISTS_DIR)/picobello_chip.v
		REQ_FILES   += $(NETLISTS_DIR)/picobello_chip.v
		endif
	endif
	ifneq ($(CLUSTER_NET_INDEX),)
		VCD_ADD     += $(foreach n, $(N_NET_CLUSTER_SEQ),vcd add -r /$(subst *,$(n),$(CLUSTER_INSTANCE))/*;)
		NET_DEFINES += $(foreach n, $(N_NET_CLUSTER_SEQ),+define+CLUSTER_TILE_NET_$(n)=$(subst *,$(n),$(CLUSTER_INSTANCE)))
		ifneq ($(subst .,,$(word $(CLUSTER_NET_INDEX),$(NETLISTS_FILES))),)
		NET_FILES   += $(word $(CLUSTER_NET_INDEX),$(NETLISTS_FILES))
		else
		NET_FILES   += $(NETLISTS_DIR)/cluster_tile.v
		REQ_FILES   += $(NETLISTS_DIR)/cluster_tile.v
		endif
	endif
	ifneq ($(CHESHIRE_NET_INDEX),)
		VCD_ADD     += vcd add -r /$(CHESHIRE_INSTANCE)/*;
		NET_DEFINES += +define+CHESHIRE_TILE_NET=$(CHESHIRE_INSTANCE)
		ifneq ($(subst .,,$(word $(CHESHIRE_NET_INDEX),$(NETLISTS_FILES))),)
		NET_FILES   += $(word $(CHESHIRE_NET_INDEX),$(NETLISTS_FILES))
		else
		NET_FILES   += $(NETLISTS_DIR)/cheshire_tile.v
		REQ_FILES   += $(NETLISTS_DIR)/cheshire_tile.v
		endif
	endif
	ifneq ($(MEM_NET_INDEX),)
		VCD_ADD     += $(foreach n, $(N_NET_MEM_SEQ),vcd add -r /$(subst *,$(n),$(MEM_INSTANCE))/*;)
		NET_DEFINES += $(foreach n, $(N_NET_MEM_SEQ),+define+MEM_TILE_NET_$(n)=$(subst *,$(n),$(MEM_INSTANCE)))
		ifneq ($(subst .,,$(word $(MEM_NET_INDEX),$(NETLISTS_FILES))),)
		NET_FILES   += $(word $(MEM_NET_INDEX),$(NETLISTS_FILES))
		else
		NET_FILES   += $(NETLISTS_DIR)/mem_tile.v
		REQ_FILES   += $(NETLISTS_DIR)/mem_tile.v
		endif
	endif
	ifneq ($(FHG_SPU_NET_INDEX),)
		VCD_ADD     += vcd add -r /$(FHG_SPU_INSTANCE)/*;
		NET_DEFINES += +define+FHG_SPU_TILE_NET=$(FHG_SPU_INSTANCE)
		ifneq ($(subst .,,$(word $(FHG_SPU_NET_INDEX),$(NETLISTS_FILES))),)
		NET_FILES   += $(word $(FHG_SPU_NET_INDEX),$(NETLISTS_FILES))
		else
		NET_FILES   += $(NETLISTS_DIR)/fhg_spu_tile.v
		REQ_FILES   += $(NETLISTS_DIR)/fhg_spu_tile.v
		endif
	endif

	ifeq ($(SDF), ON)
		SDF_DEFINES = 
		ifneq ($(PICOBELLO_NET_INDEX),)
			ifneq ($(subst .,,$(word $(PICOBELLO_NET_INDEX),$(SDF_FILES))),)
			SDF_DEFINES += -sdfmin /$(subst .,/,$(PICOBELLO_INSTANCE))=$(word $(PICOBELLO_NET_INDEX),$(SDF_FILES))
			else
			SDF_DEFINES += -sdfmin /$(subst .,/,$(PICOBELLO_INSTANCE))=$(SDF_DIR)/picobello_chip.sdf
			REQ_FILES   += $(SDF_DIR)/picobello_chip.sdf
			endif
		endif
		ifneq ($(CLUSTER_NET_INDEX),)
			ifneq ($(subst .,,$(word $(CLUSTER_NET_INDEX),$(SDF_FILES))),)
			SDF_DEFINES += $(foreach n, $(N_NET_CLUSTER_SEQ), -sdfmin /$(subst .,/,$(subst *,$(n),$(CLUSTER_INSTANCE)))=$(word $(CLUSTER_NET_INDEX),$(SDF_FILES)))
			else
			SDF_DEFINES += $(foreach n, $(N_NET_CLUSTER_SEQ), -sdfmin /$(subst .,/,$(subst *,$(n),$(CLUSTER_INSTANCE)))=$(SDF_DIR)/cluster_tile.sdf)
			REQ_FILES   += $(SDF_DIR)/cluster_tile.sdf
			endif
		endif
		ifneq ($(CHESHIRE_NET_INDEX),)
			ifneq ($(subst .,,$(word $(CHESHIRE_NET_INDEX),$(SDF_FILES))),)
			SDF_DEFINES += -sdfmin /$(subst .,/,$(CHESHIRE_INSTANCE))=$(word $(CHESHIRE_NET_INDEX),$(SDF_FILES))
			else
			SDF_DEFINES += -sdfmin /$(subst .,/,$(CHESHIRE_INSTANCE))=$(SDF_DIR)/cheshire_tile.sdf
			REQ_FILES   += $(SDF_DIR)/cheshire_tile.sdf
			endif
		endif
		ifneq ($(MEM_NET_INDEX),)
			ifneq ($(subst .,,$(word $(MEM_NET_INDEX),$(SDF_FILES))),)
			SDF_DEFINES += $(foreach n, $(N_NET_MEM_SEQ), -sdfmin /$(subst .,/,$(subst *,$(n),$(MEM_INSTANCE)))=$(word $(MEM_NET_INDEX),$(SDF_FILES)))
			else
			SDF_DEFINES += $(foreach n, $(N_NET_MEM_SEQ), -sdfmin /$(subst .,/,$(subst *,$(n),$(MEM_INSTANCE)))=$(SDF_DIR)/mem_tile.sdf)
			REQ_FILES   += $(SDF_DIR)/mem_tile.sdf
			endif
		endif
		ifneq ($(FHG_SPU_NET_INDEX),)
			ifneq ($(subst .,,$(word $(FHG_SPU_NET_INDEX),$(SDF_FILES))),)
			SDF_DEFINES += -sdfmin /$(subst .,/,$(FHG_SPU_INSTANCE))=$(word $(FHG_SPU_NET_INDEX),$(SDF_FILES))
			else
			SDF_DEFINES += -sdfmin /$(subst .,/,$(FHG_SPU_INSTANCE))=$(SDF_DIR)/fhg_spu_tile.sdf
			REQ_FILES   += $(SDF_DIR)/fhg_spu_tile.sdf
			endif
		endif
	endif

endif

VCD_ADD := $(subst .,/,$(VCD_ADD))

# vlog arguments

VLOG_ARGS_CHIP  = -suppress vlog-2583
VLOG_ARGS_CHIP += -suppress vlog-13314
VLOG_ARGS_CHIP += -suppress vlog-13233
VLOG_ARGS_CHIP += -timescale 1ns/1ps
VLOG_ARGS_CHIP += +define+TSMC_INITIALIZE_MEM_USING_DEFAULT_TASKS
VLOG_ARGS_CHIP += +define+TSMC_MEM_LOAD_RANDOM

VLOG_ARGS_CHIP_NETS := $(VLOG_ARGS_CHIP)
VLOG_ARGS_CHIP_NETS += -work $(VSIM_DIR_CHIP)/nets_lib
ifneq ($(SDF), ON)
VLOG_ARGS_CHIP_NETS += +nospecify
endif

VLOG_ARGS_CHIP += +nospecify
VLOG_ARGS_CHIP += -work $(VSIM_WORK_CHIP)
ifneq ($(NETLISTS), OFF)
VLOG_ARGS_CHIP += -Ldir $(VSIM_DIR_CHIP) -L nets_lib
VLOG_ARGS_CHIP += +define+NETS_LIB
VLOG_ARGS_CHIP += $(NET_DEFINES)
endif

# vcom arguments

VCOM_ARGS_CHIP      = -work $(VSIM_WORK_CHIP)
VCOM_ARGS_CHIP_NETS = -work $(VSIM_DIR_CHIP)/nets_lib

# vopt arguments

VOPT_ARGS_CHIP  = -work $(VSIM_WORK_CHIP)
ifneq ($(NETLISTS), OFF)
VOPT_ARGS_CHIP += -Ldir $(VSIM_DIR_CHIP) -L nets_lib
endif
ifeq ($(ACC), ON)
VOPT_ARGS_CHIP += +acc
endif
ifeq ($(SDF), ON)
VOPT_ARGS_CHIP += -sdf
VOPT_ARGS_CHIP += $(SDF_DEFINES)
else
VOPT_ARGS_CHIP += +nospecify
endif

# vsim arguments

VSIM_FLAGS_CHIP = -work $(VSIM_WORK_CHIP)
VSIM_FLAGS_CHIP += -suppress 3009
VSIM_FLAGS_CHIP += -suppress 8386
VSIM_FLAGS_CHIP += -suppress 13314
VSIM_FLAGS_CHIP += -suppress 12091
VSIM_FLAGS_CHIP += -suppress 3438
VSIM_FLAGS_CHIP += -suppress 16107
VSIM_FLAGS_CHIP += -suppress 3448
VSIM_FLAGS_CHIP += -quiet
VSIM_FLAGS_CHIP += -64
VSIM_FLAGS_CHIP += +nontcglitch
ifneq ($(NETLISTS), OFF)
VSIM_FLAGS_CHIP += -Ldir $(VSIM_DIR_CHIP) -L nets_lib
endif

define add_vsim_flag
ifdef $(1)
	VSIM_FLAGS_CHIP += +$(1)=$$($(1))
endif
endef

$(eval $(call add_vsim_flag,CHS_BINARY))
$(eval $(call add_vsim_flag,SN_BINARY))
$(eval $(call add_vsim_flag,BOOTMODE))
$(eval $(call add_vsim_flag,PRELMODE))
$(eval $(call add_vsim_flag,USE_FLL))

.PHONY: vsim-compile-chip vsim-compile-nets vsim-clean-chip vsim-clean-nets vsim-run-chip vsim-run-batch-chip

test:
	@echo "$(VCD_ADD)"

vsim-clean-chip:
	rm -rf $(VSIM_WORK_CHIP)
	rm -f $(VSIM_DIR_CHIP)/transcript
	rm -f $(VSIM_DIR_CHIP)/compile.tcl
	rm -f $(VSIM_DIR_CHIP)/compile_nets.tcl
	rm -f $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl
	rm -rf $(VSIM_DIR_CHIP)/nets_lib

vsim-clean-nets:
	rm -rf $(PB_PD_DIR)/tsmc7/src/netlist
	rm -rf $(PB_PD_DIR)/tsmc7/sdf
	rm -rf $(PB_PD_DIR)/tsmc7/vcd

ifneq ($(NETLISTS), OFF)
vsim-compile-chip: $(REQ_FILES) $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl vsim-compile-nets
endif

$(NETLISTS_DIR)/%.v: $(NETLISTS_DIR)/%.v.gz
	gunzip -k -f $<

$(SDF_DIR)/%.sdf: $(SDF_DIR)/%.sdf.gz
	gunzip -k -f $<

ifeq ($(FUSION_RUN), DELIVERY)

$(NETLISTS_DIR)/%.v.gz:
	mkdir -p $(NETLISTS_DIR)/
	ln -s $(DELIVERY_DIR)/$*/$(shell ls $(DELIVERY_DIR)/$* | grep '^v[0-9]\+$$' | cut -c2- | sort -n | tail -n1 | sed 's|^|v|')/$*.v.gz $(NETLISTS_DIR)/$*.v.gz

$(SDF_DIR)/%.sdf.gz:
	mkdir -p $(SDF_DIR)/
	ln -s $(DELIVERY_DIR)/$*/$(shell ls $(DELIVERY_DIR)/$* | grep '^v[0-9]\+$$' | cut -c2- | sort -n | tail -n1 | sed 's|^|v|')/$*.sdf.gz $(SDF_DIR)/$*.sdf.gz

else ifeq ($(FUSION_RUN), HANDOFF)

$(NETLISTS_DIR)/%.v.gz:
	mkdir -p $(NETLISTS_DIR)/
	ln -s $(HANDOFF_DIR)/$*/netlist/$*.v.gz $(NETLISTS_DIR)/$*.v.gz

else

$(NETLISTS_DIR)/%.v.gz:
	mkdir -p $(NETLISTS_DIR)/
	ln -s $(FUSION_OUT_DIR)/$*_$(FUSION_RUN)/$*.v.gz $(NETLISTS_DIR)/$*.v.gz

$(SDF_DIR)/%.sdf.gz:
	mkdir -p $(SDF_DIR)/
	ln -s $(FUSION_OUT_DIR)/$*_$(FUSION_RUN)/$*.sdf.gz $(SDF_DIR)/$*.sdf.gz

endif

vsim-compile-nets: $(VSIM_DIR_CHIP)/compile_nets.tcl
	cd $(VSIM_DIR_CHIP); $(VLIB) nets_lib
	$(VSIM) -c -quiet -64 -do "source $(VSIM_DIR_CHIP)/compile_nets.tcl; quit"

$(VSIM_DIR_CHIP)/compile_nets.tcl: $(PB_PD_DIR)/Bender.yml
	bender -d $(PB_PD_DIR) script vsim --compilation-mode common -t asic -t tsmc7 --vlog-arg="$(VLOG_ARGS_CHIP_NETS)" --vcom-arg="$(VCOM_ARGS_CHIP_NETS)" > $@
	echo 'if {[catch { vlog -incr -sv $(VLOG_ARGS_CHIP_NETS) $(NET_FILES) }]} {return 1}' >> $@

vsim-compile-chip: $(VSIM_DIR_CHIP)/compile.tcl $(PB_HW_ALL)
	$(VSIM) -c $(VSIM_FLAGS_CHIP) -do "source $<; quit"
	$(VOPT) $(VOPT_ARGS_CHIP) -o $(CONFIG_CHIP)_opt $(CONFIG_CHIP)

$(VSIM_DIR_CHIP)/compile.tcl: $(PB_ROOT)/Bender.yml
	bender script vsim --compilation-mode common $(COMMON_TARGS) $(SIM_TARGS) -t asic -t tsmc7 --vlog-arg="$(VLOG_ARGS_CHIP)" --vcom-arg="$(VCOM_ARGS_CHIP)" > $@
	echo 'vlog -work $(VSIM_WORK_CHIP) "$(realpath $(CHS_ROOT))/target/sim/src/elfloader.cpp" -ccflags "-std=c++11"' >> $@

$(VSIM_DIR_CHIP)/vcd_dump_nets.tcl:
	mkdir -p $(VCD_DIR)
	echo "set VCD_FILE $(VCD_DIR)/dump.vcd" > $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl
	echo "set VCD_NETS {$(NETLISTS)}" >> $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl
	echo "proc add_vcd {} {" >> $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl
	echo "$(VCD_ADD)" >> $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl
	echo "}" >> $(VSIM_DIR_CHIP)/vcd_dump_nets.tcl

vsim-run-chip:
	$(VSIM) $(VSIM_FLAGS_CHIP) $(CONFIG_CHIP)_opt -do "$(DO_CHIP)"

vsim-run-batch-chip:
	$(VSIM) -c $(VSIM_FLAGS_CHIP) $(CONFIG_CHIP)_opt -do "$(DO_BATCH_CHIP)"

vsim-run-batch-verify-chip: vsim-run-batch-chip
ifdef VERIFY_PY
	$(VERIFY_PY) placeholder $(SN_BINARY) --no-ipc --memdump l2mem.bin --memaddr 0x70000000
endif
