# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Remove previous I/O rings and guides
remove_io_rings -all
remove_io_guides -all
remove_power_io_constraints

# Get all I/O cells
proc get_io_cells {} {
  return [get_cells -hierarchical -quiet -filter {is_io || pad_cell}]
}

# Get all the I/O cells that don't exist in RTL and are added during I/O placement i.e. this script
proc get_special_io_cells {} {
  set special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*__added_*}]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*io_power*}]]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*io_gnd*}]]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*core_power*}]]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*core_gnd*}]]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*dummy*}]]
  set special_cells [add_to_collection $special_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && name=~*PRTEDISABLE_filler*}]]
  return $special_cells
}

# Remove any added cells and special nets
if {[sizeof_collection [get_special_io_cells]] > 0} {
  remove_cells [get_special_io_cells]
}
if {[sizeof_collection [get_nets -quiet __added_*]] > 0} {
  remove_nets [get_nets -quiet __added_*]
}

# Set the status of the I/O cells to unplaced
set_attribute -objects [get_cells -quiet -filter is_io==true -hier] -name status -value unplaced

# The core offset needs to include the I/O drivers, the connections to the I/O drivers, and the
# I/O driver itself + the keepout margin of 5um. Furthermore, in the vertical core offset we use the
# height of the corner I/O cell, which is slighlty larger.
set io2cell_spacing 5
set pad2boundary_spacing 0.3

# Use a multiple of the
# FinFet grid (0.03um)
proc finfet_ceil {value} {
  return [expr ceil($value / 0.03) * 0.03]
}

# Create the I/O ring
# TODO(fischeti): Check with Yichao about the horizontal offset of the I/O ring
create_io_ring -name io_ring -corner [get_attribute [get_lib_cells PCORNER] height] -offset [finfet_ceil $pad2boundary_spacing]
set_attribute [get_lib_cells tphn7_18gpio_univ/*_H] reference_orientation R90


# Change the reference  of two cells since they are generated wrongly by RTL

# (from _V to _H)
change_link [get_cells genblk7_0__genblk1_0__i_pad_slink_i] PDDWUWSWEWCDGS_V
# (from _H to _V)
change_link [get_cells genblk*_0__genblk*_7__i_pad_dram_slink_o] PDDWUWSWEWCDGS_V


# Definition of the power cells to use
set core_pwr_cell_name_T_B PVDD1CDGM_V
set core_pwr_cell_name_L_R PVDD1CDGM_H
set io_pwr_cell_name_T_B PVDD2CDGM_V
set io_pwr_cell_name_L_R PVDD2CDGM_H

set io_pwr_cell_power_on_control_T_B PVDD2POCM_V
set io_pwr_cell_power_on_control_L_R PVDD2POCM_H

# Dict uses name of power cell as key, {number, type} as value
set io_power_dict [dict create]

# Left side
dict set io_power_dict left_io_power [list 12 $io_pwr_cell_name_L_R]
dict set io_power_dict left_io_gnd [list 4 $io_pwr_cell_name_L_R]
dict set io_power_dict left_core_power [list 12 $core_pwr_cell_name_L_R]
dict set io_power_dict left_core_gnd [list 12 $core_pwr_cell_name_L_R]
dict set io_power_dict left_dummy_core [list 10 $core_pwr_cell_name_L_R]
dict set io_power_dict left_dummy_io [list 9 $io_pwr_cell_name_L_R]

# Top side
dict set io_power_dict top_io_power [list 8 $io_pwr_cell_name_T_B]
dict set io_power_dict top_io_gnd [list 20 $io_pwr_cell_name_T_B]
dict set io_power_dict top_core_power [list 16 $core_pwr_cell_name_T_B]
dict set io_power_dict top_core_gnd [list 16 $core_pwr_cell_name_T_B]
dict set io_power_dict top_dummy_core [list 10 $core_pwr_cell_name_T_B]
dict set io_power_dict top_dummy_io [list 10 $io_pwr_cell_name_T_B]

# Right side
dict set io_power_dict right_io_power [list 12 $io_pwr_cell_name_L_R]
dict set io_power_dict right_io_gnd [list 4 $io_pwr_cell_name_L_R]
dict set io_power_dict right_core_power [list 12 $core_pwr_cell_name_L_R]
dict set io_power_dict right_core_gnd [list 12 $core_pwr_cell_name_L_R]
dict set io_power_dict right_dummy_core [list 10 $core_pwr_cell_name_L_R]
dict set io_power_dict right_dummy_io [list 8 $io_pwr_cell_name_L_R]

# Bottom side
dict set io_power_dict bottom_io_power [list 8 $io_pwr_cell_name_T_B]
dict set io_power_dict bottom_io_gnd [list 20 $io_pwr_cell_name_T_B]
dict set io_power_dict bottom_core_power [list 16 $core_pwr_cell_name_T_B]
dict set io_power_dict bottom_core_gnd [list 16 $core_pwr_cell_name_T_B]
dict set io_power_dict bottom_dummy_core [list 10 $core_pwr_cell_name_T_B]
dict set io_power_dict bottom_dummy_io [list 9 $io_pwr_cell_name_T_B]

# Create all the I/O power cells
dict for {name type} $io_power_dict {
  lassign $type count lib_cell
  for {set i 0} {$i < $count} {incr i} {
    create_cell ${name}_${i} $lib_cell
  }
}

# Replace 1 PVDD2CDGM_V with PVDD2POCM_V as per the manual
change_link [get_cells top_io_gnd_0] $io_pwr_cell_power_on_control_T_B

# Add to filler cells manually around the PRTEDISABLE cell
create_cell {i_PRTEDISABLE_filler_0 i_PRTEDISABLE_filler_1} [get_lib_cells tphn7_18gpio_univ/PFILLER006300]

# the file below contains the position of every IO cell
set_signal_io_constraints -file $DPDIR/picobello.io

# Place the I/O pads
place_io -skip_bump_assignment

# Create corner cells
create_io_corner_cell -reference_cell PCORNER {io_ring.left io_ring.top}
create_io_corner_cell -reference_cell PCORNER {io_ring.bottom io_ring.left}
create_io_corner_cell -reference_cell PCORNER {io_ring.right io_ring.bottom}
create_io_corner_cell -reference_cell PCORNER {io_ring.top io_ring.right}

# Corner cells are not allowed to be rotated!
set_attribute [get_cells *corner_cell_1] orientation MX
set_attribute [get_cells *corner_cell_3] orientation MY

# Insert IO filler cells
create_io_filler_cells -io_guides [get_io_guides {*bottom *top}] -reference_cell {PFILLERSTRAP_V PFILLER000005 PFILLER000300 PFILLER006300}
create_io_filler_cells -io_guides [get_io_guides {*left *right}] -reference_cell {PFILLERSTRAP_H PFILLER000005 PFILLER000300 PFILLER006300}

# The previous UPF file that resulted from the `split_constraints` command
# is not yet aware of the new I/O cells. We need to create a new UPF file
# that also connects the new I/O cells to the supply nets. If we don't do that
# Fusion will connect the I/O cells to the primary supply nets (VDD/VSS) by default,
# everytime `connect_pg_net -automatic` is called.

# This script is run in both design-planning and top-level PnR. We differentiate
# between the two cases by checking if the handoff version is set.
if {[get_attribute [current_design] -quiet handoff_version] != ""} {
  set SPLIT_DIR $HANDOFF_DIR/latest/$DESIGN/scripts
  reset_upf
  load_upf $FCDIR/../constraints/${DESIGN}.upf
  # We need to reconnect the blocks, since the hierarchy has changed?
  connect_supply_net VDD -ports [get_pins -of_objects [get_all_block_inst] -filter name==VDD]
  connect_supply_net VSS -ports [get_pins -of_objects [get_all_block_inst] -filter name==VSS]
  connect_supply_net VDDIO -ports [get_pins -of_objects [get_all_block_inst] -filter name==VDDIO]
  connect_supply_net VSSIO -ports [get_pins -of_objects [get_all_block_inst] -filter name==VSSIO]
  commit_upf
# During the design-planning phase, we source the UPF file from the split directory
} else {
  set SPLIT_DIR $DPDIR/split/$DESIGN
  # The first time we run this script during design-planning, we backup the original UPF file
  # From the `split_constraints` command
  if {![file exists $SPLIT_DIR/top.original.upf]} {
    file copy $SPLIT_DIR/top.upf $SPLIT_DIR/top.original.upf
  }
  # Additionally, we also connect the newly created I/O cells to the supply nets
  connect_supply_net VDDIO -ports [get_pins -of_objects [get_special_io_cells] -filter name==VDDPST]
  connect_supply_net VSSIO -ports [get_pins -of_objects [get_special_io_cells] -filter name==VSSPST]
  # Then, we save the UPF file in the split directory again and commit it
  if {[get_attribute [current_design] -quiet handoff_version] == ""} {
    save_upf $SPLIT_DIR/top.upf
  }
  commit_upf
}

# We reapply the operating conditions for each corner,
# otherwise we get PVT mismatches later on in the flow
set current_corner [current_corner]
foreach_in_collection corner [get_corners] {
  current_corner $corner
  source $SPLIT_DIR/corner_[get_object_name $corner].tcl
}
current_corner $current_corner

# And now we do the actual logic connections
connect_pg_net -automatic -verbose

# Additionally, we (re-)connect the existing `rte` signal to all the I/O cells
disconnect_net -net rte -all
connect_net -net rte [get_pins -of_objects [get_io_cells] -filter name==RTE]
# and we lock it, so that Fusion does not try to route it
set_attribute -objects [get_nets rte] -name physical_status -value locked
set_dont_touch [get_nets rte]

# Fix the IO Cells
set_attribute -objects [get_io_cells] -name status -value fixed

# Check the I/O placement
check_io_placement
