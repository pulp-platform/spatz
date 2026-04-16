# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
#
# Set the purpose of the standard cells.

###############
## CTS Cells ##
###############

# TODO: Maybe restrict to even fewer cells? (e.g. only clk inv cells)
set_lib_cell_purpose -exclude cts [get_lib_cells */*]
set_lib_cell_purpose -include cts [get_lib_cells [list */*CK*]]

#################
## Hold Fixing ##
#################

# Use only delay cells for hold fixing
set_lib_cell_purpose -exclude hold [get_lib_cells */*]
set_lib_cell_purpose -include hold [get_lib_cells */DEL*]

################
## LVT groups ##
################

# Define LVT groups
set_attribute -quiet [get_lib_cells -quiet *_ulvt*/*]  threshold_voltage_group ulVt
set_attribute -quiet [get_lib_cells -quiet *_lvt*/*]  threshold_voltage_group lVt
set_attribute -quiet [get_lib_cells -quiet *_svt*/*] threshold_voltage_group sVt

# TODO(fischeti): Is this the correct way to differentiate? Naming is different in technology
set_threshold_voltage_group_type -type low_vt ulVt
set_threshold_voltage_group_type -type normal_vt lVt
set_threshold_voltage_group_type -type high_vt sVt
