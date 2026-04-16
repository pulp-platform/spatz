# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set current_mode [current_mode]

foreach_in_collection mode [get_modes] {

  current_mode $mode

  set SYS_TCK [get_attribute [get_clocks soc_clk] period]
  set CLK_UNCERTAINTY [get_attribute [get_clocks soc_clk] hold_uncertainty]

  # There are additional asynchronous signals between the reset synchronizer
  # inside the JTAG DMI module that are not constrained specifically
  set_max_delay [expr $SYS_TCK * 0.5   ] -from [get_clocks jtag_clk] -to [get_clocks soc_clk] -ignore_clock_latency
  set_min_delay [expr -$CLK_UNCERTAINTY] -from [get_clocks jtag_clk] -to [get_clocks soc_clk] -ignore_clock_latency
  set_max_delay [expr $SYS_TCK * 0.5   ] -from [get_clocks soc_clk] -to [get_clocks jtag_clk] -ignore_clock_latency
  set_min_delay [expr -$CLK_UNCERTAINTY] -from [get_clocks soc_clk] -to [get_clocks jtag_clk] -ignore_clock_latency

}

current_mode $current_mode
