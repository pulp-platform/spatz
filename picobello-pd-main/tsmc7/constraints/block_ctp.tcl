# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set_max_transition -clock_path 0.1 [get_clocks soc_clk]

set_ignored_layers -min_routing_layer M1 -max_routing_layer M11

set_lib_cell_purpose -exclude cts [get_lib_cells */*]
set_lib_cell_purpose -include cts [get_lib_cells [list */*CK*]]

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
