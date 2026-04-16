# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

open_lib $block_libfilename
open_block $block_refname

remove_track_patterns -all

create_track_pattern -layer M0 -direction horizontal \
    -type non_uniform -offsets {0.00 0.06 0.1 0.14 0.18 0.24} \
    -mask_pattern {mask_one mask_two mask_one mask_two mask_one mask_one}

# We redefine the floorplan to redo all the tracks
# Also, we account for the fact that we only have 2x width boundary cells
# which is why we set the `plan.flow.segment_rule` to `horizontal_even`
set_app_options -name plan.flow.segment_rule -value horizontal_even
initialize_floorplan -keep_boundary -keep_all
reset_placement

# Pre-color the tracks on layers M0 to M3
set_attribute [get_tracks -filter {layer_name == M2}] mask_pattern {mask_two mask_one}
set_attribute [get_tracks -filter {layer_name == M3}] mask_pattern {mask_one mask_two}

# Layer M1 is a special layer, where we need to define the cut metal locations with
# the `end_grip` settings

set m1_pitch [get_attribute [get_layers M1] pitch]

set s1 0.13
set s2 0.11
set lo_mask_one 0.03
set ho_mask_one 0.0
set lo_mask_two 0.11
set ho_mask_two 0.08

remove_tracks -layer M1 -dir X

create_track -layer M1 -dir X \
    -mask_pattern mask_one \
    -space [expr 2 * $m1_pitch] \
    -offset [expr 2 * $m1_pitch] -relative_to core_area \
    -end_grid_high_steps [list $s1 $s2] -end_grid_low_steps [list $s1 $s2] \
    -end_grid_low_offset $lo_mask_one -end_grid_high_offset $ho_mask_one

create_track -layer M1 -dir X \
    -mask_pattern mask_two \
    -space [expr 2 * $m1_pitch] \
    -offset [expr $m1_pitch] -relative_to core_area \
    -end_grid_high_steps [list $s1 $s2] -end_grid_low_steps [list $s1 $s2] \
    -end_grid_low_offset $lo_mask_two -end_grid_high_offset $ho_mask_two

# The tracks on the pin layer M5 to M8 should start from the die boundary (not core)
foreach layer {M5 M6 M7 M8} {
    remove_tracks -layer $layer
    create_track -layer $layer -relative_to block_boundary
}

# Set the current QoR strategy
set_qor_strategy -stage compile_initial -metric timing

# Set the placement style
# TODO(fischeti): Try out different placement styles
set_app_options -name plan.macro.style -value hybrid

# Do the actual placement
if {[file exists $FCDIR/block_pnr/$block_refname_no_label/place_macros.tcl]} {
    # Source the block specific macro placement script
    source $FCDIR/block_pnr/$block_refname_no_label/place_macros.tcl
    # Place the special cells
    source $FCDIR/scripts/special_cells.tcl
    # Run the placement without macro placement
    set_app_options -name place.coarse.fix_hard_macros -value true
} else {
    # Run the placement including the macro placement
    puts "No placement script found for $block_refname_no_label"
}

create_placement -floorplan

# Create frame and abstract
create_frame -block_all M11
create_abstract

# Save and close again
save_lib -all
close_lib
