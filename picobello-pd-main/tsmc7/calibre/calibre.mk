# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

CALIBRE_DIR = $(PB_PD_DIR)/tsmc7/calibre

MGC_CALIBRE_DRC_RUNSET_FILE = $(CALIBRE_DIR)/runset.drc
MGC_CALIBRE_DRC_RULES_FILE = /usr/pack/tsmc-7-kgf/dz/calibre/1P15M_1X_h_1Xa_v_1Ya_h_5Y_vhvhv_2Yy2Yx2R_UT-AlRDL_RF__CRN7_v1.0_2p1a/CLN7FF_15M_1X1Xa1Ya5Y2Yy2Yx2R_001.14_1a.encrypt

######################
## Realtime Calibre ##
######################

MGC_CALIBRE_ROOT = /usr/pack/$(word 1, $(MGC_CALIBRE))
ICC_CALIBRE_PATH = $(shell find $(MGC_CALIBRE_ROOT) -type f -path '*/lib/icc_calibre.tcl' 2>/dev/null | head -n 1)
ICC_LAYER_MAP_FILE = $(shell find -L $(PB_PD_DIR)/tsmc7/technology/ndm -type f -name '*.map' 2>/dev/null | head -n 1)

ifeq ($(RT_CALIBRE), ON)
# The following environment variables are needed
# for the realtime Calibre setup before starting Fusion Compiler:
# Enable RT calibre
RT_CALIBRE_ENV = MGC_REALTIME_ENABLE_ICC2=1
# Set the layer map file
RT_CALIBRE_ENV += MGC_REALTIME_LAYER_MAP_FILE=$(ICC_LAYER_MAP_FILE)
# Set the runset file for DRC
RT_CALIBRE_ENV += MGC_REALTIME_RUNSET_FILE=$(MGC_CALIBRE_DRC_RUNSET_FILE)
# Set the calibre sepp environment
RT_CALIBRE_ENV += $(word 1, $(MGC_CALIBRE))
# Realtime Calibre is only possible if the GUI is already open
FC_GUI = -gui
# Source the `icc_calibre.tcl` script to invoke realtime Calibre
FC_PARAMS += source $(ICC_CALIBRE_PATH);
endif

#################
## Calibre DRC ##
#################

.PHONY: drc-% drc

# This target checks whether `RUN_NAME` is set, otherwise it defaults
# to the latest run, called `$(DESIGN)_latest`. It also resolves the
# absolute path of the run directory, to avoid multiple `$(DESIGN)_latest`
# directories.
drc-%:
	$(eval DESIGN := $(subst -,_,$*))
	$(eval _RUN_NAME := $(shell readlink -f $(PB_PD_DIR)/tsmc7/fusion/out/$(or $(RUN_NAME),$(DESIGN)_latest) | xargs basename))
	$(MAKE) drc DESIGN=$(DESIGN) RUN_NAME=$(_RUN_NAME)

# This target sets the environment variables that are required for the DRC runset.
# It also launches the Calibre DRC run with two runsets:
# - `runset.drc`: The main runset for DRC checks
# - `runset.env.drc`: Overrides of the inputs and outputs taken from the environment variables
drc: export LAYOUT_PATH = $(PB_PD_DIR)/tsmc7/fusion/out/$(RUN_NAME)/$(DESIGN).gds.gz
drc: export LAYOUT_PRIMARY = $(DESIGN)
drc: export DRC_WORKDIR = $(CALIBRE_DIR)/drc/out/$(RUN_NAME)
drc:
	@echo "Running Calibre DRC for layout '$(LAYOUT_PATH)' and run name '$(RUN_NAME)'..."
	$(MGC_CALIBRE) -gui -drc -batch -runset "$(CALIBRE_DIR)/runset.drc $(CALIBRE_DIR)/runset.env.drc"


#################
## Calibre LVS ##
#################

.PHONY: lvs-% lvs

# This target checks whether `RUN_NAME` is set, otherwise it defaults
# to the latest run, called `$(DESIGN)_latest`. It also resolves the
# absolute path of the run directory, to avoid multiple `$(DESIGN)_latest`
# directories.
lvs-%:
	$(eval DESIGN := $(subst -,_,$*))
	$(eval _RUN_NAME := $(shell readlink -f $(PB_PD_DIR)/tsmc7/fusion/out/$(or $(RUN_NAME),$(DESIGN)_latest) | xargs basename))
	$(MAKE) lvs DESIGN=$(DESIGN) RUN_NAME=$(_RUN_NAME)

# This target just sets all the environment variables
# required by the runset
lvs: export LAYOUT_PATH = $(PB_PD_DIR)/tsmc7/fusion/out/$(RUN_NAME)/$(DESIGN).gds.gz
lvs: export LAYOUT_PRIMARY = $(DESIGN)
lvs: export SPICE_INPUT = $(CALIBRE_DIR)/lvs/out/$(RUN_NAME)/$(DESIGN).lvs.spi
lvs: export RESULTS_FILE_NAME = $(DESIGN).lvs
lvs: export LVS_WORKDIR = $(CALIBRE_DIR)/lvs/out/$(RUN_NAME)
lvs: $(CALIBRE_DIR)/lvs/out/$(RUN_NAME)/$(DESIGN).lvs.spi
	@echo "Running Calibre LVS for layout '$(LAYOUT_PATH)' and run name '$(RUN_NAME)'..."
	@mkdir -p $(LVS_WORKDIR)
	ln -sfn $(CALIBRE_DIR)/lvs/spice.inc $(LVS_WORKDIR)/spice.inc
	$(MGC_CALIBRE) -gui -lvs -batch -runset "$(CALIBRE_DIR)/runset.lvs $(CALIBRE_DIR)/runset.env.lvs"

# Rule to generate SPICE netlist from Verilog
$(CALIBRE_DIR)/lvs/out/$(RUN_NAME)/$(DESIGN).lvs.spi: $(PB_PD_DIR)/tsmc7/fusion/out/$(RUN_NAME)/$(DESIGN).lvs.v.gz
	@echo "Converting Verilog netlist to SPICE for $(DESIGN) run $(RUN_NAME)..."
	@mkdir -p $(dir $@)
	cd $(CALIBRE_DIR)/lvs && ./verilog2spice $(abspath $<) $(abspath $@)
