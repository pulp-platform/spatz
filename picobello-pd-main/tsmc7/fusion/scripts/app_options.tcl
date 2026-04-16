# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
#
# General app options

# Step options: Enable CCD, high effort clock hold optimization
set_app_options -name place_opt.flow.enable_ccd -value true
set_app_options -name clock_opt.flow.enable_ccd -value true
set_app_options -name route_opt.flow.enable_ccd -value false

# Set maximum fanout for tie cells
set_app_options -name opt.tie_cell.max_fanout -value 8
set_app_options -name cts.common.max_fanout -value 16
set_app_options -name cts.common.default_max_transition -value 0.2

# Floorplanning options
# Prevents re-initialization of the floorplan during `compile_fusion`
set_app_options -name compile.auto_floorplan.initialize -value false
# We account for the fact that we only have 2x width boundary cells
# which is why we set the `plan.flow.segment_rule` to `horizontal_even`
set_app_options -name plan.flow.segment_rule -value horizontal_even

# Activate POCVM
set_app_options -name time.pocvm_enable_analysis -value true
# Ensure AOCV is disabled to ensure it cannot overwrite POCV. From ref flow
reset_app_options time.aocvm_enable_analysis
set_app_options -name time.enable_constraint_variation -value true
set_app_options -name time.enable_slew_variation -value true
set_app_options -name time.pocvm_precedence -value library

# Improve correlation with StarRC/PT
# TODO(fischeti): Check whether this is needed for TSMC 7nm (taken from GF12 flow)
set_app_options -name extract.enable_zero_pincap -value true
set_app_options -name extract.enable_interlayer_coupling_cap -value true
set_app_options -name extract.dense_region_cc -value true
set_app_options -name extract.max_scan_track -value 20

# Clock NDR rules are specified before physical synthesis as the initial_drc stage already starts creating buffer trees
# The reference flow by default uses double width and double spacing clock routing for the root and internal nets
# No shielding is used and the leaf routes are done using the default routing rule
# We don't use double width and spacing for the M1 layer
set clock_routing_layers {M2 M3 M4 M5 M6 M7 M8 M9 M10 M11}

# Helper function which returns a list of layer names with doubled attribute
# interleaved such as: {M2 0.1 M3 0.2 M4 0.3 ...}
proc double_attrs {layers attr} {
    concat {*}[lmap layer $layers {list $layer [expr {[get_attribute [get_layers $layer] $attr] * 2}]}]
}

if {[sizeof_collection [get_routing_rules ndr_2w2s -quiet]] > 0} {
    remove_routing_rules ndr_2w2s
}

create_routing_rule ndr_2w2s -default_reference_rule \
    -widths [double_attrs $clock_routing_layers min_width] \
    -spacings [double_attrs $clock_routing_layers min_spacing]

set_clock_routing_rules -net_type root -rule ndr_2w2s -min_routing_layer M1 -max_routing_layer M11
set_clock_routing_rules -net_type internal -rule ndr_2w2s -min_routing_layer M1 -max_routing_layer M11

# Router options
set_app_options -name route.global.timing_driven -value true
set_app_options -name route.track.timing_driven -value true
set_app_options -name route.detail.timing_driven -value true
set_app_options -name route.track.crosstalk_driven -value true

# Route optimization options
set_app_options -name route_opt.flow.enable_power -value false
set_app_options -name route_opt.flow.enable_multibit_debanking -value true

# Restrict connections to macro pins
set_app_options -name route.common.single_connection_to_pins -value macro_cell_pins
