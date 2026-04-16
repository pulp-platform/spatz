# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status unplaced

remove_edit_groups -all
remove_routing_blockages -all
if {[sizeof [get_keepout_margins -quiet *] ] > 0} {remove_keepout_margins *}

source $FCDIR/scripts/fp_utils.tcl

set ICacheDataMacros [get_memory_macros "i_cluster/i_cluster/gen_hive*/i_snitch_icache/*i_lookup/*i_data/*"]
set ICacheDataHeight [get_attribute [lindex $ICacheDataMacros 0] height]
set ICacheDataWidth  [get_attribute [lindex $ICacheDataMacros 0] width]

set ICacheTagMacros  [get_memory_macros "i_cluster/i_cluster/gen_hive*/i_snitch_icache/*i_lookup/*i_tag/*"]
set ICacheTagHeight  [get_attribute [lindex $ICacheTagMacros 0] height]
set ICacheTagWidth   [get_attribute [lindex $ICacheTagMacros 0] width]

set TCDMMacros       [get_memory_macros "i_cluster/i_cluster/gen_tcdm_super_bank*/*"]
set TCDMNumMacros    [llength $TCDMMacros]
set TCDMHeight       [get_attribute [lindex $TCDMMacros 0] height]
set TCDMWidth        [get_attribute [lindex $TCDMMacros 0] width]

# Set the vertical channels such that 16 TCDM span the entire core area
# and round down the result to the nearest site width to align with the finFET grid
set vert_tcdm_channel [expr 200 * $site_width]

set tcdm_eg_bottom_left [create_macro_array \
  -num_rows 1 -num_cols 8  \
  -horizontal_channel_height [expr 4 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_tcdm_channel $macro_hor_keepout_margin]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  [lrange $TCDMMacros 0 [expr $TCDMNumMacros / 4 - 1]]]

set tcdm_eg_bottom_right [create_macro_array \
  -num_rows 1 -num_cols 8  \
  -horizontal_channel_height [expr 4 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_tcdm_channel $macro_hor_keepout_margin]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  [lrange $TCDMMacros [expr $TCDMNumMacros / 4] [expr $TCDMNumMacros / 2 - 1]]]

set tcdm_eg_top_left [create_macro_array \
  -num_rows 4 -num_cols 4  \
  -horizontal_channel_height [expr 4 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_tcdm_channel $macro_hor_keepout_margin]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  [lrange $TCDMMacros [expr $TCDMNumMacros / 2] [expr 3 * $TCDMNumMacros / 4 - 1]]]

set tcdm_eg_top_right [create_macro_array \
  -num_rows 4 -num_cols 4  \
  -horizontal_channel_height [expr 4 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_tcdm_channel $macro_hor_keepout_margin]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  [lrange $TCDMMacros [expr 3 * $TCDMNumMacros / 4] end]]

set icache_data_eg [create_macro_array \
  -num_rows 1 -num_cols 2  \
  -vertical_channel_width [expr 200 * $site_width] \
  -orientation FN \
  $ICacheDataMacros]

set icache_tag_eg [create_macro_array \
  -num_rows 1 -num_cols 2  \
  -vertical_channel_width [expr 200 * $site_width] \
  -orientation FN \
  $ICacheTagMacros]

# Set the relative locations of each macro array
set_macro_relative_location \
  -target_object $tcdm_eg_bottom_left \
  -target_corner bl -target_orientation R0 \
  -anchor_corner bl \
  -offset [list $macro_hor_keepout_margin $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $tcdm_eg_bottom_right \
  -target_corner br -target_orientation R0 \
  -anchor_corner br \
  -offset [list -$macro_hor_keepout_margin $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $tcdm_eg_top_left \
  -target_corner bl -target_orientation R0 \
  -anchor_object $tcdm_eg_bottom_left \
  -anchor_corner tl \
  -offset [list 0 $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $tcdm_eg_top_right \
  -target_corner br -target_orientation R0 \
  -anchor_object $tcdm_eg_bottom_right \
  -anchor_corner tr \
  -offset [list 0 $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $icache_data_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_corner tr \
  -offset [list -$macro_hor_keepout_margin -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $icache_tag_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_object $icache_data_eg \
  -anchor_corner br \
  -offset [list 0 [expr -20 * $site_height]];

# Create the actual placement
create_macro_relative_location_placement

# Add keepout margins around the srams
create_sram_keepouts [get_cells $ICacheDataMacros]
create_sram_keepouts [get_cells $ICacheTagMacros]
create_sram_keepouts [get_cells $TCDMMacros]

# This is necessary for the design planning flow, otherwise
# the macros will be replaced
set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status fixed
