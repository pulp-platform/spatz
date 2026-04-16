# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set_attribute [get_cells -filter {design_type =~ macro}] physical_status unplaced

remove_edit_groups -all
remove_routing_blockages -all
if {[sizeof [get_keepout_margins -quiet *] ] > 0} {remove_keepout_margins *}

source $FCDIR/scripts/fp_utils.tcl


# Type E : ICacheDataMacros
set ICacheDataMacros [get_memory_macros "i_cheshire_soc/gen_cva6_cores_0__i_core_cva6/gen_cache_wb_i_cache_subsystem/i_cva6_icache_axi_wrapper/i_cva6_icache/*_data_sram/*"]
set ICacheDataHeight [get_attribute [lindex $ICacheDataMacros 0] height]
set ICacheDataWidth  [get_attribute [lindex $ICacheDataMacros 0] width]


# Type C : ICacheTagMacros
set ICacheTagMacros [get_memory_macros "i_cheshire_soc/gen_cva6_cores_0__i_core_cva6/gen_cache_wb_i_cache_subsystem/i_cva6_icache_axi_wrapper/i_cva6_icache/*_tag_sram/*"]
set ICacheTagHeight [get_attribute [lindex $ICacheTagMacros 0] height]
set ICacheTagWidth  [get_attribute [lindex $ICacheTagMacros 0] width]


# Type D : NCacheDataMacros
set NCacheDataMacros [get_memory_macros "i_cheshire_soc/gen_cva6_cores_0__i_core_cva6/gen_cache_wb_i_cache_subsystem/i_nbdcache/*_data_sram/*"]
set NCacheDataHeight [get_attribute [lindex $NCacheDataMacros 0] height]
set NCacheDataWidth  [get_attribute [lindex $NCacheDataMacros 0] width]


# Type B : NCacheTagMacros
set NCacheTagMacros [get_memory_macros "i_cheshire_soc/gen_cva6_cores_0__i_core_cva6/gen_cache_wb_i_cache_subsystem/i_nbdcache/*_tag_sram/*"]
set NCacheTagHeight [get_attribute [lindex $NCacheTagMacros 0] height]
set NCacheTagWidth  [get_attribute [lindex $NCacheTagMacros 0] width]


# Type F : NCacheVDMacros
set NCacheVDMacros [get_memory_macros "i_cheshire_soc/gen_cva6_cores_0__i_core_cva6/gen_cache_wb_i_cache_subsystem/i_nbdcache/valid_dirty_sram/*"]
set NCacheVDHeight [get_attribute [lindex $NCacheVDMacros 0] height]
set NCacheVDWidth [get_attribute [lindex $NCacheVDMacros 0] width]


# Type A : LLCHMTagMacros
set LLCHMTagMacros [get_memory_macros "i_cheshire_soc/gen_llc_i_llc/i_axi_llc_top_raw/i_hit_miss_unit/i_tag_store/gen_tag_macros*/*"]
set LLCHMTagHeight [get_attribute [lindex $LLCHMTagMacros 0] height]
set LLCHMTagWidth  [get_attribute [lindex $LLCHMTagMacros 0] width]


# Type G : LLCDataMacros
set LLCDataMacros [get_memory_macros "i_cheshire_soc/gen_llc_i_llc/i_axi_llc_top_raw/i_llc_ways/gen_data_ways_*/*"]
set LLCDataHeight [get_attribute [lindex $LLCDataMacros 0] height]
set LLCDataWidth  [get_attribute [lindex $LLCDataMacros 0] width]

# Set the vertical channels for each macro into 200 * site_width
set vert_macro_channel [expr 200 * $site_width]

# Set the height of macro array
set ncache_data_eg_height [expr (2*$NCacheDataHeight + 4*$site_height)]
set ncache_tag_eg_height  [expr (2*$NCacheTagHeight + 4*$site_height)]


set icache_data_eg [create_macro_array \
  -num_rows 1 -num_cols 4 \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_macro_channel $macro_hor_keepout_margin ]]] \
  -orientation [concat {*}[lrepeat 2 {N FN}]] \
  $ICacheDataMacros
]


set icache_tag_eg [create_macro_array \
  -num_rows 1 -num_cols 4 \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $macro_hor_keepout_margin $vert_macro_channel]]] \
  -orientation [concat {*}[lrepeat 2 {FN N}]] \
  $ICacheTagMacros
]


set ncache_data_eg [create_macro_array \
  -num_rows 2 -num_cols 4 \
  -horizontal_channel_height [expr 20 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_macro_channel $macro_hor_keepout_margin ]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $NCacheDataMacros
  ]


set ncache_tag_eg [create_macro_array \
  -num_rows 2 -num_cols 4 \
  -horizontal_channel_height [expr 20 * $site_height] \
  -vertical_channel_width [concat {*}[lrepeat 2 [list $vert_macro_channel $macro_hor_keepout_margin ]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $NCacheTagMacros
]


set ncache_vd_eg [create_macro_array \
  -num_rows 1 -num_cols 1 \
  -orientation {N} \
  $NCacheVDMacros
]


set llc_data_eg [create_macro_array \
  -num_rows 1 -num_cols 8 \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_macro_channel $macro_hor_keepout_margin]]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $LLCDataMacros
]


set llc_hitmiss_tag_eg [create_macro_array \
  -num_rows 1 -num_cols 8 \
  -vertical_channel_width [concat {*}[lrepeat 4 [list $vert_macro_channel [expr 2*$LLCDataWidth - 2*$LLCHMTagWidth + $macro_hor_keepout_margin]]]]  \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $LLCHMTagMacros
]



# Set the relative locations of each macro array
set_macro_relative_location \
  -target_object $icache_data_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_corner tr \
  -offset [list -[expr $macro_hor_keepout_margin] -$macro_ver_keepout_margin]


set_macro_relative_location \
  -target_object $icache_tag_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_object $icache_data_eg \
  -anchor_corner tl \
  -offset [list -[expr 2*$macro_hor_keepout_margin + $vert_macro_channel] 0]


set_macro_relative_location \
  -target_object $ncache_data_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_object $icache_data_eg \
  -anchor_corner br \
  -offset [list 0 -[expr 100*$site_height]]


set_macro_relative_location \
  -target_object $ncache_tag_eg \
  -target_corner br -target_orientation R0 \
  -anchor_object $ncache_data_eg \
  -anchor_corner bl \
  -offset [list -[expr 2*$macro_hor_keepout_margin + $vert_macro_channel] [expr ($ncache_data_eg_height - $ncache_tag_eg_height)/2 ]]


set_macro_relative_location \
  -target_object $ncache_vd_eg \
  -target_corner br -target_orientation R0 \
  -anchor_object $ncache_tag_eg \
  -anchor_corner bl \
  -offset [list -[expr 2*$macro_hor_keepout_margin + $vert_macro_channel] [expr ($ncache_tag_eg_height - $NCacheVDHeight)/2 ]]


set_macro_relative_location \
  -target_object $llc_data_eg \
  -target_corner br -target_orientation R0 \
  -anchor_corner br \
  -offset [list -[expr $macro_hor_keepout_margin] $macro_ver_keepout_margin]


set_macro_relative_location \
  -target_object $llc_hitmiss_tag_eg \
  -target_corner br -target_orientation R0 \
  -anchor_object $llc_data_eg \
  -anchor_corner tr \
  -offset [list [expr -($LLCDataWidth - $LLCHMTagWidth)] $macro_ver_keepout_margin]





# Create the actual placement
create_macro_relative_location_placement


create_sram_keepouts [get_cells $ICacheDataMacros]
create_sram_keepouts [get_cells $ICacheTagMacros]
create_sram_keepouts [get_cells $NCacheDataMacros]
create_sram_keepouts [get_cells $NCacheTagMacros]
create_sram_keepouts [get_cells $NCacheVDMacros]
create_sram_keepouts [get_cells $LLCHMTagMacros]
create_sram_keepouts [get_cells $LLCDataMacros]

# Add a placement blockage in order to fix some boundary cell issues
# TODO(fischeti): Remove this once the issues are fixed
remove_placement_blockages -all

if {[sizeof_collection [get_grids site_row_grid -quiet]] > 0} {
    remove_grids site_row_grid
}
create_grid -x_step [expr 2 * $site_width] -y_step $site_height site_row_grid -x_offset $site_width


# This is necessary for the design planning flow, otherwise
# the macros will be replaced
set_attribute [get_cells -filter {design_type =~ macro}] physical_status fixed
