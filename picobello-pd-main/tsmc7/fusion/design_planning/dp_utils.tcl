# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# All the RTL modules that become subblocks
set block_refs [list cluster_tile mem_tile cheshire_tile fhg_spu_tile]

# Returns all the instances of the blocks
proc get_all_block_insts {} {
    global block_refs
    set block_insts {}
    foreach tile_ref $block_refs {
        set block_cells [get_object_name [get_cells -hier -filter ref_name==$tile_ref]]
        set block_insts [concat $block_insts $block_cells]
    }
    return $block_insts
}

# Get a specific block instance by ref name (and index)
proc get_block_inst {block_ref {index ""}} {
    global block_refs
    set block_insts [sort_collection [get_cells -hier -filter ref_name==$block_ref] name]

    if {$index != ""} {
        return [index_collection $block_insts $index]
    }
    return [get_object_name $block_insts]
}

# Returns all the child blocks of the blocks
proc get_child_blocks {} {
    global block_refs
    set child_blocks {}
    foreach block $block_refs {
        lappend child_blocks [get_object_name [get_blocks -hier -filter "block_name==$block && view_name!=frame"]]
    }
    return [get_blocks $child_blocks]
}

# Returns a collection of all hard macros i.e. SRAMs
proc get_hard_macros {} {
    return [get_cells -hierarchical -filter "is_hard_macro==true" -quiet]
}

# Removes the pins from the subblocks
proc remove_block_pins {{status "unrestricted"}} {
    set block_pins [get_terminals -of_objects [get_cells [get_all_block_insts]] -filter "physical_status==$status"]
    remove_terminals $block_pins
}

# Check that the dimensions of the blocks are multiple of the grid size
proc check_block_shapes {} {

    set grid_x_step [get_attribute [get_grids block_grid] x_step]
    set grid_y_step [get_attribute [get_grids block_grid] y_step]

    foreach_in_collection design [get_designs -hierarchical] {
        set fp_box [get_attribute $design boundary_bbox]
        set fp_x0 [lindex [lindex $fp_box 0] 0]
        set fp_y0 [lindex [lindex $fp_box 0] 1]
        set fp_x1 [lindex [lindex $fp_box 1] 0]
        set fp_y1 [lindex [lindex $fp_box 1] 1]
        set fp_dx [expr $fp_x1 - $fp_x0]
        set fp_dy [expr $fp_y1 - $fp_y0]

        set design_name [get_attribute $design name]
        puts "Checking design $design_name with dimensions $fp_dx x $fp_dy"

        if {[expr int($fp_dx*1000) % int($grid_x_step)] != 0} {
            warn "FP x dimension of $design_name is not aligned to the grid."
        }
        if {[expr int($fp_dy*1000) % int($grid_y_step)] != 0} {
            warn "FP y dimension of $design_name is not aligned to the grid."
        }
    }
}

# Create new handoff directory for each block
proc create_handoff {version {link_as_latest true}} {
    global OUTDIR HANDOFF_DIR
    global block_refs

    if {[file exists $HANDOFF_DIR/v${version}]} {
        warn "Handoff directory $HANDOFF_DIR/v${version} already exists. Skipping creation."
        return
    }

    # Create the handoff and deliverables directory and point `latest` to it
    log "Handoff" "Creating handoff directories and symlinks for version $version."
    file mkdir $HANDOFF_DIR/v${version}
    if {$link_as_latest} {
        log "Handoff" "Linking latest handoff and deliverables directories to version $version."
        file delete $HANDOFF_DIR/latest
        file link $HANDOFF_DIR/latest $HANDOFF_DIR/v${version}
    }

    foreach block_ref [concat picobello_chip $block_refs] {

        log "Handoff" "Creating handoff directories for block $block_ref."

        # Copy all the files for each block
        file copy $OUTDIR/$block_ref $HANDOFF_DIR/v${version}/$block_ref
    }
}

proc set_block_pins_status {block_ref status} {
    set_working_design -push [get_block_inst $block_ref 0]
    set port_list [get_ports -quiet -filter "port_type!=power && port_type!=ground"]
    if {[sizeof_collection $port_list] > 0} {
        set_attribute $port_list physical_status $status
        puts "Set pin status of $block_ref to $status for [sizeof_collection $port_list] pins."
    }
    set_working_design -pop -level 0
}
