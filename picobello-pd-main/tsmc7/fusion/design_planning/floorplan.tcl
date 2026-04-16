# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

remove_grids -all

# Source the block dimensions
source $DPDIR/block_dims.tcl

set channel_size [expr 2 * $grid_step_size]
set block_offset [expr 2 * $channel_size]

set fp_height [expr 2 * $block_offset + 4 * $height(mem_tile) + 3 * $channel_size]
set fp_width [expr 2 * $block_offset + 2 * $width(mem_tile) + 4 * $width(cluster_tile) + $width(cheshire_tile) + 6 * $channel_size]

# Use a multiple of the
# FinFet grid (0.03um), DRC CHB.W.1.1 need multiple of 0.12um
proc finfet_ceil {value} {
  return [expr ceil($value / 0.03) * 0.03]
}

# DRC CHB.EN.4.2: 0.9um + 0.12um * n, where n is an integer >= 0
proc chben_ceil {value} {
   set n [expr {ceil((2 * $value - 0.9) / 0.24)}]
   set offset [expr {(0.9 + 0.24 * $n) / 2.0}]
   # Round offset up to next multiple of 0.12
   set final [expr {ceil($offset / 0.12) * 0.12}]
   return $final
}

# The core offset needs to include the I/O drivers, the connections to the I/O drivers, and the
# I/O driver itself + the keepout margin of 5um. Furthermore, in the vertical core offset we use the
# height of the corner I/O cell, which is slighlty larger.
set io2cell_spacing 5
set pad2boundary_spacing 0.3

set core_offset_x [finfet_ceil [expr [get_attribute [get_lib_cells PFILLER000005] height] + $io2cell_spacing + $pad2boundary_spacing]]
set core_offset_y [finfet_ceil [expr [get_attribute [get_lib_cells PCORNER] height] + $io2cell_spacing + $pad2boundary_spacing]]

remove_track_patterns -all

create_track_pattern -layer M0 -direction horizontal \
    -type non_uniform -offsets {0.00 0.06 0.1 0.14 0.18 0.24} \
    -mask_pattern {mask_one mask_two mask_one mask_two mask_one mask_one}

# Keep it simple for now
initialize_floorplan \
  -control_type core \
  -side_length [list $fp_width $fp_height] \
  -core_offset [list $core_offset_x $core_offset_y]

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


# We want to align the tracks on M5 and M7 (all same pitch), where the
# block pins are placed. Further, we also align the site width and height
set grid_step_size [lcm [list \
    [get_attribute [get_layers M5] pitch] \
    [get_attribute [get_site_defs unit] height] \
    [get_attribute [get_site_defs unit] width]]]

# We start the grid at the core area
set block_grid_offsets [lindex [get_attribute [get_core_area] bbox] 0]

create_grid block_grid \
    -type block \
    -x_step $grid_step_size -y_step $grid_step_size \
    -x_offset [lindex $block_grid_offsets 0] \
    -y_offset [lindex $block_grid_offsets 1] \
    -orientations {R0}

set_block_grid_references -designs [get_designs -hierarchical] -grid block_grid
