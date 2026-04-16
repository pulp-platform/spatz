# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

open_lib $block_libfilename
open_block $block_refname

# Enable Multibit
set ENABLE_MULTIBIT true

# Set the QoR strategy
set_qor_strategy -stage compile_initial -metric timing

# Don't use any floorplan information yet
set_app_options -name compile.auto_floorplan.initialize -value true

# Retime if tile requires it
source_optional $FCDIR/block_pnr/${block_refname_no_label}/retiming.tcl

# Compile to logic_opto
compile_fusion -to logic_opto

# Disable the auto-floorplanning again, otherwise it
# might reinitialize the floorplan again
set_app_options -name compile.auto_floorplan.initialize -value false

# Remove auto-floorplaning objects (terminals, tracks, etc.)
set auto_placed_terms [get_terminals -quiet]
if {[sizeof_collection $auto_placed_terms]} {
  remove_terminals $auto_placed_terms
}
remove_site_rows -all
remove_voltage_area_shapes -all
remove_tracks -all

# Save and close again
save_lib -all
close_block $block_refname
close_lib $block_libfilename
