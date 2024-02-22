# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

$(VSIM_BUILDDIR):
	mkdir -p $@

$(VSIM_BUILDDIR)/compile.vsim.tcl: $(BENDER_LOCK) | $(VSIM_BUILDDIR)
	$(VLIB) $(dir $@)
	$(BENDER) script vsim $(VSIM_BENDER) --vlog-arg="$(VLOG_FLAGS) -work $(dir $@) " > $@
	echo '$(VLOG) -work $(dir $@) $(TB_CC_SOURCES) -vv -ccflags "$(TB_CC_FLAGS)"' >> $@
	echo 'return 0' >> $@

$(BIN_DIR):
	mkdir -p $@

# Build compilation script and compile all sources for Questasim simulation
$(BIN_DIR)/$(TARGET).vsim: $(VSIM_BUILDDIR)/compile.vsim.tcl $(VSIM_SOURCES) $(TB_SRCS) $(TB_CC_SOURCES) work/lib/libfesvr.a | $(BIN_DIR)
	$(VSIM) -c -do "source $<; quit" | tee $(dir $<)vlog.log
	@! grep -P "Errors: [1-9]*," $(dir $<)vlog.log
	$(VOPT) $(VOPT_FLAGS) -work $(VSIM_BUILDDIR) tb_bin -o tb_bin_opt | tee $(dir $<)vopt.log
	@! grep -P "Errors: [1-9]*," $(dir $<)vopt.log
	@echo "#!/bin/bash" > $@
	@echo 'binary=$$(realpath $$1)' >> $@
	@echo 'echo $$binary > .rtlbinary' >> $@
	@echo '$(VSIM) +permissive $(VSIM_FLAGS) $$3 -work $(MKFILE_DIR)/$(VSIM_BUILDDIR) -c \
				-ldflags "-Wl,-rpath,$(FESVR)/lib -L$(FESVR)/lib -lfesvr -lutil" \
				tb_bin_opt +permissive-off ++$$binary ++$$2' >> $@
	@chmod +x $@
	@echo "#!/bin/bash" > $@.gui
	@echo 'binary=$$(realpath $$1)' >> $@.gui
	@echo 'echo $$binary > .rtlbinary' >> $@.gui
	@echo '$(VSIM) +permissive $(VSIM_FLAGS) -work $(MKFILE_DIR)/$(VSIM_BUILDDIR) \
				-ldflags "-Wl,-rpath,$(FESVR)/lib -L$(FESVR)/lib -lfesvr -lutil" \
				tb_bin_opt +permissive-off ++$$binary ++$$2' >> $@.gui
	@chmod +x $@.gui

# Clean all build directories and temporary files for Questasim simulation
.PHONY: clean-vsim
clean-vsim: clean-work
	rm -rf $(BIN_DIR)/$(TARGET).vsim $(BIN_DIR)/$(TARGET).vsim.gui $(VSIM_BUILDDIR) vsim.wlf
