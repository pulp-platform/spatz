# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Somehow Fusion does not want to insert tie cells automatically in the top-level.
set tie_low_cells [get_lib_cells TIEL*]
set tie_high_cells [get_lib_cells TIEH*]

# First round of tie cell insertion can be done by the `add_tie_cells` command,
# which at least inserts tie cells for standard cells and I/O pad pins.

proc get_tie_off_pins {} {
    set pins [get_pins -hierarchical \
                -filter [list \
                    (net.net_type == ground or net.net_type == power) and \
                    (port_type == signal or port_type == clock or port_type == reset or port_type == tie_high or port_type == tie_low) and \
                    (parent_block == [current_block])]]
         return $pins
}

add_tie_cells -objects [get_tie_off_pins] -tie_high_lib_cells $tie_high_cells -tie_low_lib_cells $tie_low_cells

legalize_placement -cells [get_cells -of_references [get_lib_cells {TIEL* TIEH*}]]

# Second round of tie cell insertion needs to be done manually
set tie_off_pins [get_tie_off_pins]

set tielo_pins [filter_collection $tie_off_pins "net==[get_nets VSS]"]
set tiehi_pins [filter_collection $tie_off_pins "net==[get_nets VDD]"]
set tielo_cells [regsub -all {[/\[\]]} [get_object_name $tielo_pins] "_"]
set tiehi_cells [regsub -all {[/\[\]]} [get_object_name $tiehi_pins] "_"]

create_cell $tielo_cells [get_lib_cells TIELXNBWP240H8P57PDSVT]
create_cell $tiehi_cells [get_lib_cells TIEHXPBWP240H8P57PDSVT]

set tielo_cell_pins [get_pins -of_objects $tielo_cells -filter "name==ZN"]
set tiehi_cell_pins [get_pins -of_objects $tiehi_cells -filter "name==Z"]

create_net_bus tielo_nets[[llength $tielo_cells]:1] -create_nets
create_net_bus tiehi_nets[[llength $tiehi_cells]:1] -create_nets

disconnect_net $tielo_pins
disconnect_net $tiehi_pins

connect_net -net tielo_nets $tielo_pins
connect_net -net tielo_nets $tielo_cell_pins
connect_net -net tiehi_nets $tiehi_pins
connect_net -net tiehi_nets $tiehi_cell_pins

# We disable timing driven routing for simple tie cell insertion
set route_detail_timing_driven [get_app_option_value -name route.detail.timing_driven]
set route_global_timing_driven [get_app_option_value -name route.global.timing_driven]
set_app_options -name route.detail.timing_driven -value false
set_app_options -name route.global.timing_driven -value false

place_eco_cells -cells [get_cells [concat $tielo_cells $tiehi_cells]] -legalize_mode free_site_only
route_eco -utilize_dangling_wires true -reroute modified_nets_first_then_others -open_net_driven true \
    -cells [get_cells [concat $tielo_cells $tiehi_cells]] \
    -nets [get_nets {tielo_nets* tiehi_nets*}]
set_fixed_objects [get_cells [concat $tielo_cells $tiehi_cells]]
set_fixed_objects [get_nets {tielo_nets* tiehi_nets*}]

set_app_options -name route.detail.timing_driven -value $route_detail_timing_driven
set_app_options -name route.global.timing_driven -value $route_global_timing_driven
