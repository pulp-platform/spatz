# Constrain I/O
set_input_delay  [expr 0.5 * $TCK] -clock virtual_clk [remove_from_collection [all_inputs] {clk_i rst_ni}]
set_output_delay [expr 0.5 * $TCK] -clock virtual_clk [all_outputs]

# Driving cells and loads.
set driving_cell     BUFFD8BWP240H11P57PDSVT
set driving_cell_clk CKBD8BWP240H11P57PDSVT
set load_cell        BUFFD8BWP240H11P57PDSVT

# set_max_fanout 8 [all_designs]
set_load -pin_load [expr 4*[load_of [get_lib_pins */$load_cell/I]]] [all_outputs]
set_driving_cell [all_inputs]      -lib_cell $driving_cell     -pin Z -no_design_rule
set_driving_cell [get_ports clk_i] -lib_cell $driving_cell_clk -pin Z -no_design_rule
