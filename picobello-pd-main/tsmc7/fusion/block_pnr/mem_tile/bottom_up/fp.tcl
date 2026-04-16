# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>


if {[sizeof_collection [get_bundles -quiet]] > 0} {
    remove_bundles [get_bundles -quiet]
}

if {[sizeof_collection [get_pin_constraints -quiet]] > 0} {
    remove_pin_constraints [get_pin_constraints -quiet]
}

#################
##  Floorplan  ##
#################

# The size of the mem tile as taped out in picobello
set width 679.44
set height 921.12

# Initialize floorplan
initialize_floorplan -side_length [list $width $height]

# Redo the tracks after initializing the floorplan
source $FCDIR/scripts/tracks.tcl

############
##  Pins  ##
############

remove_pin_constraints *

set pin_layers [get_layer {M5 M6 M7 M8}]
set floo_directions [list "north" "east" "south" "west"]
set floo_port_structs [list "floo_req" "floo_rsp" "floo_wide"]

proc get_noc_ports_by_dir {dir {io ""}} {
    global floo_directions floo_port_structs

    set io_suffixes {}
    if {$io == "input"} {
        lappend io_suffixes "_i"
    } elseif {$io == "output"} {
        lappend io_suffixes "_o"
    } else {
        lappend io_suffixes "_i" "_o"
    }
    set ports {}
    foreach port_struct $floo_port_structs {
        foreach io_suffix $io_suffixes {
            set port_col [sort_collection [get_ports ${port_struct}${io_suffix}*] name]
            set num_ports_per_dir [expr [sizeof_collection $port_col] / [llength $floo_directions]]
            set dir_index [lsearch $floo_directions $dir]
            set ports [add_to_collection $ports [index_collection $port_col [expr $num_ports_per_dir * $dir_index] [expr $num_ports_per_dir * [expr $dir_index + 1] - 1]]]
        }
    }
    return $ports
}

foreach dir $floo_directions {
    set bundle_pins [get_noc_ports_by_dir $dir]
    set bundle_nets [all_connected $bundle_pins]
    set bundle_name bundle_${dir}
    create_bundle -name $bundle_name $bundle_nets
    puts "Created bundle $bundle_name with [sizeof_collection $bundle_nets] nets"
}

if {[sizeof_collection [get_pin_constraints]] > 0} {
    remove_pin_constraints *
}

create_pin_constraint \
    -type bundle -bundles [get_bundles bundle_west] \
    -self \
    -sides 1 -range {30% 70%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type bundle -bundles [get_bundles bundle_north] \
    -sides 2 -range {30% 70%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type bundle -bundles [get_bundles bundle_east] \
    -sides 3 -range {30% 70%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type bundle -bundles [get_bundles bundle_south] \
    -sides 4 -range {30% 70%} \
    -keep_pins_together true -pin_spacing_tracks 2 \
    -layer $pin_layers -interleaving_layer_range 1

create_pin_constraint \
    -type individual -ports [remove_from_collection [get_ports] [get_ports {floo*}]] \
    -sides 1 -layers $pin_layers

place_pins -self


##############
##  Macros  ##
##############

# Simply source the original macro placement script
source $FCDIR/block_pnr/$DESIGN/place_macros.tcl
