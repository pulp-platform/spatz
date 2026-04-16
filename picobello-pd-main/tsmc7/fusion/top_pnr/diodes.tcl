# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
# Riccardo Fiorani Gallota <riccardo.fiorani3@unibo.it>

# The command for inserting diodes with Fusion Compiler is the following:
# create_diodes -options { \
#     {pin cell antenna_lib_cells num_diodes max_layer max_distance} \
# }

# This proc reads in a calibre antenna DRC report and extracts the locations of each antenna for each block
# Which is stored in `block_pnr/$tile/antenna_locations.txt`
proc extract_calibre_antennas {} {
    global FCDIR

    # This open the DRC error result file in Fusion. Edit the path where the file is located.
    read_drc_error_file -drc_type calibre -error_data picobello_chip.antenna.calibre.err \
        -file $FCDIR/../calibre/drc/picobello_chip.antenna-A.R.6__A.R.6.1__A.R.8.drc.results

    set error_db [get_drc_error_data picobello_chip.antenna.calibre.err]
    set top_antenna_violations [get_drc_errors -error_data $error_db]

    foreach tile {cluster_tile cheshire_tile mem_tile fhg_spu_tile} {
        sh rm -f $FCDIR/block_pnr/$tile/antenna_locations.txt
        foreach inst [get_block_inst $tile] {
            set antenna_violations [get_drc_errors -quiet -error_data $error_db -touching [get_attribute [get_cells $inst] boundary_bounding_box]]

            set top_antenna_violations [remove_from_collection $top_antenna_violations $antenna_violations]

            set inst_boundary_box [get_attribute [get_cells $inst] boundary_bounding_box]
            set ll_x [get_attribute $inst_boundary_box ll_x]
            set ll_y [get_attribute $inst_boundary_box ll_y]
            set ur_x [get_attribute $inst_boundary_box ur_x]
            set ur_y [get_attribute $inst_boundary_box ur_y]

            foreach_in_collection antenna_violation $antenna_violations {
                set bounding_box [get_attribute $antenna_violation bounding_box]
                set ant_ll_x [get_attribute $bounding_box ll_x]
                set ant_ll_y [get_attribute $bounding_box ll_y]
                set ant_ur_x [get_attribute $bounding_box ur_x]
                set ant_ur_y [get_attribute $bounding_box ur_y]
                sh echo "\{\{[expr {$ant_ll_x - $ll_x}] [expr {$ant_ll_y - $ll_y}]\} \{[expr {$ant_ur_x - $ll_x}] [expr {$ant_ur_y - $ll_y}]\}\}" >> $FCDIR/block_pnr/$tile/antenna_locations.txt
            }
        }
    }

    sh rm -f $FCDIR/top_pnr/antenna_locations.txt
    foreach_in_collection antenna_violation $top_antenna_violations {
        set bounding_box [get_attribute $antenna_violation bounding_box]
        set ant_ll_x [get_attribute $bounding_box ll_x]
        set ant_ll_y [get_attribute $bounding_box ll_y]
        set ant_ur_x [get_attribute $bounding_box ur_x]
        set ant_ur_y [get_attribute $bounding_box ur_y]
        sh echo "\{\{$ant_ll_x $ant_ll_y\} \{$ant_ur_x $ant_ur_y\}\}" >> $FCDIR/top_pnr/antenna_locations.txt
    }
}

# This proc reads the `antenna_locations.txt` for the current block and returns all pins that need antenna diodes
proc get_antenna_pins {} {
    global FCDIR
    set DESIGN [get_attribute [current_design] name]

    set antenna_pins ""
    if {$DESIGN == "picobello_chip"} {
        set antenna_locations [open $FCDIR/top_pnr/antenna_locations.txt r]
    } else {
        set antenna_locations [open $FCDIR/block_pnr/$DESIGN/antenna_locations.txt r]
    }

    set lines [split [read $antenna_locations] "\n"]
    close $antenna_locations

    foreach line $lines {
        if {[string trim $line] eq ""} {
            continue
        }
        set antenna_cells [get_cells -intersect $line]
        if {[sizeof_collection $antenna_cells] > 0} {
            set antenna_sel_pins [get_pins -of_objects $antenna_cells -filter "pin_direction==in&&port_type==signal"]
            foreach_in_collection antenna_pin $antenna_sel_pins {
                 if {$DESIGN == "picobello_chip"} {
                    set antenna_pins [add_to_collection -unique $antenna_pins $antenna_pin]
                } else {
                    set antenna_port [get_ports -quiet [get_attribute [get_flat_nets -of_objects $antenna_pin] name]]
                    if {[sizeof_collection $antenna_port] > 0} {
                        set antenna_pins [add_to_collection -unique $antenna_pins $antenna_pin]
                    }
                }
            }
        }
    }
    return $antenna_pins
}

# This proc finally adds the diodes for the pins that have antenna violations
proc add_antenna_diodes {} {
    set option_list {}
    foreach_in_collection antenna_pin [get_antenna_pins] {
        lappend option_list [list [get_attribute $antenna_pin name] [get_attribute [get_attribute $antenna_pin cell] full_name] ANTENNAPADBWP240H11P57PDSVT 2 M4 5]
    }

    create_diodes -options $option_list
}
