# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# This scripts needs to be sourced after initializing the floorplan
# with `initialize_floorplan`

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
    -end_grid_relative_to core_area \
    -offset [expr 2 * $m1_pitch] -relative_to core_area \
    -end_grid_high_steps [list $s1 $s2] -end_grid_low_steps [list $s1 $s2] \
    -end_grid_low_offset $lo_mask_one -end_grid_high_offset $ho_mask_one

create_track -layer M1 -dir X \
    -mask_pattern mask_two \
    -space [expr 2 * $m1_pitch] \
    -end_grid_relative_to core_area \
    -offset [expr $m1_pitch] -relative_to core_area \
    -end_grid_high_steps [list $s1 $s2] -end_grid_low_steps [list $s1 $s2] \
    -end_grid_low_offset $lo_mask_two -end_grid_high_offset $ho_mask_two
