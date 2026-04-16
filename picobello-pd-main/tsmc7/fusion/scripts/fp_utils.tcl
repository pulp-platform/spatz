# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

###########################
##  Floorplan variables  ##
###########################

# Set the site_height and width
set site_height [get_attribute [get_site_defs unit] height]
set site_width  [get_attribute [get_site_defs unit] width]

# Set some variables for the dimension of the current core area
set fp_box [get_attribute [get_core_area] bbox]
set fp_x0 [lindex [lindex $fp_box 0] 0]
set fp_y0 [lindex [lindex $fp_box 0] 1]
set fp_x1 [lindex [lindex $fp_box 1] 0]
set fp_y1 [lindex [lindex $fp_box 1] 1]
set fp_dx [expr $fp_x1 - $fp_x0]
set fp_dy [expr $fp_y1 - $fp_y0]

# According to SRAM documentation (Section 2.1.19)
# this is true for vertically/horizontally and to other SRAMs/STD cells
set macro_min_keepout_margin 0.912
set macro_routing_blockage 0.38
# The actual keepout margin are rounded up to the nearest site width/height
# to make sure that the SRAMs are aligned with the site rows (and finFET grid?)
set macro_hor_keepout_margin [expr {ceil($macro_min_keepout_margin / (2 * $site_width)) * 2 * $site_width}]
set macro_ver_keepout_margin [expr {ceil($macro_min_keepout_margin / $site_height) * $site_height}]


# PG pad2ring temporary routing blockage prefix
set pad2ring_RB_prefix temp_RB_pad2ring_

##################################
##  Floorplan helper functions  ##
##################################

proc grid_ceil {value} {
    set grid_step [get_attribute [get_grids block_grid] x_step]
    return [expr {ceil($value / $grid_step) * $grid_step}]
}

proc get_memory_macros {macro_name} {
    set macros [get_cells -physical_context -filter {design_type =~ macro} $macro_name]
    set macros [sort_collection -dictionary $macros {full_name}]
    set macros [collection_to_list -name_only -no_braces -newline -no_sort $macros]
    return $macros
}

# Creates a routing keepout blockage around a cell
# - cells: The cells to create the keepout around
# - halo: The halo around the cell, can be a single value or a list of four values for left, bottom, right and top
# - layer: The layer(s) to create the routing keepout on, can be a single layer or a list of layers
proc add_routing_keepout_margin {cells halo layers} {
    if {[llength $halo] == 1} {
        set halo [list $halo $halo $halo $halo]
    }

    set left_halo [lindex $halo 0]
    set bottom_halo [lindex $halo 1]
    set right_halo [lindex $halo 2]
    set top_halo [lindex $halo 3]

    foreach_in_collection cell $cells {
        set llx [lindex [get_attribute $cell boundary_bbox] 0 0]
        set lly [lindex [get_attribute $cell boundary_bbox] 0 1]
        set urx [lindex [get_attribute $cell boundary_bbox] 1 0]
        set ury [lindex [get_attribute $cell boundary_bbox] 1 1]

        puts "layer=$layers top_halo=$top_halo, bottom_halo=$bottom_halo, left_halo=$left_halo, right_halo=$right_halo"

        if {$top_halo > 0} {
            create_routing_blockage -zero_spacing -net_types signal -layers $layers -boundary [list [list [expr $llx - $left_halo] $ury] [list [expr $urx + $right_halo] [expr $ury + $top_halo]]]
        }
        if {$bottom_halo > 0} {
            create_routing_blockage -zero_spacing -net_types signal -layers $layers -boundary [list [list [expr $llx - $left_halo] [expr $lly - $bottom_halo]] [list [expr $urx + $right_halo] $lly]]
        }
        if {$left_halo > 0} {
            create_routing_blockage -zero_spacing -net_types signal -layers $layers -boundary [list [list [expr $llx - $left_halo] [expr $lly - $bottom_halo]] [list $llx [expr $ury + $top_halo]]]
        }
        if {$right_halo > 0} {
            create_routing_blockage -zero_spacing -net_types signal -layers $layers -boundary [list [list $urx [expr $lly - $bottom_halo]] [list [expr $urx + $right_halo] [expr $ury + $top_halo]]]
        }
    }
}

# Creates a signal routing blockage inside a cell
# - cells: The cells to create the blobkage
# - layer: The layer(s) to create the signal routing blockage on, can be a single layer or a list of layers
proc add_signal_blockage {cells layers} {
    foreach_in_collection cell $cells {
        create_routing_blockage -zero_spacing -net_types {signal clock reset scan} -layers $layers -boundary [get_attribute $cell boundary_bbox]
    }
}

# Create keepout margins around blocks
proc create_block_keepouts {cells} {
    global macro_min_keepout_margin macro_hor_keepout_margin macro_ver_keepout_margin site_height site_width
    # Use the same placement keepout margins as for the SRAMs
    create_keepout_margin -type hard -outer [list $macro_hor_keepout_margin $macro_ver_keepout_margin $macro_hor_keepout_margin $macro_ver_keepout_margin] $cells
    # Create the routing blockages around all the edges of the blocks on M0 for PG routing
    set keepout_hor [expr $macro_hor_keepout_margin + 7 * $site_width]
    set keepout_ver [expr $macro_ver_keepout_margin + $site_height / 2]
    create_keepout_margin -type routing_blockage -layer {M0 M1 M2 M3} -outer [list $keepout_hor $keepout_ver $keepout_hor $keepout_ver] $cells

    # We want to avoid M1/M2 routing over non-finfet area
    set rout_keepout_hor [list \
        [expr $macro_hor_keepout_margin + 10 * $site_width] \
        [expr $macro_ver_keepout_margin + $site_height] \
        [expr $macro_hor_keepout_margin + 10 * $site_width] \
        [expr $macro_ver_keepout_margin + $site_height]]

    add_routing_keepout_margin $cells $rout_keepout_hor M1
    add_routing_keepout_margin $cells $rout_keepout_hor M2
}

# Creates proper placement and PG routing blockages around macros
proc create_sram_keepouts {macros} {
    global macro_min_keepout_margin macro_hor_keepout_margin macro_ver_keepout_margin site_height site_width
    # Create the std cell SRAM keepout margins
    create_keepout_margin -type hard -outer [lrepeat 4 $macro_min_keepout_margin] $macros
    # Create the routing blockages around all the edges of the SRAM on M0 to M3 for the PG routing
    set keepout_hor [expr $macro_hor_keepout_margin + 7 * $site_width]
    set keepout_ver [expr $macro_ver_keepout_margin + $site_height / 2]
    create_keepout_margin -type routing_blockage -layer {M0 M1 M2 M3 M4} -outer [list $keepout_hor $keepout_ver $keepout_hor $keepout_ver] $macros
    # Create signal routing blockages around the SRAM on M0 to M3
    # The routing keepouts for the layers are taken from the SRAM documentation (see Figure 2.79)
    set m1_keepout $macro_min_keepout_margin
    # For M3, we use the keepout as defined in the SRAM documentation
    set m3_keepout $macro_min_keepout_margin

    foreach_in_collection macro $macros {
        # The pins are accessed over M2. Therefore, we disable the keepout on the side where the pins are located
        set m2_keepout [lrepeat 4 $macro_min_keepout_margin]
        if {[get_attribute $macro orientation] == "MY"} {
            lset m2_keepout 0 0
        } else {
            lset m2_keepout 2 0
        }

        puts "macro [get_object_name $macro] has orientation [get_attribute $macro orientation], m2_keepout=$m2_keepout"

        # Add the routing halos
        add_routing_keepout_margin $macro $m1_keepout M1
        add_routing_keepout_margin $macro $m2_keepout M2
        add_routing_keepout_margin $macro $m3_keepout M3
    }

    # Add the signal routing blockage inside
    add_signal_blockage $macros M3
}

# Removes the boundary cells again which were inserted with `compile_boundary_cells`
proc remove_boundary_cells {} {
    set boundary_cells [get_cells boundarycell* -quiet]
    if {[sizeof_collection $boundary_cells] > 0} {
        remove_cells $boundary_cells
    }
}

# Remove the tap cells again which were inserted with `create_tap_cells`
proc remove_tap_cells {} {
    set tap_cells [get_cells tapfiller* -quiet]
    if {[sizeof_collection $tap_cells] > 0} {
        remove_cells $tap_cells
    }
}

# Remove all PG shapes, vias and terminals
proc remove_pg {} {
    if {[sizeof_collection [get_shapes -quiet -filter "net_type==power || net_type==ground"]] > 0} {
        remove_shapes [get_shapes -quiet -filter "net_type==power || net_type==ground"]
    }
    if {[sizeof_collection [get_vias -quiet -filter "net_type==power || net_type==ground"]] > 0} {
        remove_vias [get_vias -quiet -filter "net_type==power || net_type==ground"]
    }
    if {[sizeof_collection [get_terminals -quiet {VDD* VSS*}]] > 0} {
        remove_terminals [get_terminals -quiet {VDD* VSS*}]
    }
}


proc create_routing_blockage_pad2ring {io_pad prefix layerlist} {

    # Create a routing blockage around the I/O pad to prevent routing on the M12 layer
    # The M12 layer is used for the I/O ring and the I/O pads, so we need to create a blockage
    # around the I/O pad to prevent routing on that layer

    set io_pad_reference [get_attribute $io_pad ref_name]
    set io_pad_orientation [get_attribute $io_pad orientation]
    set io_pad_bbox [get_attribute $io_pad bbox]

    set ring_pg_region_bbox [get_attribute [get_pg_regions ring_pg_region] bbox]

    if {[string match "*_V" $io_pad_reference]} {

        set left_x [lindex $io_pad_bbox 0 0]
        set right_x [lindex $io_pad_bbox 1 0]

        if {$io_pad_orientation == "R0"} {

            set top_y [lindex $io_pad_bbox 1 1]
            set bot_y [lindex $ring_pg_region_bbox 0 1]

        } elseif {$io_pad_orientation == "R180"} {

            set top_y [lindex $io_pad_bbox 0 1]
            set bot_y [lindex $ring_pg_region_bbox 1 1]

        } else {
            puts "Error: I/O pad $io_pad_reference has an unexpected orientation $io_pad_orientation"
            return
        }

        create_routing_blockage -layers $layerlist \
                                -boundary [list [list $left_x $bot_y] [list $right_x $top_y]] \
                                -name_prefix $prefix


    } elseif {[string match "*_H" $io_pad_reference]} {

        if {$io_pad_orientation == "R0"} {

            set left_x [lindex $ring_pg_region_bbox 1 0]
            set right_x [lindex $io_pad_bbox 0 0]

        } elseif {$io_pad_orientation == "R180"} {

            set left_x [lindex $ring_pg_region_bbox 0 0]
            set right_x [lindex $io_pad_bbox 1 0]

        } else {
            puts "Error: I/O pad $io_pad_reference has an unexpected orientation $io_pad_orientation"
            return
        }

        set top_y [lindex $io_pad_bbox 1 1]
        set bot_y [lindex $io_pad_bbox 0 1]

        create_routing_blockage -layers $layerlist \
                                -boundary [list [list $left_x $bot_y] [list $right_x $top_y]] \
                                -name_prefix $prefix


    } else {
        puts "Error: I/O pad $io_pad_reference does not match any expected pattern (*_V or *_H)"
        return
    }

}
