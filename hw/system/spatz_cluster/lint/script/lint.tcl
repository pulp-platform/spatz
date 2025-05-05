# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

set PROJECT   spatz_cluster_wrapper
set TIMESTAMP [exec date +%Y%m%d_%H%M%S]

new_project sg_projects/${PROJECT}_${TIMESTAMP}
current_methodology $env(SPYGLASS_HOME)/GuideWare/latest/block/rtl_handoff

# Read the RTL
read_file -type sourcelist tmp/files

set_option enableSV09 yes
set_option allow_module_override yes
set_option designread_disable_flatten no
set_option nopreserve yes
set_option top spatz_cluster_wrapper

# Do not elaborate some problematic FPU modules
set_option stop fpnew_sdotp_multi
set_option stop fpnew_fma_multi
set_option stop tc_sram

# Read constraints
current_design spatz_cluster_wrapper
set_option sdc2sgdc yes
sdc_data -file sdc/func.sdc

# Link Design
compile_design

#
# Waivers
#

# Input [] declared but not read.
waive -rule "W240"
# Input [] not connected.
waive -rule "W287b"
# Variable [] set but not read.
waive -rule "W528"
# Signal [] is being assigned multiple times in the same block.
waive -rule "W415a"
# Bit-width mismatch between function call argument [] and function input [].
waive -rule "STARC05-2.1.3.1"
# Initial assignment for [] is ignored by synthesis.
waive -rule "SYNTH_89"
# Based number [] contains a dont care.
waive -rule "W467"
# Rhs width with shift is less than lhs width.
waive -rule "W486"
# For operator [] left expression width should match right expression width.
waive -rule "W116"
waive -rule "W362"
# Unsigned element [] passed to the $unsigned() function call.
waive -rule "WRN_1024"
# Enable pin EN on Flop [] (master RTL_FDCE) is always disabled (tied low)
waive -rule "FlopEConst"
# Return type width is less than return value width.
waive -rule "W416"

# Set lint_rtl goal and run
current_goal lint/lint_rtl
run_goal

# Create a link to the results
exec rm -rf sg_projects/${PROJECT}
exec ln -sf ${PROJECT}_${TIMESTAMP} sg_projects/${PROJECT}

# Ciao!
exit -save
