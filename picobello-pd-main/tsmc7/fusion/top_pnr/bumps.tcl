# Copyright (c) 2025 ETH Zurich.
# Albi Mema <almema@iis.ee.ethz.ch>
# Yichao Zhang <yiczhang@iis.ee.ethz.ch>
# Tim Fischer <fischeti@iis.ee.ethz.ch>

source $FCDIR/scripts/fp_utils.tcl

# Include the bump library
if {[sizeof_collection [get_libs N10_N7_EU_LF_Bump]] == 0} {
    set_ref_libs -add N10_N7_EU_LF_Bump.ndm
}

set_app_options -name design.enable_lib_cell_editing -value mutable
set_attribute [get_lib_pins -of_objects [get_lib_cells 220P]] is_pad true

##################
##  Bump Array  ##
##################

# Remove previous bump array if it exists
if {[sizeof_collection [get_cells -quiet bump_array*]] > 0} {
    remove_cells [get_cells -quiet bump_array*]
}

# Remove previous RDL shapes if they exist
proc remove_rdl_shapes {} {
    set rdl_shapes [get_shapes -quiet -filter {shape_use==rdl}]
    if {[sizeof_collection $rdl_shapes] > 0} {
        remove_shapes $rdl_shapes
    }
}

remove_rdl_shapes

set bump_bbox [get_attribute [get_lib_cells 220P] bbox]

set bump_x0 [lindex $bump_bbox 0 0]
set bump_x1 [lindex $bump_bbox 1 0]
set bump_y0 [lindex $bump_bbox 0 1]
set bump_y1 [lindex $bump_bbox 1 1]
set bump_dx [expr $bump_x1 - $bump_x0]
set bump_dy [expr $bump_y1 - $bump_y0]

set fp_x0_bump [expr $fp_x0 + $bump_dx]
set fp_y0_bump [expr $fp_y0 + $bump_dy]
set fp_x1_bump [expr $fp_x1 - $bump_dx]
set fp_y1_bump [expr $fp_y1 - $bump_dy]

# Create a polygon with removed corners
set bump_region [list \
    [list $fp_x0_bump   $fp_y0] \
    [list $fp_x0_bump   $fp_y0_bump] \
    [list $fp_x0        $fp_y0_bump] \
    [list $fp_x0        $fp_y1_bump] \
    [list $fp_x0_bump   $fp_y1_bump] \
    [list $fp_x0_bump   $fp_y1] \
    [list $fp_x1_bump   $fp_y1] \
    [list $fp_x1_bump   $fp_y1_bump] \
    [list $fp_x1        $fp_y1_bump] \
    [list $fp_x1        $fp_y0_bump] \
    [list $fp_x1_bump   $fp_y0_bump] \
    [list $fp_x1_bump   $fp_y0] \
    [list $fp_x0_bump   $fp_y0]
]

# Create the actual bump array using the polygon
create_bump_array \
    -lib_cell [get_object_name [get_lib_cells 220P]] \
    -delta {112.5 225} \
    -pattern staggered_1 \
    -boundary $bump_region \
    -origin {20.66 14.66} \
    -origin_spec bl \
    -name bump_array

# Fix the placement of the bump array
set_attribute [get_cells -of_references [get_lib_cells 220P]] physical_status fixed

####################
##  Bump Mapping  ##
####################

source $FCDIR/top_pnr/bump_mapping.tcl

place_io -bump_assignment_only -matching_types [get_matching_types *]

###################
##  RDL routing  ##
###################

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
route_rdl_differential -layer {AP} -nets [get_nets pad_dram_slink*_i*]
route_rdl_differential -layer {AP} -nets [get_nets pad_dram_slink*_o*]

# Create the manual power RDL shapes again, because `route_rdl_flip_chip` deletes them
source $FCDIR/top_pnr/power_bump_routing.tcl
# Remove the routing blockages again
remove_routing_blockages manual_rdl_shape_blockage*

# Check for violations
check_rdl_routes
