# Copyright (c) 2025 ETH Zurich.
# Albi Mema <almema@iis.ee.ethz.ch>

set bump_mapping_file "$FCDIR/top_pnr/picobello_connections.csv"
set fp [open $bump_mapping_file r]
set power_signals { "VDD" "VSS" "VDDIO" "VSSIO" }

if {[sizeof_collection [get_matching_types -quiet *]] > 0} {
    remove_matching_types -all
}

set bump_dict [dict create]
set bump_dict_inv [dict create]

# Create a dictionary to map signals to bumps
while {[gets $fp line] >= 0} {
    # Split the columns in the CSV line
    lassign [split $line ","] bump_name signal_name
    if {$signal_name == ""} {
        puts "Bump $bump_name is unassigned, skipping..."
        continue
    }
    dict lappend bump_dict $signal_name $bump_name
    dict lappend bump_dict_inv $bump_name $signal_name
}
close $fp

# Create matching types for each signal
foreach signal [dict keys $bump_dict] {

    set bumps [dict get $bump_dict $signal]

    if {[lsearch -exact $power_signals $signal] != -1} {
        # For power signals, disconnect and reconnect bumps to the signal net
        disconnect_net [get_pins -of_objects $bumps]
        connect_net -net $signal [get_pins -of_objects $bumps]
    } else {
        set num_bumps [llength $bumps]
        create_matching_type -name ${signal}_mt [list [get_pins -of_objects [get_nets $signal]] [get_cells $bumps]] -uniquify -$num_bumps
        log bumps "Created matching type for signal $signal with bumps: $bumps"
    }
}

# Top VDD bumps
create_matching_type -name VDD_top_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_0 top_core_power_1}] -filter name==VDD] \
          [get_cells bump_array_16_18]]
create_matching_type -name VDD_top_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_2 top_core_power_3}] -filter name==VDD] \
          [get_cells bump_array_14_18]]
create_matching_type -name VDD_top_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_4 top_core_power_5}] -filter name==VDD] \
          [get_cells bump_array_15_19]]
create_matching_type -name VDD_top_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_6 top_core_power_9}] -filter name==VDD] \
          [get_cells bump_array_14_20]]
create_matching_type -name VDD_top_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_7 top_core_power_8}] -filter name==VDD] \
          [get_cells bump_array_16_20]]
create_matching_type -name VDD_top_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_10 top_core_power_11}] -filter name==VDD] \
          [get_cells bump_array_15_21]]
create_matching_type -name VDD_top_6 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_12 top_core_power_13}] -filter name==VDD] \
          [get_cells bump_array_14_22]]
create_matching_type -name VDD_top_7 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_power_14 top_core_power_15}] -filter name==VDD] \
          [get_cells bump_array_16_22]]

# Top VSS bumps
create_matching_type -name VSS_top_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_0 top_core_gnd_1}] -filter name==VSS] \
          [get_cells bump_array_14_14]]
create_matching_type -name VSS_top_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_2 top_core_gnd_3}] -filter name==VSS] \
          [get_cells bump_array_14_16]]
create_matching_type -name VSS_top_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_4 top_core_gnd_5}] -filter name==VSS] \
          [get_cells bump_array_16_16]]
create_matching_type -name VSS_top_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_6 top_core_gnd_7}] -filter name==VSS] \
          [get_cells bump_array_15_17]]
create_matching_type -name VSS_top_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_8 top_core_gnd_9}] -filter name==VSS] \
          [get_cells bump_array_15_23]]
create_matching_type -name VSS_top_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_10 top_core_gnd_11}] -filter name==VSS] \
          [get_cells bump_array_16_24]]
create_matching_type -name VSS_top_6 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_12 top_core_gnd_13}] -filter name==VSS] \
          [get_cells bump_array_14_24]]
create_matching_type -name VSS_top_7 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {top_core_gnd_14 top_core_gnd_15}] -filter name==VSS] \
          [get_cells bump_array_14_26]]

# Bottom VDD bumps
create_matching_type -name VDD_bottom_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_0 bottom_core_power_1}] -filter name==VDD] \
          [get_cells bump_array_0_22]]
create_matching_type -name VDD_bottom_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_2 bottom_core_power_3}] -filter name==VDD] \
          [get_cells bump_array_2_22]]
create_matching_type -name VDD_bottom_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_4 bottom_core_power_5}] -filter name==VDD] \
          [get_cells bump_array_1_21]]
create_matching_type -name VDD_bottom_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_6 bottom_core_power_9}] -filter name==VDD] \
          [get_cells bump_array_2_20]]
create_matching_type -name VDD_bottom_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_7 bottom_core_power_8}] -filter name==VDD] \
          [get_cells bump_array_0_20]]
create_matching_type -name VDD_bottom_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_10 bottom_core_power_11}] -filter name==VDD] \
          [get_cells bump_array_1_19]]
create_matching_type -name VDD_bottom_6 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_12 bottom_core_power_13}] -filter name==VDD] \
          [get_cells bump_array_2_18]]
create_matching_type -name VDD_bottom_7 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_power_14 bottom_core_power_15}] -filter name==VDD] \
          [get_cells bump_array_0_18]]

# Bottom VSS bumps
create_matching_type -name VSS_bottom_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_0 bottom_core_gnd_1}] -filter name==VSS] \
          [get_cells bump_array_2_26]]
create_matching_type -name VSS_bottom_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_2 bottom_core_gnd_3}] -filter name==VSS] \
          [get_cells bump_array_2_24]]
create_matching_type -name VSS_bottom_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_4 bottom_core_gnd_5}] -filter name==VSS] \
          [get_cells bump_array_0_24]]
create_matching_type -name VSS_bottom_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_6 bottom_core_gnd_7}] -filter name==VSS] \
          [get_cells bump_array_1_23]]
create_matching_type -name VSS_bottom_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_8 bottom_core_gnd_9}] -filter name==VSS] \
          [get_cells bump_array_1_17]]
create_matching_type -name VSS_bottom_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_10 bottom_core_gnd_11}] -filter name==VSS] \
          [get_cells bump_array_0_16]]
create_matching_type -name VSS_bottom_6 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_12 bottom_core_gnd_13}] -filter name==VSS] \
          [get_cells bump_array_2_16]]
create_matching_type -name VSS_bottom_7 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {bottom_core_gnd_14 bottom_core_gnd_15}] -filter name==VSS] \
          [get_cells bump_array_2_14]]

# Left VDD bumps
create_matching_type -name VDD_left_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_0 left_core_power_1}] -filter name==VDD] \
          [get_cells bump_array_7_7]]
create_matching_type -name VDD_left_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_2 left_core_power_3}] -filter name==VDD] \
          [get_cells bump_array_8_6]]
create_matching_type -name VDD_left_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_4 left_core_power_5}] -filter name==VDD] \
          [get_cells bump_array_8_2]]
create_matching_type -name VDD_left_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_6 left_core_power_7}] -filter name==VDD] \
          [get_cells bump_array_8_0]]
create_matching_type -name VDD_left_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_8 left_core_power_9}] -filter name==VDD] \
          [get_cells bump_array_8_4]]
create_matching_type -name VDD_left_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_power_10 left_core_power_11}] -filter name==VDD] \
          [get_cells bump_array_9_7]]

# Left VSS bumps
create_matching_type -name VSS_left_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_0 left_core_gnd_1}] -filter name==VSS] \
          [get_cells bump_array_6_6]]
create_matching_type -name VSS_left_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_2 left_core_gnd_3}] -filter name==VSS] \
          [get_cells bump_array_6_10]]
create_matching_type -name VSS_left_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_4 left_core_gnd_5}] -filter name==VSS] \
          [get_cells bump_array_7_1]]
create_matching_type -name VSS_left_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_6 left_core_gnd_7}] -filter name==VSS] \
          [get_cells bump_array_9_1]]
create_matching_type -name VSS_left_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_8 left_core_gnd_9}] -filter name==VSS] \
          [get_cells bump_array_10_10]]
create_matching_type -name VSS_left_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {left_core_gnd_10 left_core_gnd_11}] -filter name==VSS] \
          [get_cells bump_array_10_6]]

# Right VDD bumps
create_matching_type -name VDD_right_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_0 right_core_power_1}] -filter name==VDD] \
          [get_cells bump_array_9_33]]
create_matching_type -name VDD_right_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_2 right_core_power_3}] -filter name==VDD] \
          [get_cells bump_array_8_34]]
create_matching_type -name VDD_right_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_4 right_core_power_5}] -filter name==VDD] \
          [get_cells bump_array_8_38]]
create_matching_type -name VDD_right_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_6 right_core_power_7}] -filter name==VDD] \
          [get_cells bump_array_8_40]]
create_matching_type -name VDD_right_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_8 right_core_power_9}] -filter name==VDD] \
          [get_cells bump_array_8_36]]
create_matching_type -name VDD_right_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_power_10 right_core_power_11}] -filter name==VDD] \
          [get_cells bump_array_7_33]]

# Right VSS bumps
create_matching_type -name VSS_right_0 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_0 right_core_gnd_1}] -filter name==VSS] \
          [get_cells bump_array_10_34]]
create_matching_type -name VSS_right_1 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_2 right_core_gnd_3}] -filter name==VSS] \
          [get_cells bump_array_10_30]]
create_matching_type -name VSS_right_2 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_4 right_core_gnd_5}] -filter name==VSS] \
          [get_cells bump_array_9_39]]
create_matching_type -name VSS_right_3 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_6 right_core_gnd_7}] -filter name==VSS] \
          [get_cells bump_array_7_39]]
create_matching_type -name VSS_right_4 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_8 right_core_gnd_9}] -filter name==VSS] \
          [get_cells bump_array_6_30]]
create_matching_type -name VSS_right_5 -uniquify 2 \
    [list [get_pins -of_objects [get_cells {right_core_gnd_10 right_core_gnd_11}] -filter name==VSS] \
          [get_cells bump_array_6_34]]


# VDDIO/VSSIO bumps are all connected twice
set manual_vssio_bumps [get_cells {bump_array_10_4 bump_array_6_4 bump_array_10_36 bump_array_6_36}]
set manual_vssio_cells [get_cells {left_io_gnd* right_io_gnd*}]
create_matching_type -name VSSIO_manual_mt -uniquify 2 \
    [list [get_pins -of_objects $manual_vssio_cells -filter name==VSSPST] $manual_vssio_bumps] \

set manual_vddio_bumps [get_cells {bump_array_13_1 bump_array_3_1 bump_array_3_39 bump_array_13_39}]
set manual_vddio_cells [get_cells {left_io_power_0 left_io_power_1 left_io_power_10 left_io_power_11 right_io_power_0 right_io_power_1 right_io_power_10 right_io_power_11}]
create_matching_type -name VDDIO_manual1_mt -uniquify 2 \
    [list [get_pins -of_objects $manual_vddio_cells -filter name==VDDPST] $manual_vddio_bumps] \

set vddio_bumps [remove_from_collection [get_cells [dict get $bump_dict VDDIO]] $manual_vddio_bumps]
set vddio_cells [remove_from_collection [get_cells *io_power*] $manual_vddio_cells]
create_matching_type -name VDDIO_mt -uniquify 2 \
    [list [get_pins -of_objects $vddio_cells -filter name==VDDPST] $vddio_bumps] \

set vssio_bumps [remove_from_collection [get_cells [dict get $bump_dict VSSIO]] $manual_vssio_bumps]
set vssio_cells [remove_from_collection [get_cells *io_gnd*] $manual_vssio_cells]
create_matching_type -name VSSIO_mt -uniquify 2 \
    [list [get_pins -of_objects $vssio_cells -filter name==VSSPST] \
    [get_cells $vssio_bumps]] \
