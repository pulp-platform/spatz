# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set die_area [get_attribute [current_design] boundary_bounding_box]
set core_area [get_attribute [get_core_area] bounding_box]

set llx [get_attribute $die_area ll_x]
set lly [get_attribute $die_area ll_y]
set urx [get_attribute $die_area ur_x]
set ury [get_attribute $die_area ur_y]

set new_lly 0.0600
set new_ury [expr $ury - 0.0600]

# set new_die_area_boundary {{0.0000 0.0600} {0.0000 3911.22} {4765.5600 3911.22} {4765.5600 0.0600}}
set new_die_area_boundary [list [list $llx $new_lly] [list $llx $new_ury] [list $urx $new_ury] [list $urx $new_lly]]

set_attribute [current_design] boundary $new_die_area_boundary

proc get_cells_in_between {cell1 cell2} {
  set bbox1 [get_attribute $cell1 boundary_bounding_box]
  set bbox2 [get_attribute $cell2 boundary_bounding_box]
  set area [list \
    [list [get_attribute $bbox1 ll_x] [get_attribute $bbox1 ll_y]] \
    [list [get_attribute $bbox2 ur_x] [get_attribute $bbox2 ur_y]]]
  return [get_cells -touching $area -filter {is_io}]
}

set bl_corner_cell [get_cells *corner_cell_2]
set tl_corner_cell [get_cells *corner_cell_1]
set tr_corner_cell [get_cells *corner_cell_4]
set br_corner_cell [get_cells *corner_cell_3]

set filler_bl_to_remove [get_cells_in_between [get_cells left_io_power_0] $bl_corner_cell ]
set filler_br_to_remove [get_cells_in_between [get_cells right_io_power_11] $br_corner_cell]
set filler_tl_to_remove [get_cells_in_between $tl_corner_cell [get_cells left_io_power_11]]
set filler_tr_to_remove [get_cells_in_between $tr_corner_cell [get_cells right_io_power_0]]

remove_cells $filler_bl_to_remove
remove_cells $filler_br_to_remove
remove_cells $filler_tl_to_remove
remove_cells $filler_tr_to_remove

set bottom_ios [get_cells_in_between $bl_corner_cell $br_corner_cell]
set top_ios [get_cells_in_between $tl_corner_cell $tr_corner_cell]

set_attribute $bottom_ios physical_status unplaced
set_attribute $top_ios physical_status unplaced

foreach_in_collection cell $bottom_ios {
  set origin [get_attribute $cell origin]
  set new_origin [list [lindex $origin 0] [expr [lindex $origin 1] + 0.06]]
  set_attribute $cell origin $new_origin
}

foreach_in_collection cell $top_ios {
  set origin [get_attribute $cell origin]
  set new_origin [list [lindex $origin 0] [expr [lindex $origin 1] - 0.06]]
  set_attribute $cell origin $new_origin
}

create_io_filler_cells \
  -io_guides [get_io_guides {*left *right}] \
  -reference_cell {PFILLERSTRAP_H PFILLER000005 PFILLER000300 PFILLER006300} \
  -prefix die_size_io_filler

connect_net -net rte [get_pins -of_objects [get_cells die_size_io_filler*] -filter name==RTE]
connect_pg_net -net VDD [get_pins -of_objects [get_cells die_size_io_filler*] -filter name==VDD]
connect_pg_net -net VSS [get_pins -of_objects [get_cells die_size_io_filler*] -filter name==VSS]
connect_pg_net -net VDDIO [get_pins -of_objects [get_cells die_size_io_filler*] -filter name==VDDPST]
connect_pg_net -net VSSIO [get_pins -of_objects [get_cells die_size_io_filler*] -filter name==VSSPST]

proc get_io_cells {} {
  return [get_cells -hierarchical -quiet -filter {is_io || pad_cell}]
}

set_attribute -objects [get_io_cells] -name status -value fixed

set bottom_io_io_cells [filter_collection $bottom_ios "ref_name==PDDWUWSWEWCDGS_V"]
set top_io_io_cells [filter_collection $top_ios "ref_name==PDDWUWSWEWCDGS_V"]

trim_pg_mesh -layers {M9 M11 M13}


# Remove previous RDL shapes if they exist
proc remove_rdl_shapes {} {
    set rdl_shapes [get_shapes -quiet -filter {shape_use==rdl}]
    if {[sizeof_collection $rdl_shapes] > 0} {
        remove_shapes $rdl_shapes
    }
}

# Remove rdl shapes if not already done
remove_rdl_shapes

set_app_options -name flip_chip.route.connect_edge_center -value true
set_app_options -name flip_chip.route.connect_pad_without_endcap -value true

if {[sizeof_collection [get_routing_rules rdl_routing_rule -quiet]] > 0} {
    remove_routing_rules rdl_routing_rule
}

create_routing_rule rdl_routing_rule -width {AP 8.1} -spacings {AP 8}
set_routing_rule -rule rdl_routing_rule [get_nets -filter {is_rdl && (net_type==power || net_type==ground)}]

source $FCDIR/top_pnr/power_bump_routing.tcl

# Some cells are assigned to the same bumps and require separate RDL routes (i.e. star routing)
set_app_options -name flip_chip.route.routing_style -value star
set multibump_pins [get_attribute [get_matching_types -filter "uniquify_number > 0"] objects]
route_rdl_flip_chip -layers {AP} -objects $multibump_pins

# Afterwards, we route the remaining RDL signals, and we can switch back to spanning tree routing
set_app_options -name flip_chip.route.routing_style -value spanning_tree
route_rdl_flip_chip -layers {AP}

# Optimize the RDL routes
optimize_rdl_routes -layer {AP}

# Perform differential routing for serial link signals
route_rdl_differential -layer {AP} -nets [get_nets pad_slink*_i*]
route_rdl_differential -layer {AP} -nets [get_nets pad_slink*_o*]
# route_rdl_differential -layer {AP} -nets [get_nets pad_dram_slink*_i*]
# route_rdl_differential -layer {AP} -nets [get_nets pad_dram_slink*_o*]

# Create the manual power RDL shapes again, because `route_rdl_flip_chip` deletes them
source $FCDIR/top_pnr/power_bump_routing.tcl
# Remove the routing blockages again
remove_routing_blockages manual_rdl_shape_blockage*

# Check for violations
check_rdl_routes

set bottom_io_signal_pins [filter_collection [get_pins -of_objects $bottom_io_io_cells] "port_type==signal and name!=RTE and name!=PAD"]
set top_io_signal_pins [filter_collection [get_pins -of_objects $top_io_io_cells] "port_type==signal and name!=RTE and name!=PAD"]

set io_signal_nets ""
set io_signal_nets [add_to_collection $io_signal_nets [get_nets -of_objects $bottom_io_signal_pins]]
set io_signal_nets [add_to_collection $io_signal_nets [get_nets -of_objects $top_io_signal_pins]]

set_app_options -name route.global.timing_driven -value false
set_app_options -name route.track.timing_driven -value false
set_app_options -name route.detail.timing_driven -value false
set_app_options -name route.detail.antenna -value false

route_eco -nets $io_signal_nets

move_block_origin [current_design] -to [list $llx $new_lly]
