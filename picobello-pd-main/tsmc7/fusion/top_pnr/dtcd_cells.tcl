# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

if {[sizeof_collection [get_lib_cells *DTCD*]] == 0} {
    set_ref_libs -add N7_DTCD.ndm
}

if {[sizeof_collection [get_cells -quiet dtcd_cell*]] > 0} {
    remove_cells [get_cells -quiet dtcd_cell*]
}

create_cell dtcd_cell_tl [get_lib_cells *DTCD*]
create_cell dtcd_cell_tr [get_lib_cells *DTCD*]
create_cell dtcd_cell_bl [get_lib_cells *DTCD*]
create_cell dtcd_cell_br [get_lib_cells *DTCD*]
create_cell dtcd_cell_bc [get_lib_cells *DTCD*]
create_cell dtcd_cell_tc [get_lib_cells *DTCD*]

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_tl] \
  -target_corner tl \
  -target_orientation R0 \
  -anchor_corner tl \
  -offset {96 -60.3}

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_tr] \
  -target_corner tr \
  -target_orientation R0 \
  -anchor_corner tr \
  -offset {-96 -60.3}

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_bl] \
  -target_corner bl \
  -target_orientation R0 \
  -anchor_corner bl \
  -offset {96 60.3}

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_br] \
  -target_corner br \
  -target_orientation R0 \
  -anchor_corner br \
  -offset {-96 60.3}

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_bc] \
  -target_corner bl \
  -target_orientation R0 \
  -anchor_object [get_cells bottom_core_power_5] \
  -anchor_corner br \
  -offset {14.5 60}

set_macro_relative_location \
  -target_object [get_cells dtcd_cell_tc] \
  -target_corner tl \
  -target_orientation R0 \
  -anchor_object [get_cells top_core_power_5] \
  -anchor_corner tr \
  -offset {14.5 -60}

create_macro_relative_location_placement

set_attribute [get_cells dtcd_cell*] physical_status fixed
