# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
PB_ROOT ?= /srv/home/maoyuan.cai/spatz
# PB_PD_DIR ?= $(PB_ROOT)/pd
PB_PD_DIR ?= $(PB_ROOT)/picobello-pd-main
FC_DIR ?= $(PB_PD_DIR)/tsmc7/fusion

###################
## Tool Versions ##
###################

# # Fusion Compiler
# SNPS_FC = fs_compiler-2024.09 fc_shell
# # Libary Compiler
# SNPS_LC = synopsys-2024.09 lc_shell
# # Calibre
# MGC_CALIBRE = calibre-2024.4-dz calibre
# # IC-Validator
# SNPS_ICV = ic_validator-2025.06-dz icv64

# Fusion Compiler
SNPS_FC = fc_shell
# Libary Compiler
SNPS_LC = lc_shell

#######################
## Fusion parameters ##
#######################

define add_run_flags
ifdef $(2)
	$(1) += set $(2) $($(2));
endif
endef

$(eval $(call add_run_flags,FC_PARAMS,DESIGN))
$(eval $(call add_run_flags,FC_PARAMS,RUN_DESC))
$(eval $(call add_run_flags,FC_PARAMS,RUN_NAME))
$(eval $(call add_run_flags,FC_PARAMS,HANDOFF))
$(eval $(call add_run_flags,FC_PARAMS,FROM_STAGE))
$(eval $(call add_run_flags,FC_PARAMS,FROM_LABEL))
$(eval $(call add_run_flags,FC_PARAMS,SKIP_STAGES))
$(eval $(call add_run_flags,FC_PARAMS,PAUSE_AFTER_STAGE))
$(eval $(call add_run_flags,FC_PARAMS,TO_STAGE))
$(eval $(call add_run_flags,FC_PARAMS,ECO_OPT))
$(eval $(call add_run_flags,FC_PARAMS,MODE))
$(eval $(call add_run_flags,FC_PARAMS,NO_UNGROUP))

#############
## Calibre ##
#############

-include $(PB_PD_DIR)/tsmc7/calibre/calibre.mk

##################
## IC Validator ##
##################

SNPS_ICV_SEPP = $(word 1, $(SNPS_ICV))

#############
## Cockpit ##
#############

# .PHONY: cockpit

# cockpit:
# 	cd $(PB_PD_DIR)/tsmc7; icdesign tsmc7 -update all -nogui
.PHONY: cockpit
cd $(PB_PD_DIR)/tsmc7; ln -s /sw/TECH/TSMC/7/technology technology

###################
## Tool starters ##
###################

.PHONY: start-fusion start-fusion-% start-lc clean-fusion start-calibre

start-fusion:
	# cd $(FC_DIR); $(RT_CALIBRE_ENV) $(SNPS_ICV_SEPP) $(SNPS_FC) $(FC_GUI) -x "$(FC_PARAMS); source scripts/utils.tcl"
	cd $(FC_DIR); $(SNPS_FC) $(FC_GUI) -x "$(FC_PARAMS); source scripts/utils.tcl"

start-lc:
	cd $(FC_DIR); $(SNPS_LC)

# start-calibre:
# 	cd $(PB_PD_DIR)/tsmc7/calibre; ./start_calibre

# Removes unimportant Fusion Compiler files
clean-fusion:
	rm -f $(PB_PD_DIR)/**/fc_output.txt
	rm -rf $(PB_PD_DIR)/**/HDL_LIBRARIES
	rm -rf $(PB_PD_DIR)/**/checkpoint_*
	rm -rf $(PB_PD_DIR)/**/tmp


#####################
## Design Planning ##
#####################

.PHONY: design-planning

design-planning:
	cd $(FC_DIR)/design_planning; $(SNPS_FC) \
	-x "$(FC_PARAMS); set DESIGN picobello_chip; \
	source $(FC_DIR)/design_planning/dp.tcl"

#################
## Elaboration ##
#################

.PHONY: elab elab-%

elab-%:
	$(MAKE) elab DESIGN=$(subst -,_,$*)

elab:
	cd $(FC_DIR)/design_planning; $(SNPS_FC) \
	-x "$(FC_PARAMS); set RUN_NAME elab_$(DESIGN); set TO_STAGE elaboration; \
	source $(FC_DIR)/design_planning/dp.tcl"

###################
## Place & Route ##
###################

.PHONY: pnr pnr-% export-% delivery-%

pnr-%:
	$(MAKE) pnr DESIGN=$(subst -,_,$*)

export-%:
	$(MAKE) pnr DESIGN=$(subst -,_,$*) FROM_STAGE=export TO_STAGE=export

delivery-%:
	$(MAKE) pnr DESIGN=$(subst -,_,$*) FROM_STAGE=export TO_STAGE=delivery

pnr:
	cd $(FC_DIR)/block_pnr; $(SNPS_FC) \
	-x "$(FC_PARAMS); source $(FC_DIR)/block_pnr/pnr.tcl"

pnr-picobello-chip:
	cd $(FC_DIR)/top_pnr; $(SNPS_FC) \
	-x "$(FC_PARAMS); set DESIGN picobello_chip; \
	source $(FC_DIR)/top_pnr/pnr.tcl"

##############
# Simulation #
##############

TB_DUT_CHIP = tb_picobello_chip

include $(PB_PD_DIR)/target/sim/vsim/vsim-chip.mk

####################
##  Miscellanous  ##
####################

.PHONY: gen-chip-regs clean-chip-regs

gen-chip-regs: $(PB_GEN_DIR)/pb_chip_regs.sv
$(PB_GEN_DIR)/pb_chip_regs.sv: $(PB_GEN_DIR)/pb_chip_regs_pkg.sv
$(PB_GEN_DIR)/pb_chip_regs_pkg.sv: $(PB_PD_DIR)/cfg/pb_chip_regs.rdl
	$(PEAKRDL) regblock $< -o $(dir $@) --cpuif apb4-flat --default-reset arst_n

clean-chip-regs:
	rm -f $(PB_GEN_DIR)/pb_chip_regs.sv
	rm -f $(PB_GEN_DIR)/pb_chip_regs_pkg.sv

PB_RDL_ALL += $(PB_PD_DIR)/cfg/pb_chip_regs.rdl
PB_RDL_ALL += $(PB_PD_DIR)/cfg/fll.rdl

PB_RDL_HW_ALL += $(PB_GEN_DIR)/pb_chip_regs_pkg.sv
PB_RDL_HW_ALL += $(PB_GEN_DIR)/pb_chip_regs.sv

PEAKRDL_DEFINES += -D PB_CHIP_RDL
PEAKRDL_INCLUDES := -I$(PB_PD_DIR)/cfg $(PEAKRDL_INCLUDES)

.PHONY: dvt-flist-pd

dvt-flist-pd:
	$(BENDER) script flist-plus -d $(PB_ROOT) $(COMMON_TARGS) $(SIM_TARGS) -t asic -t tsmc7 > .dvt/default.build

##############################
##  ArtistIC GDS rendering  ##
##############################

ARTISTIC_OUTDIR ?= $(PB_PD_DIR)/util/out
ARTISTIC_CFG ?= $(PB_PD_DIR)/util/artistic_cfg.json.in
GDS_DIR ?= /usr/scratch/rosinli/picobello_handoff/final_runs
PX_WIDTH ?= 4000

$(ARTISTIC_OUTDIR)/$(DESIGN).json: $(ARTISTIC_CFG)
	mkdir -p $(ARTISTIC_OUTDIR)
	oseda klayout -b -r $(PD_DIR)/util/gen_artistic_cfg.py \
	-rd infile=$(GDS) \
	-rd template=$(ARTISTIC_CFG) \
	-rd out=$(ARTISTIC_OUTDIR)/$(DESIGN).json \
	-rd px_width=$(PX_WIDTH)

render-%:
	@mod_name=$(subst -,_,$*) && $(MAKE) render DESIGN=$$mod_name GDS=$(GDS_DIR)/$$mod_name/$$mod_name.gds.gz

render: $(ARTISTIC_OUTDIR)/$(DESIGN).json
	cd $(PD_DIR)/util/artistic && oseda make CFG_FILE=$< gen_raw
	cd $(PD_DIR)/util/artistic && make CFG_FILE=$< gen_pdfs -j
