# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# gui_remove_all_annotations

proc remove_bump_colors {} {
    gui_remove_annotations [gui_get_annotations -filter "client_data==user_coloring"]
}

remove_bump_colors

proc color_bumps {bumps color} {

    foreach_in_collection bump $bumps {
        set bump_shape [get_shapes -of_objects $bump]
        set bump_points [get_attribute $bump_shape points]

        set annotation [gui_add_annotation -color $color $bump_points]

        set_attribute $annotation client_data user_coloring
    }
}

color_bumps [get_cells [dict get $bump_dict VDD]] blue
color_bumps [get_cells [dict get $bump_dict VSS]] red
color_bumps [get_cells [dict get $bump_dict VDDIO]] light_blue
color_bumps [get_cells [dict get $bump_dict VSSIO]] light_red

set signal_bump_dict [dict remove $bump_dict VDD VSS VSSIO VDDIO]
dict for {signal bumps} $signal_bump_dict {
    color_bumps [get_cells $bumps] green
}
