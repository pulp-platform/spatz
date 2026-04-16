# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set mem_tile [get_block_inst mem_tile 7]

set mem_cheshire_hold_paths [get_timing_paths -through $mem_tile -delay_type min -slack_lesser_than -0.005 -nworst 100 -include_hierarchical_pins]

set noc_pins_to_buffer ""
foreach_in_collection timing_path $mem_cheshire_hold_paths {
    set noc_pins_to_buffer [add_to_collection $noc_pins_to_buffer [filter_collection [get_attribute $timing_path points] "name=~[get_object_name $mem_tile]/floo*"]]
}

add_buffer \
  -new_net_names "buffer_mem7_to_cheshire" \
  -new_cell_names "buffer_mem7_to_cheshire" \
  -lib_cell DELCD1BWP240H8P57PDSVT [get_attribute $noc_pins_to_buffer object]

place_eco_cells -cells [get_cells buffer_mem7_to_cheshire*] -legalize_mode free_site_only

route_eco -open_net_driven true -reuse_existing_global_route true
