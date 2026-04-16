# Copyright (c) 2025 ETH Zurich.
# Albi Mema <almema@iis.ee.ethz.ch>

set AP_width 31.5
set bump_dim [get_attribute [get_lib_cells 220P] width]

proc get_manual_rdl_shapes {} {
    global AP_width
    set shapes [get_shapes -quiet -filter "owner==[get_nets VDD] && width==$AP_width"]
    set shapes [add_to_collection $shapes [get_shapes -quiet -filter "owner==[get_nets VSS] && width==$AP_width"]]
    return $shapes
}


proc remove_manual_rdl_shapes {} {
    if {[sizeof_collection [get_manual_rdl_shapes]] > 0} {
        remove_shapes -force [get_manual_rdl_shapes]
    }

    if {[sizeof_collection [get_routing_blockages -quiet manual_rdl_shape_blockage*]] > 0} {
        remove_routing_blockages -force [get_routing_blockages -quiet manual_rdl_shape_blockage*]
    }
}

remove_manual_rdl_shapes

proc bump2bump_path {bump_col} {
    global AP_width bump_dim bump_dict_inv

    for {set i 0} {$i < [sizeof_collection $bump_col] - 1} {incr i} {

        set start_bump [index_collection $bump_col $i]
        set end_bump [index_collection $bump_col [expr {$i + 1}]]

        set start_net [dict get $bump_dict_inv [get_object_name $start_bump]]
        set end_net [dict get $bump_dict_inv [get_object_name $end_bump]]

        if {$start_net != $end_net} {
            error "Bumps $start_bump and $end_bump are not connected to the same net: $start_net != $end_net"
            return
        }

        set start_origin [get_attribute [get_cell $start_bump] origin]
        set end_origin [get_attribute [get_cell $end_bump] origin]

        set diff_x [expr {[lindex $end_origin 0] - [lindex $start_origin 0]}]
        set diff_y [expr {[lindex $end_origin 1] - [lindex $start_origin 1]}]

        if {$diff_x == 0} {
            set diff_sign_y [expr {abs($diff_y) / $diff_y}]
            set start_origin [list [lindex $start_origin 0] [expr {[lindex $start_origin 1] + $diff_sign_y * ($bump_dim - $AP_width) / 2}]]
            set end_origin [list [lindex $end_origin 0] [expr {[lindex $end_origin 1] - $diff_sign_y * ($bump_dim - $AP_width) / 2}]]
        } elseif {$diff_y == 0} {
            set diff_sign_x [expr {abs($diff_x) / $diff_x}]
            set start_origin [list [expr {[lindex $start_origin 0] + $diff_sign_x * ($bump_dim - $AP_width) / 2}] [lindex $start_origin 1]]
            set end_origin [list [expr {[lindex $end_origin 0] - $diff_sign_x * ($bump_dim - $AP_width) / 2}] [lindex $end_origin 1]]
        } else {
            puts "Error: Bumps are not aligned vertically or horizontally."
        }

        create_shape \
            -shape_type path -layer AP \
            -shape_use rdl -net $start_net \
            -width $AP_width -path [list $start_origin $end_origin] \
            -start_endcap octagon -end_endcap octagon
    }
}

proc get_bumps_by_net {net {row "*"} {col "*"} args} {
    global bump_dict
    return [get_cells [lsearch -all -inline [dict get $bump_dict $net] bump_array_${row}_${col}]]
}

# Create horizontal manual VDD bump-to-bump paths
bump2bump_path [get_bumps_by_net VDD 2 *]
bump2bump_path [get_bumps_by_net VDD 3 *]
bump2bump_path [get_bumps_by_net VDD 4 *]
bump2bump_path [get_bumps_by_net VDD 5 *]
bump2bump_path [get_bumps_by_net VDD 6 *]
bump2bump_path [get_bumps_by_net VDD 7 *]
bump2bump_path [get_bumps_by_net VDD 8 *]
bump2bump_path [get_bumps_by_net VDD 9 *]
bump2bump_path [get_bumps_by_net VDD 10 *]
bump2bump_path [get_bumps_by_net VDD 11 *]
bump2bump_path [get_bumps_by_net VDD 12 *]
bump2bump_path [get_bumps_by_net VDD 13 *]
bump2bump_path [get_bumps_by_net VDD 14 *]

# Create vertical manual VDD bump-to-bump paths
bump2bump_path [get_bumps_by_net VDD * 7]
bump2bump_path [get_bumps_by_net VDD * 9]
bump2bump_path [get_bumps_by_net VDD * 11]
bump2bump_path [get_bumps_by_net VDD * 13]
bump2bump_path [get_bumps_by_net VDD * 15]
bump2bump_path [get_bumps_by_net VDD * 16]
bump2bump_path [get_bumps_by_net VDD * 17]
bump2bump_path [index_collection [get_bumps_by_net VDD * 18] 1 end-1]
bump2bump_path [get_bumps_by_net VDD * 19]
bump2bump_path [get_bumps_by_net VDD * 20]
bump2bump_path [get_bumps_by_net VDD * 21]
bump2bump_path [index_collection [get_bumps_by_net VDD * 22] 1 end-1]
# bump2bump_path [get_cells [lrange [lsearch -all -inline [dict get $bump_dict VDD] bump_array_*_22] 1 end-1]]
bump2bump_path [get_bumps_by_net VDD * 23]
bump2bump_path [get_bumps_by_net VDD * 24]
bump2bump_path [get_bumps_by_net VDD * 25]
bump2bump_path [get_bumps_by_net VDD * 27]
bump2bump_path [get_bumps_by_net VDD * 29]
bump2bump_path [get_bumps_by_net VDD * 31]
bump2bump_path [get_bumps_by_net VDD * 33]

# Create horizontal manual VSS bump-to-bump paths
# Create vertical manual VSS bump-to-bump paths
bump2bump_path [index_collection [get_bumps_by_net VSS * 10] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 10] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 11] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 11] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 12] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 12] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 13] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 13] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 14] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 14] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 15] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 15] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 16] 1 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 16] end-2 end-1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 17] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 17] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 23] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 23] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 24] 1 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 24] end-2 end-1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 25] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 25] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 26] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 26] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 27] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 27] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 28] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS * 28] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 29] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 29] end-1 end]
bump2bump_path [index_collection [get_bumps_by_net VSS * 30] 0 1]
bump2bump_path [index_collection [get_bumps_by_net VSS * 30] end-1 end]

# Create horizontal manual VSS bump-to-bump paths
bump2bump_path [index_collection [get_bumps_by_net VSS 2 *] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS 2 *] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 3 *] 0 3]
bump2bump_path [index_collection [get_bumps_by_net VSS 3 *] end-3 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 4 *] 0 3]
bump2bump_path [index_collection [get_bumps_by_net VSS 4 *] end-3 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 5 *] 0 4]
bump2bump_path [index_collection [get_bumps_by_net VSS 5 *] end-4 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 6 *] 0 4]
bump2bump_path [index_collection [get_bumps_by_net VSS 6 *] end-4 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 7 *] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS 7 *] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 9 *] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS 9 *] end-2 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 10 *] 0 4]
bump2bump_path [index_collection [get_bumps_by_net VSS 10 *] end-4 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 11 *] 0 4]
bump2bump_path [index_collection [get_bumps_by_net VSS 11 *] end-4 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 12 *] 0 3]
bump2bump_path [index_collection [get_bumps_by_net VSS 12 *] end-3 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 13 *] 0 3]
bump2bump_path [index_collection [get_bumps_by_net VSS 13 *] end-3 end]
bump2bump_path [index_collection [get_bumps_by_net VSS 14 *] 0 2]
bump2bump_path [index_collection [get_bumps_by_net VSS 14 *] end-2 end]

# # Create routing blockages on top of the manually inserted metals,
# # since `route_rdl_flip_chip`, just removes then
foreach_in_collection shape [get_manual_rdl_shapes] {
    create_routing_blockage \
        -name_prefix manual_rdl_shape_blockage \
        -layers AP \
        -boundary [get_attribute $shape bbox]
}
