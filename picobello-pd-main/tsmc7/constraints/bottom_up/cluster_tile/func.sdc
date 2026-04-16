# Use 1GHz clock if not specified otherwise
if {![info exists FREQ]} {set FREQ 1.0}
set TCK [freq_to_period [expr $FREQ * 1e9]]

# Create the clock
create_clock -period $TCK -name clk [get_ports clk_i]

# Create a virtual clock for I/O delays
create_clock -period $TCK -name virtual_clk
set_latency_adjustment_options -reference_clock clk -clocks_to_update {virtual_clk} -ocv_included

# Mode specific constant signals
set_case_analysis 0 [get_ports test_enable_i]
set_case_analysis 1 [get_ports tile_clk_en_i]
set_case_analysis 0 [get_ports clk_rst_bypass_i]

# False-path some static signals
set_false_path -from [get_ports debug_req_i*]
set_false_path -from [get_ports {meip_i* mtip_i* msip_i*}]
set_false_path -from [get_ports {hart_base_id_i* cluster_base_addr_i* id_i*}]
