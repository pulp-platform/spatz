# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>


if {[sizeof_collection [get_bundles -quiet]] > 0} {
    remove_bundles [get_bundles -quiet]
}

if {[sizeof_collection [get_pin_constraints -quiet]] > 0} {
    remove_pin_constraints [get_pin_constraints -quiet]
}

set noc_pin_layers [get_layer {M5 M6 M7 M8}]
set floo_directions [list "north" "east" "south" "west"]
set floo_pin_structs [list "floo_req" "floo_rsp" "floo_wide"]

proc get_noc_pins_by_dir {cell dir {io ""}} {
    global floo_directions floo_pin_structs

    if {[string is alpha $cell]} {
        set cell [get_cells -hierarchical $cell]
    }

    set cell [get_object_name $cell]

    set io_suffixes {}
    if {$io == "input"} {
        lappend io_suffixes "_i"
    } elseif {$io == "output"} {
        lappend io_suffixes "_o"
    } else {
        lappend io_suffixes "_i" "_o"
    }
    set pins {}
    foreach pin_struct $floo_pin_structs {
        foreach io_suffix $io_suffixes {
            set pin_col [sort_collection [get_pins ${cell}/${pin_struct}${io_suffix}*] name]
            set num_pins_per_dir [expr [sizeof_collection $pin_col] / [llength $floo_directions]]
            set dir_index [lsearch $floo_directions $dir]
            set pins [add_to_collection $pins [index_collection $pin_col [expr $num_pins_per_dir * $dir_index] [expr $num_pins_per_dir * [expr $dir_index + 1] - 1]]]
        }
    }
    return $pins
}

# The indexes of the block sides in Fusion are different from the
# ones in FlooNoC
proc get_side_by_dir {dir} {
    set pd_directions [list "west" "north" "east" "south"]
    return [expr [lsearch -exact $pd_directions $dir] + 1]
}


###################
##  Net bundles  ##
###################

foreach block_ref {cluster_tile mem_tile} {
    set i 0
    foreach block_inst [get_block_inst $block_ref] {
        foreach dir $floo_directions {
            set bundle_pins [get_noc_pins_by_dir $block_inst $dir]
            set bundle_nets [all_connected $bundle_pins]
            if {[llength [get_block_inst $block_ref]] > 1} {
                set bundle_name ${block_ref}_${i}_${dir}
            } else {
                set bundle_name ${block_ref}_${dir}
            }
            create_bundle -name $bundle_name $bundle_nets
            puts "Created bundle $bundle_name with [sizeof_collection $bundle_nets] nets"
        }
        incr i
    }
}

create_bundle -name cheshire_tile_west [all_connected [get_pins -of_objects [get_block_inst cheshire_tile] -filter "name=~floo_*_west*"]]
create_bundle -name cheshire_tile_south [all_connected [get_pins -of_objects [get_block_inst cheshire_tile] -filter "name=~floo_*_south*"]]
create_bundle -name fhg_spu_tile_west [all_connected [get_pins -of_objects [get_block_inst fhg_spu_tile] -filter "name=~floo_*_west*"]]
create_bundle -name fhg_spu_tile_north [all_connected [get_pins -of_objects [get_block_inst fhg_spu_tile] -filter "name=~floo_*_north*"]]


###########################
##  General constraints  ##
###########################

# In general, don't want to place any pins lower than M5
set_block_pin_constraints -allowed_layers $noc_pin_layers -pin_spacing 1

############################
##  Cheshire Constraints  ##
############################

puts "Creating pin constraints for Cheshire tile"

create_pin_constraint \
    -type bundle -bundles [get_bundles cheshire_tile_west] \
    -cells [get_block_inst cheshire_tile] \
    -sides 1 -range {60% 95%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $noc_pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type bundle -bundles [get_bundles cheshire_tile_south] \
    -cells [get_block_inst cheshire_tile] \
    -sides 4 -range {50% 90%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $noc_pin_layers -interleaving_layer_range 1

###########################
##  FhG SPU Constraints  ##
###########################

puts "Creating pin constraints for FhG SPU tile"

create_pin_constraint \
    -type bundle -bundles [get_bundles fhg_spu_tile_west] \
    -cells [get_block_inst fhg_spu_tile] \
    -sides 1 \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $noc_pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type bundle -bundles [get_bundles fhg_spu_tile_north] \
    -cells [get_block_inst fhg_spu_tile] \
    -sides 2 -range {5% 40%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $noc_pin_layers -interleaving_layer_range 1

################################
##  Cluster Tile constraints  ##
################################

set i 0
foreach block_inst [get_block_inst cluster_tile] {

    puts "Creating pin constraints for cluster_tile_${i}"

    create_pin_constraint \
        -type bundle -bundles [get_bundles cluster_tile_${i}_west] \
        -cells [get_cells $block_inst] \
        -sides 1 -range {60% 95%} \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles cluster_tile_${i}_north] \
        -cells [get_cells $block_inst] \
        -sides 2 \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles cluster_tile_${i}_east] \
        -cells [get_cells $block_inst] \
        -sides 3 -range {5% 40%} \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles cluster_tile_${i}_south] \
        -cells [get_cells $block_inst] \
        -sides 4 \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

        incr i
}

###############################
##  Memory Tile constraints  ##
###############################

set i 0
foreach block_inst [get_block_inst mem_tile] {

    puts "Creating pin constraints for mem_tile_${i}"

    create_pin_constraint \
        -type bundle -bundles [get_bundles mem_tile_${i}_west] \
        -cells [get_cells $block_inst] \
        -sides 1 -range {60% 95%}\
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles mem_tile_${i}_north] \
        -cells [get_cells $block_inst] \
        -sides 2 -range {5% 95%} \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles mem_tile_${i}_east] \
        -cells [get_cells $block_inst] \
        -sides 3 -range {5% 40%} \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    create_pin_constraint \
        -type bundle -bundles [get_bundles mem_tile_${i}_south] \
        -cells [get_cells $block_inst] \
        -sides 4 -range {5% 95%} \
        -keep_pins_together true -pin_spacing_tracks 2 \
        -layer $noc_pin_layers -interleaving_layer_range 1

    incr i
}

##################
##  Clock pins  ##
##################

set mib_block_clock_pins ""

# The clock pins of MIB tiles, we need to place manually for CTP
foreach block_ref {cluster_tile mem_tile} {
    foreach block_inst [get_block_inst $block_ref] {
        lappend mib_block_clock_pins [get_object_name [get_pins -of_objects $block_inst -filter "name==clk_i"]]

    }
}

set mib_block_clock_pins [get_pins $mib_block_clock_pins]

# Place all the clock pins on the south side of the tile
create_pin_constraint -type individual \
    -layer {M7 M8} \
    -sides 4 \
    -pins $mib_block_clock_pins
