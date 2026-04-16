# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Remove all existing PG shapes, vias and terminals


source $FCDIR/scripts/fp_utils.tcl
if {![info exists NO_PG_REMOVE]} {
    remove_pg
}
if {[sizeof_collection [get_routing_blockages -quiet $pad2ring_RB_prefix*]] != 0} {
    puts "Removing existing routing blockages with prefix $pad2ring_RB_prefix*"
    remove_routing_blockages $pad2ring_RB_prefix*
}
if {[sizeof_collection [get_routing_blockages -quiet RV_blockage*]] != 0} {
    remove_routing_blockages RV_blockage*
}



# For blocks, expose only M11 terminals to the top-level
if {![string match "picobello_chip*" [get_attribute [current_design] lib_name]]} {

    compile_pg -strategies block_rail_pg_strategy -via_rule rail_pg_strategy_via_rule

    # Trim the rails before continuing with the power straps to prevent later dropping
    # of vias in places where the rails are not supposed to be
    # The `plan.pgroute.disable_floating_removal` option is automatically set
    # to true by fusion for hierarchical PG routing, but we don't want to connect
    # the M0 rails hierarchically.
    trim_pg_mesh -layers M0

    set_pg_strategy M4_to_SRAM_connection_pg_strategy \
        -pattern {{pattern: SRAM_conn_pattern} {nets: {VSS VDD}}} \
        -macros [get_cells -hierarchical -filter design_type==macro] \
        -tag M4_SRAM_connection

    set_pg_strategy_via_rule M11_to_M4_SRAM_pg_strategy_via_rule -via_rule { \
        {{{strategies: M11_mesh_pg_strategy             } {layers: M11}} \
         {{strategies: M4_to_SRAM_connection_pg_strategy} {layers: M4}} \
         {via_master: {pg_via45_mrule pg_via56_mrule pg_via67_mrule pg_via78_mrule pg_via89_mrule VIA910_1cut_BW114 VIA1011_1cut}}} \
        {{intersection: undefined}{via_master: NIL}}}

    compile_pg -strategies { \
        M3_M4_block_mesh_pg_strategy \
        M4_to_SRAM_connection_pg_strategy \
        M11_mesh_pg_strategy \
    } -via_rule { \
        M3_to_M0_rails_pg_strategy_via_rule \
        M4_to_SRAM_pins_pg_strategy_via_rule \
        M11_to_M4_SRAM_pg_strategy_via_rule \
        M11_to_M4_pg_strategy_via_rule
    }

    trim_pg_mesh -layers {M3 M4}

    # Create longer terminals on M11
    create_terminal -of_objects [get_shapes -of_objects {VDD VSS} -filter layer_name==M11]

} else {

    # Create a routing blockage on the entire core area on the RV via layer (to AP).
    # Otherwise, vias are created to the AP, which is not what we want.
    create_routing_blockage -name_prefix RV_blockage \
        -boundary [get_attribute [current_design] boundary] \
        -net_types {power ground} -layers RV

    # Create power ring around the entire core
    foreach layer_pair {{M10 M11} {M12 M13} {M14 M13}} {
        set hor_layer [lindex $layer_pair 0]
        set ver_layer [lindex $layer_pair 1]

        compile_pg -strategies pg_ring_${hor_layer}_${ver_layer}
    }

    compile_pg -strategies [list pg_ring_connect_M9 pg_ring_connect_M11 pg_ring_connect_M13]
    compile_pg -strategies [list pg_ring_connect_M10 pg_ring_connect_M12 pg_ring_connect_M14]

    # Before running the PG, create routing blockage on the pad to ring metals for every Core PWR IO. This way no wierd vias get created
    set layerlist {M9 M10 M11 M12 M13 M14}
    foreach cell [get_attribute [get_cells -of_references {PVDD1*}] full_name] {
        create_routing_blockage_pad2ring $cell $pad2ring_RB_prefix $layerlist
    }

    # Top level M0 rails
    compile_pg -strategies top_rail_pg_strategy -via_rule rail_pg_strategy_via_rule

    # Trim the rails before continuing with the power straps to prevent later dropping
    # of vias in places where the rails are not supposed to be
    # The `plan.pgroute.disable_floating_removal` option is automatically set
    # to true by fusion for hierarchical PG routing, but we don't want to connect
    # the M0 rails hierarchically.
    trim_pg_mesh -layers M0

    # Mesh on M3 and M4 (connect to SRAM), and M11 (connect to M4)
    compile_pg -strategies { \
        M3_M4_top_mesh_pg_strategy \
        M6_FLL_mesh_pg_strategy \
        M11_mesh_pg_strategy \
    } -via_rule { \
        M3_to_M0_rails_pg_strategy_via_rule \
        M4_to_SRAM_pins_pg_strategy_via_rule \
        M6_to_FLL_pg_strategy_via_rule \
        M11_to_M6_pg_strategy_via_rule \
        M11_to_M4_pg_strategy_via_rule \
        M11_to_rings_pg_strategy_via_rule
    }

    set_pg_strategy M4_to_SRAM_connection_pg_strategy \
        -pattern {{pattern: SRAM_conn_pattern} {nets: {VSS VDD}}} \
        -macros [get_cells -hierarchical -filter "design_type==macro && name=~*spm_tile*"] \
        -tag M4_SRAM_connection

    set_pg_strategy_via_rule M11_to_M4_SRAM_pg_strategy_via_rule -via_rule { \
        {{{existing: strap             } {layers: M11}} \
         {{strategies: M4_to_SRAM_connection_pg_strategy} {layers: M4}} \
         {via_master: {pg_via45_mrule pg_via56_mrule pg_via67_mrule pg_via78_mrule pg_via89_mrule VIA910_1cut_BW114 VIA1011_1cut}}} \
        {{intersection: undefined}{via_master: NIL}}}

    compile_pg -strategies M4_to_SRAM_connection_pg_strategy -via_rule M11_to_M4_SRAM_pg_strategy_via_rule

    # Trim the M3,M4 and M11 to clean PG mesh
    trim_pg_mesh -layers {M3 M4 M11}


    # Mesh on M12 to M15
    compile_pg -strategies { \
        M12_pg_strategy \
        M13_pg_strategy \
        M14_pg_strategy \
        M15_pg_strategy \
    } -via_rule { \
        mesh_to_ring_pg_strategy_via_rule \
        mesh_to_mesh_pg_strategy_via_rule
    }

    # Trim the the upper layers of the mesh
    trim_pg_mesh -layers {M12 M13 M14 M15}

    # Remove the routing blockage around the I/O pads
    remove_routing_blockage $pad2ring_RB_prefix*
    remove_routing_blockages RV_blockage*

}
