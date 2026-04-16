# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Driving cells and loads
set driving_cell     PDDWUWSWEWCDGS_H
set driving_cell_pin PAD
set max_load 15 ; # 15pF - higher end of typical parasitics on standard PCBs (incidentally also the test capacitance used by Cypress for characterizing the HyperBus interface)
set min_load 5  ; # 5pF - lower end of typical parasitics on standard PCBs

set_load -max -pin_load $max_load [all_outputs]
set_load -min -pin_load $min_load [all_outputs]
set_driving_cell [all_inputs] -lib_cell $driving_cell -pin $driving_cell_pin -no_design_rule
