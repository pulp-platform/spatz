# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

if {![info exists MODE]} {set MODE "func"}

#######################
##  Apply scenarios  ##
#######################

# Use slowest corner for setup
set setup_scenarios {}
foreach mode ${MODE} {
  lappend setup_scenarios {*}[list ${mode}_ssgnp_0p675v_m40c ${mode}_ssgnp_0p675v_125c]
}

# Use fastest corner for hold
set hold_scenarios [list func_ffgnp_0p825v_125c func_ssgnp_0p675v_m40c]

# Use hold corner for min capacitance
set min_capacitance_scenarios $hold_scenarios

# Use setup corners for max transition, max capacitance
set max_capacitance_scenarios $setup_scenarios
set max_transition_scenarios $setup_scenarios

# Use hottest corner for leakage
set leakage_scenarios {func_ssgnp_0p675v_125c}
# Use typical corner for dynamic power
set dynamic_power_scenarios {func_tt_0p75v_25c}

set_scenario_status -none                  [all_scenarios]
set_scenario_status -setup           true  $setup_scenarios
set_scenario_status -hold            true  $hold_scenarios
set_scenario_status -leakage_power   true  $leakage_scenarios
set_scenario_status -dynamic_power   true  $dynamic_power_scenarios
set_scenario_status -max_transition  true  $max_transition_scenarios
set_scenario_status -max_capacitance true  $max_capacitance_scenarios
set_scenario_status -min_capacitance true  $min_capacitance_scenarios
