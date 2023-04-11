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

# Set lint_rtl goal and run
current_goal lint/lint_rtl
run_goal

# Create a link to the results
exec rm -rf sg_projects/${PROJECT}
exec ln -sf ${PROJECT}_${TIMESTAMP} sg_projects/${PROJECT}

# Ciao!
exit -save
