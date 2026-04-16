# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

source $FCDIR/scripts/fp_utils.tcl

set fll_cell [get_cells -of_references [get_lib_cells tsmc7_FLL]]
set wide_spm_srams [get_cells -hierarchical -filter {design_type==macro && full_name=~*wide_spm_tile*}]
set narrow_spm_srams [get_cells -hierarchical -filter {design_type==macro && full_name=~*narrow_spm_tile*}]

set_attribute $fll_cell physical_status unplaced
set_attribute $wide_spm_srams physical_status unplaced
set_attribute $narrow_spm_srams physical_status unplaced

remove_edit_groups -all
remove_routing_blockages -all
if {[sizeof [get_keepout_margins -quiet *] ] > 0} {remove_keepout_margins *}

source $FCDIR/scripts/fp_utils.tcl

set_macro_relative_location \
  -target_object $fll_cell \
  -target_corner tl \
  -target_orientation R0 \
  -anchor_object [get_block_inst cheshire_tile] \
  -anchor_corner bl \
  -offset [list [expr 50 * $macro_hor_keepout_margin] [expr -50 * $macro_ver_keepout_margin]]

set vert_channel_front [expr 20 * $macro_hor_keepout_margin]
set vert_channel_back [expr $macro_hor_keepout_margin]

set wide_spm_array1 [create_macro_array \
  -num_rows 2 -num_cols 5  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_channel_back $vert_channel_front $vert_channel_back $vert_channel_front $vert_channel_front]]] \
  -orientation [concat {*}[lrepeat 2 {FN N FN N FN}]] \
  [index_collection $wide_spm_srams 0 9]]

set wide_spm_array2 [create_macro_array \
  -num_rows 2 -num_cols 3  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_channel_back $vert_channel_front $vert_channel_back $vert_channel_front $vert_channel_front]]] \
  -orientation [concat {*}[lrepeat 2 {FN N FN}]] \
  [index_collection $wide_spm_srams 10 15]]

set narrow_spm_array1 [create_macro_array \
  -num_rows 2 -num_cols 5  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_channel_back $vert_channel_front $vert_channel_back $vert_channel_front $vert_channel_front]]] \
  -orientation [concat {*}[lrepeat 2 {FN N FN N FN}]] \
  [index_collection $narrow_spm_srams 0 9]]

set narrow_spm_array2 [create_macro_array \
  -num_rows 2 -num_cols 3  \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_channel_back $vert_channel_front $vert_channel_back $vert_channel_front $vert_channel_front]]] \
  -orientation [concat {*}[lrepeat 2 {FN N FN}]] \
  [index_collection $narrow_spm_srams 10 15]]

set_macro_relative_location \
  -target_object $narrow_spm_array2 \
  -target_corner tr -target_orientation R0 \
  -anchor_object [get_block_inst cheshire_tile] \
  -anchor_corner br \
  -offset [list [expr -20*$macro_hor_keepout_margin] [expr -100*$macro_ver_keepout_margin]]

set_macro_relative_location \
  -target_object $narrow_spm_array1 \
  -target_corner tr -target_orientation R0 \
  -anchor_object $narrow_spm_array2 \
  -anchor_corner br \
  -offset [list 0 [expr -$macro_ver_keepout_margin]]

set_macro_relative_location \
  -target_object $wide_spm_array2 \
  -target_corner br -target_orientation R0 \
  -anchor_object [get_block_inst fhg_spu_tile] \
  -anchor_corner tr \
  -offset [list [expr -20*$macro_hor_keepout_margin] [expr 100*$macro_ver_keepout_margin]]

set_macro_relative_location \
  -target_object $wide_spm_array1 \
  -target_corner br -target_orientation R0 \
  -anchor_object $wide_spm_array2 \
  -anchor_corner tr \
  -offset [list 0 [expr $macro_ver_keepout_margin]]

# Create the actual placement
create_macro_relative_location_placement

# The FLL still has keepout margins on one side, which means its width
# is not a multiple of smallest boundary cell. We just remove them for now
if {[sizeof_collection [get_keepout_margins -quiet -of_objects $fll_cell]] > 0} {
  remove_keepout_margins [get_keepout_margins -of_objects $fll_cell]
}

if {[sizeof_collection [get_grids site_row_grid -quiet]] > 0} {
    remove_grids site_row_grid
}

# Because of the weird core offset, we need to adjust the grid offset
# for the macro placement slightly
# INFO(fischeti): This was a hacky solution to align the FLL tracks with the top-level tracks.
# However, it can result in further DRCs if the FLL is not aligned to a multiple of 2*site-width
# It would be better to not align the M3 tracks/pins and rather connect to the FLL from M4.
set x_step [expr 2 * [lcm [list [get_attribute [get_layers M3] pitch] $site_width]]]
set x_grid_offset 3.201
set y_grid_offset [expr (int($fp_y0 * 1000) % int($site_height * 1000))/1000.0]
create_grid -x_step $x_step -y_step $site_height -x_offset $x_grid_offset -y_offset $y_grid_offset site_row_grid
snap_cells_to_block_grid -grid site_row_grid -cells $fll_cell

# The north of the FLL requires at least 2um keepout margin, due to some DRCs
set fll_ver_keepout_margin [expr ceil(2.0 / $site_height) * $site_height]; # >= 2um and multiple of sitewidth
create_keepout_margin -type hard -outer [list $macro_hor_keepout_margin $fll_ver_keepout_margin $macro_hor_keepout_margin $fll_ver_keepout_margin] $fll_cell
# Create the routing blockages around all the edges of the blocks on M0 for PG routing
set keepout_hor [expr $macro_hor_keepout_margin + 7 * $site_width]
set keepout_ver [expr $fll_ver_keepout_margin + $site_height / 2]
create_keepout_margin -type routing_blockage -layer {M0 M1 M2 M3} -outer [list $keepout_hor $keepout_ver $keepout_hor $keepout_ver] $fll_cell

create_sram_keepouts $wide_spm_srams
create_sram_keepouts $narrow_spm_srams

set_attribute $fll_cell physical_status fixed
set_attribute $wide_spm_srams physical_status fixed
set_attribute $narrow_spm_srams physical_status fixed
