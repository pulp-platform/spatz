# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status unplaced

remove_edit_groups -all
remove_routing_blockages -all
if {[sizeof [get_keepout_margins -quiet *] ] > 0} {remove_keepout_margins *}

source $FCDIR/scripts/fp_utils.tcl

set ICacheDataMacros [get_memory_macros "i_fhg_cluster/i_cluster/gen_hive*/i_snitch_icache/*i_lookup/*i_data/*"]
set ICacheDataHeight [get_attribute [lindex $ICacheDataMacros 0] height]
set ICacheDataWidth  [get_attribute [lindex $ICacheDataMacros 0] width]

set ICacheTagMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_hive*/i_snitch_icache/*i_lookup/*i_tag/*"]
set ICacheTagHeight  [get_attribute [lindex $ICacheTagMacros 0] height]
set ICacheTagWidth   [get_attribute [lindex $ICacheTagMacros 0] width]

set allTCDMMacros    [get_memory_macros "i_fhg_cluster/i_cluster/gen_tcdm_super_bank*/*"]
set TCDMHeight       [get_attribute [lindex $allTCDMMacros 0] height]
set TCDMWidth        [get_attribute [lindex $allTCDMMacros 0] width]

set Bank0TCDMMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_tcdm_super_bank_0*/*"]
set Bank1TCDMMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_tcdm_super_bank_1*/*"]
set Bank2TCDMMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_tcdm_super_bank_2*/*"]
set Bank3TCDMMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_tcdm_super_bank_3*/*"]

set allAuMacros      [get_memory_macros "i_fhg_cluster/i_cluster/gen_core_*__i_snitch_cc/gen_spu_i_spu_top/gen_spu_0__u_spu_subsystem/u_imem_au/u_memory/col_*__row_*__generic_mem/*"]
set AuHeight         [get_attribute [lindex $allAuMacros 0] height]
set AuWidth          [get_attribute [lindex $allAuMacros 0] width]

set allSagMacros     [get_memory_macros "i_fhg_cluster/i_cluster/gen_core_*__i_snitch_cc/gen_spu_i_spu_top/gen_spu_0__u_spu_subsystem/u_imem_sag/u_memory/col_*__row_*__generic_mem/*"]
set SagHeight        [get_attribute [lindex $allSagMacros 0] height]
set SagWidth         [get_attribute [lindex $allSagMacros 0] width]

set small_vert_tcdm_height [expr 30 * $site_height]
set large_vert_tcdm_height [expr 60 * $site_height]

set c_fp_dx [lindex [lindex [get_attribute [get_block] boundary] 2] 0]
set c_fp_dy [lindex [lindex [get_attribute [get_block] boundary] 2] 1]

set middle_fp_dx [expr int(($c_fp_dx-$macro_hor_keepout_margin-2*$site_width)/2.0/$site_width) * $site_width]

set vert_tcdm_channel [expr int(($c_fp_dx-8*$macro_hor_keepout_margin-14*$TCDMWidth-0*$site_width)/7.0/$site_width) * $site_width]
set vert_macro_channel [expr 200 * $site_width]

remove_edit_groups -all

set tcdm_bank_0_eg [create_macro_array \
  -num_rows 4 -num_cols 2  \
  -horizontal_channel_height [concat {*}[list $small_vert_tcdm_height $large_vert_tcdm_height $macro_ver_keepout_margin]] \
  -vertical_channel_width $vert_tcdm_channel \
  -orientation {FN FN FN FN N FN N FN} \
  $Bank0TCDMMacros]

set tcdm_bank_1_eg [create_macro_array \
  -num_rows 2 -num_cols 4  \
  -horizontal_channel_height $macro_ver_keepout_margin \
  -vertical_channel_width [concat {*}[list $vert_tcdm_channel $macro_hor_keepout_margin $vert_tcdm_channel]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $Bank1TCDMMacros]

set tcdm_bank_2_eg [create_macro_array \
  -num_rows 2 -num_cols 4  \
  -horizontal_channel_height $macro_ver_keepout_margin \
  -vertical_channel_width [concat {*}[list $vert_tcdm_channel $macro_hor_keepout_margin $vert_tcdm_channel]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $Bank2TCDMMacros]

set tcdm_bank_3_eg [create_macro_array \
  -num_rows 2 -num_cols 4  \
  -horizontal_channel_height $macro_ver_keepout_margin \
  -vertical_channel_width [concat {*}[list $vert_tcdm_channel $macro_hor_keepout_margin $vert_tcdm_channel]] \
  -orientation [concat {*}[lrepeat 4 {N FN}]] \
  $Bank3TCDMMacros]

set spu_sag_macro_eg_list {}
set spu_au_macro_eg_list {}

for {set i 0} {$i < 8} {incr i} {
  set selAuMacros  [get_memory_macros "i_fhg_cluster/i_cluster/gen_core_${i}__i_snitch_cc/gen_spu_i_spu_top/gen_spu_0__u_spu_subsystem/u_imem_au/u_memory/col_*__row_*__generic_mem/*"]
  set selSagMacros [get_memory_macros "i_fhg_cluster/i_cluster/gen_core_${i}__i_snitch_cc/gen_spu_i_spu_top/gen_spu_0__u_spu_subsystem/u_imem_sag/u_memory/col_*__row_*__generic_mem/*"]
  switch $i {
    1 -
    5 -
    3 -
    4 {
      lappend spu_sag_macro_eg_list [create_macro_array \
        -num_rows 2 -num_cols 2  \
        -vertical_channel_width $vert_macro_channel \
        -orientation {N N} \
        $selSagMacros ]

      lappend spu_au_macro_eg_list [create_macro_array \
        -num_rows 1 -num_cols 1  \
        -orientation {N} \
        $selAuMacros ]
    }
    default {
      lappend spu_sag_macro_eg_list [create_macro_array \
        -num_rows 2 -num_cols 2  \
        -vertical_channel_width $vert_macro_channel \
        -orientation {FN FN FN} \
        $selSagMacros ]

      lappend spu_au_macro_eg_list [create_macro_array \
        -num_rows 1 -num_cols 1  \
        -orientation {FN} \
        $selAuMacros ]
    }
  }
}

set icache_data_eg [create_macro_array \
  -num_rows 1 -num_cols 2  \
  -vertical_channel_width $vert_macro_channel \
  -orientation FN \
  $ICacheDataMacros]

set icache_tag_eg [create_macro_array \
  -num_rows 1 -num_cols 2  \
  -vertical_channel_width $vert_macro_channel \
  -orientation FN \
  $ICacheTagMacros]

# Set the relative locations of each macro array
set_macro_relative_location \
  -target_object $tcdm_bank_0_eg \
  -target_corner br -target_orientation R0 \
  -anchor_corner br \
  -offset [list -[expr $macro_hor_keepout_margin + 0] $macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object $tcdm_bank_1_eg \
  -target_corner br -target_orientation R0 \
  -anchor_corner bl -anchor_object $tcdm_bank_0_eg \
  -offset [list -$macro_hor_keepout_margin 0]

set_macro_relative_location \
  -target_object $tcdm_bank_2_eg \
  -target_corner bl -target_orientation R0 \
  -anchor_corner br -anchor_object $tcdm_bank_3_eg \
  -offset [list $macro_hor_keepout_margin 0]

set_macro_relative_location \
  -target_object $tcdm_bank_3_eg \
  -target_corner bl -target_orientation R0 \
  -anchor_corner bl \
  -offset [list [expr $macro_hor_keepout_margin + 0] $macro_ver_keepout_margin]

# SPU 0

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 0] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner tr \
  -offset [list -[expr $macro_hor_keepout_margin + 0] -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 0] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_sag_macro_eg_list 0] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 1

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 1] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner tr \
  -offset [list -[expr $middle_fp_dx + $site_width] -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 1] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_sag_macro_eg_list 1] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 2

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 2] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner tl -anchor_object [lindex $spu_sag_macro_eg_list 1] \
  -offset [list -$macro_hor_keepout_margin 0]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 2] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_sag_macro_eg_list 2] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 3

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 3] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner tl \
  -offset [list [expr $macro_hor_keepout_margin + 0] -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 3] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_sag_macro_eg_list 3] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 4

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 4] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_au_macro_eg_list 3] \
  -offset [list 0 -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 4] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_au_macro_eg_list 4] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 5

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 5] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_sag_macro_eg_list 4] \
  -offset [list 0 -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 5] \
  -target_corner tl -target_orientation R0 \
  -anchor_corner bl -anchor_object [lindex $spu_sag_macro_eg_list 5] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 6

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 6] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_sag_macro_eg_list 7] \
  -offset [list 0 -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 6] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_sag_macro_eg_list 6] \
  -offset [list 0 -$macro_ver_keepout_margin]

# SPU 7

set_macro_relative_location \
  -target_object [lindex $spu_au_macro_eg_list 7] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_au_macro_eg_list 0] \
  -offset [list 0 -$macro_ver_keepout_margin]

set_macro_relative_location \
  -target_object [lindex $spu_sag_macro_eg_list 7] \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_au_macro_eg_list 7] \
  -offset [list 0 -$macro_ver_keepout_margin]

# I$

set_macro_relative_location \
  -target_object $icache_tag_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object [lindex $spu_au_macro_eg_list 6] \
  -offset [list 0 -[expr 2*$large_vert_tcdm_height]]

set_macro_relative_location \
  -target_object $icache_data_eg \
  -target_corner tr -target_orientation R0 \
  -anchor_corner br -anchor_object $icache_tag_eg \
  -offset [list 0 -$small_vert_tcdm_height]

# Create the actual placement
create_macro_relative_location_placement

# Snap macros to cell site

if {[sizeof_collection [get_grids site_row_grid -quiet]] > 0} {
    remove_grids site_row_grid
}
create_grid -x_step [expr 1 * $site_width] -y_step $site_height site_row_grid -x_offset $site_width
snap_cells_to_block_grid -grid site_row_grid -cells [get_cells $allTCDMMacros]
snap_cells_to_block_grid -grid site_row_grid -cells [get_cells $allAuMacros]
snap_cells_to_block_grid -grid site_row_grid -cells [get_cells $allSagMacros]
snap_cells_to_block_grid -grid site_row_grid -cells [get_cells $ICacheDataMacros]
snap_cells_to_block_grid -grid site_row_grid -cells [get_cells $ICacheTagMacros]

# Add keepout margins around the srams
create_sram_keepouts [get_cells $ICacheDataMacros]
create_sram_keepouts [get_cells $ICacheTagMacros]
create_sram_keepouts [get_cells $allTCDMMacros]
create_sram_keepouts [get_cells $allAuMacros]
create_sram_keepouts [get_cells $allSagMacros]

# This is necessary for the design planning flow, otherwise
# the macros will be replaced
set_attribute [get_cells -hierarchical -filter {design_type =~ macro}] physical_status fixed
