# Copyright (c) 2025 ETH Zurich.
# Albi Mema <almema@ethz.ch>

# Get all I/O cells
proc get_io_cells {} {
  return [get_cells -hierarchical -quiet -filter {is_io || pad_cell}]
}

set non_pad_io_cells [get_cells -hierarchical -quiet -filter {(is_io || pad_cell) && (name=~*__added_* || name=~*RTE* || name =~*dummy*)}]
set pad_io_cells [remove_from_collection [get_io_cells] $non_pad_io_cells]

if {[sizeof_collection [get_lib_cells -quiet PAD64]] == 0} {
    set_ref_libs -add tpbn7_univ.ndm
}

remove_cells *_PAD
remove_matching_types *
remove_shapes [get_shapes -filter (shape_use==rdl)]
remove_shapes [get_shapes -filter "((shape_type==text) && (layer_name==TEXT_AP))"]
set pad2cell_spacing 1.35
set pad_reference PAD64
set pad_cell_width 56
set pad_cell_height 68


foreach_in_collection io_cell $pad_io_cells {

  set cell_name [get_attribute $io_cell name]
  set cell_reference [get_attribute $io_cell ref_name]
  set cell_orientation [get_attribute $io_cell orientation]
  set cell_llx [lindex [lindex [get_attribute $io_cell boundary_bbox] 0] 0]
  set cell_lly [lindex [lindex [get_attribute $io_cell boundary_bbox] 0] 1]
  set cell_urx [lindex [lindex [get_attribute $io_cell boundary_bbox] 1] 0]
  set cell_ury [lindex [lindex [get_attribute $io_cell boundary_bbox] 1] 1]

  puts "Creating cell ${cell_name}_PAD"
  create_cell "${cell_name}_PAD" PAD64

  set pin2pad ""
  # create matching types
  if {[string match "*_io_*" $cell_name]} {
    if {[string match "*_power_*" $cell_name]} {

      set pin2pad ${cell_name}/VDDPST
      create_matching_type -name ${cell_name} [list $pin2pad  ${cell_name}_PAD/BUMP ]

    } elseif {[string match "*_gnd_*" $cell_name]} {
      set pin2pad ${cell_name}/VSSPST
      create_matching_type -name ${cell_name} [list $pin2pad  ${cell_name}_PAD/BUMP ]

    }
  } elseif {[string match "*_core_*" $cell_name]} {

    if {[string match "*_power_*" $cell_name]} {
      set pin2pad ${cell_name}/VDD
      create_matching_type -name ${cell_name} [list $pin2pad  ${cell_name}_PAD/BUMP ]

    } elseif {[string match "*_gnd_*" $cell_name]} {
      set pin2pad ${cell_name}/VSS
      create_matching_type -name ${cell_name} [list $pin2pad ${cell_name}_PAD/BUMP ]
    }

  } else {
    set pin2pad ${cell_name}/PAD
    create_matching_type -name ${cell_name} [list ${cell_name}/PAD  ${cell_name}_PAD/BUMP ]
  }

  set pin_shape [get_shapes -of $pin2pad -filter "layer.name == AP"]
  set pin_net_name [get_attribute $pin2pad net_name]

  set pin_llx [lindex [lindex [get_attribute $pin_shape bbox] 0] 0]
  set pin_lly [lindex [lindex [get_attribute $pin_shape bbox] 0] 1]
  set pin_urx [lindex [lindex [get_attribute $pin_shape bbox] 1] 0]
  set pin_ury [lindex [lindex [get_attribute $pin_shape bbox] 1] 1]

  #place and connect the cells
  if {[string match "*_H" $cell_reference]} {
    # Left or right pads
    if {$cell_orientation == "R0"} {
      # Right io cell
        set_attribute ${cell_name}_PAD -name orientation -value MXR90

        set x_origin [expr $cell_urx + $pad2cell_spacing]
        set y_origin [expr ($cell_ury + $cell_lly - $pad_cell_width)/2]

        set_attribute ${cell_name}_PAD -name origin -value [list $x_origin $y_origin ]
        # set_attribute ${cell_name}_PAD/BUMP -name net -value $pin_net_name

        set text_mid_point_X [expr $x_origin + $pad_cell_height/2]
        set text_mid_point_Y [expr $y_origin + $pad_cell_width/2]

        create_shape -shape_type text -text $pin_net_name -origin [list $text_mid_point_X $text_mid_point_Y] -height 4 -layer TEXT_AP
        create_shape -shape_use rdl -net $pin_net_name -shape_type rect -layer AP -boundary [list [list $pin_llx $pin_lly] [list $x_origin $pin_ury]]

    } elseif {$cell_orientation == "R180"} {
      # Left io cell
        set_attribute ${cell_name}_PAD -name orientation -value R90

        set x_origin [expr $cell_llx - $pad2cell_spacing]
        set y_origin [expr ($cell_ury + $cell_lly - $pad_cell_width)/2]

        set_attribute ${cell_name}_PAD -name origin -value [list $x_origin $y_origin ]
        # set_attribute ${cell_name}_PAD/BUMP -name net -value $pin_net_name

        set text_mid_point_X [expr $x_origin - $pad_cell_height/2]
        set text_mid_point_Y [expr $y_origin + $pad_cell_width/2]

        create_shape -shape_type text -text $pin_net_name -origin [list $text_mid_point_X $text_mid_point_Y] -height 4 -layer TEXT_AP

        create_shape -net $pin_net_name -shape_use rdl -shape_type rect -layer AP -boundary [list [list $x_origin $pin_lly] [list $pin_urx $pin_ury]]

    } else {
      puts "Something wrong with figuring out the orientation of cell $cell_name"
    }

  } elseif {[string match "*_V" $cell_reference]} {
    # Top of bottom pads
    if {$cell_orientation == "R0"} {
        # bottom io cell
        set_attribute ${cell_name}_PAD -name orientation -value R180

        set x_origin [expr ($cell_urx + $cell_llx + $pad_cell_width)/2  ]
        set y_origin [expr $cell_lly - $pad2cell_spacing]

        set_attribute ${cell_name}_PAD -name origin -value [list $x_origin $y_origin ]
        # set_attribute ${cell_name}_PAD/BUMP -name net -value $pin_net_name

        set text_mid_point_X [expr $x_origin - $pad_cell_width/2]
        set text_mid_point_Y [expr $y_origin - $pad_cell_height/2]

        create_shape -shape_type text -text $pin_net_name -origin [list $text_mid_point_X $text_mid_point_Y] -height 4 -layer TEXT_AP
        create_shape -net $pin_net_name -shape_use rdl -shape_type rect -layer AP -boundary [list [list $pin_llx $pin_lly] [list $pin_urx $y_origin]]

    } elseif {$cell_orientation == "R180"} {
        # top io cell

        set_attribute ${cell_name}_PAD -name orientation -value R0

        set x_origin [expr ($cell_urx + $cell_llx - $pad_cell_width)/2  ]
        set y_origin [expr $cell_ury + $pad2cell_spacing]

        set_attribute ${cell_name}_PAD -name origin -value [list $x_origin $y_origin ]
        # set_attribute ${cell_name}_PAD/BUMP -name net -value $pin_net_name

        set text_mid_point_X [expr $x_origin + $pad_cell_width/2]
        set text_mid_point_Y [expr $y_origin + $pad_cell_height/2]

        create_shape -shape_type text -text $pin_net_name -origin [list $text_mid_point_X $text_mid_point_Y] -height 4 -layer TEXT_AP
        create_shape -net $pin_net_name -shape_use rdl -shape_type rect -layer AP -boundary [list [list $pin_llx $y_origin] [list $pin_urx $pin_lly]]

    } else {
      puts "Something wrong with figuring out the orientation of cell $cell_name"
    }

  } else {
    puts "Something wrong with figuring out the side of the cell $cell_name"
  }

}

set_fixed_objects [get_cells *_PAD]
set_fixed_objects [get_shapes -filter "(shape_use==rdl) || ((shape_type==text) && (layer_name==TEXT_AP))"]
