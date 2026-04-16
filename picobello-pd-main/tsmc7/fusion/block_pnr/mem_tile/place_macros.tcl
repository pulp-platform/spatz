# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status unplaced

remove_edit_groups -all
remove_routing_blockages -all
if {[sizeof [get_keepout_margins -quiet *] ] > 0} {remove_keepout_margins *}

source $FCDIR/scripts/fp_utils.tcl

set SRAMacros [get_memory_macros "gen_sram_banks_*__gen_sram_macros_*__i_mem/gen_1024x128m4_i_cut"]
set SRAMNumMacros [llength $SRAMacros]

set SRAMHeight  [get_attribute [lindex $SRAMacros 0] height]
set SRAMWidth   [get_attribute [lindex $SRAMacros 0] width]

set vert_channel_front [expr 10 * $macro_hor_keepout_margin]
set vert_channel_back [expr $macro_hor_keepout_margin]

set num_macros(tr) [expr 3 * 6]; # 3 rows, 6 columns
set num_macros(tl) [expr 2 * 7]; # 2 rows, 7 columns
set num_macros(br) [expr 2 * 7]; # 2 rows, 7 columns
set num_macros(bl) [expr 3 * 6]; # 3 rows, 6 columns

set i 0;
foreach corner {tr tl br bl} {
  set macros($corner) [lrange $SRAMacros $i [expr $i + $num_macros($corner) - 1]]
  set i [expr $i + $num_macros($corner)]
}

set sram_eg_tr [create_macro_array \
  -num_rows 3 -num_cols 6  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 9 [list $vert_channel_front $vert_channel_back]]] \
  -orientation [concat {*}[lrepeat 9 {N FN}]] \
  -fill_pattern by_row \
  $macros(tr)]

set sram_eg_tl [create_macro_array \
  -num_rows 2 -num_cols 7  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 7 [list $vert_channel_front $vert_channel_back]]] \
  -orientation [concat {*}[lrepeat 4 {N N FN FN}]] \
  -fill_pattern by_col \
  $macros(tl)]

set sram_eg_br [create_macro_array \
  -num_rows 2 -num_cols 7  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 7 [list $vert_channel_back $vert_channel_front ]]] \
  -orientation [concat {*}[lrepeat 4 {FN FN N N}]] \
  -fill_pattern by_col \
  $macros(br)]

set sram_eg_bl [create_macro_array \
  -num_rows 3 -num_cols 6  \
  -horizontal_channel_height [expr $macro_ver_keepout_margin] \
  -vertical_channel_width [concat {*}[lrepeat 9 [list $vert_channel_front $vert_channel_back]]] \
  -orientation [concat {*}[lrepeat 9 {N FN}]] \
  -fill_pattern by_row \
  $macros(bl)]

# Set the relative locations of each macro array
set_macro_relative_location \
  -target_object $sram_eg_tl \
  -target_corner tl -target_orientation R0 \
  -anchor_corner tl \
  -offset [list $macro_hor_keepout_margin -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $sram_eg_tr \
  -target_corner tr -target_orientation R0 \
  -anchor_corner tr \
  -offset [list -$macro_hor_keepout_margin -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $sram_eg_bl \
  -target_corner bl -target_orientation R0 \
  -anchor_corner bl \
  -offset [list $macro_hor_keepout_margin $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $sram_eg_br \
  -target_corner br -target_orientation R0 \
  -anchor_corner br \
  -offset [list -$macro_hor_keepout_margin $macro_ver_keepout_margin]

# Create the actual placement
create_macro_relative_location_placement

# Add keepout margins around the srams
create_sram_keepouts [get_cells $SRAMacros]

# This is necessary for the design planning flow, otherwise
# the macros will be replaced
set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status fixed
