# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set_app_options -name plan.pgroute.max_undo_steps -value 20
set_app_options -name plan.pgroute.verbose -value true

remove_pg_regions -all
remove_pg_patterns -all
remove_pg_strategies -all
remove_pg_strategy_via_rules -all
remove_pg_via_master_rules -all

############################
## Custom PG Vias & Rules ##
############################

set V0_width 0.02
set V0_height 0.05

set V1_width 0.02
set V1_height 0.02

set V2_width 0.02
set V2_height 0.02

set M0_width 0.06

set M1_stich_width 0.037
set M1_stich_length 0.21

set M2_stich_width 0.02
set M2_stich_length 0.184

set M3_width 0.04

if {[sizeof_collection [get_via_defs -quiet] ] > 0} {
    remove_via_defs [get_via_defs -quiet]
}

# We create custom vias for the PG grid on M0 to M3, which are based on the
# ARM recommendations and should not violate any DRC rules
create_via_def VIA01_PG \
    -lower_layer M0 -upper_layer M1 -cut_layer VIA0 \
    -cut_size [list $V0_width $V0_height] \
    -lower_enclosure [list 0.03 [expr ($M0_width - $V0_height) / 2]] \
    -upper_enclosure [list [expr ($M1_stich_width - $V0_width) / 2] [expr ($M1_stich_length - $V0_height) / 2]] \
    -is_excluded_for_signal_route

create_via_def VIA12_PG \
    -lower_layer M1 -upper_layer M2 -cut_layer VIA1 \
    -cut_size [list $V1_width $V1_height] \
    -lower_enclosure [list [expr ($M1_stich_width - $V1_width) / 2] [expr ($M1_stich_length - $V1_height) / 2]] \
    -upper_enclosure [list [expr ($M2_stich_length - $V1_width) / 2] [expr ($M2_stich_width - $V1_height) / 2]] \
    -is_excluded_for_signal_route

create_via_def VIA23_PG \
    -lower_layer M2 -upper_layer M3 -cut_layer VIA2 \
    -cut_size [list $V2_width $V2_height] \
    -lower_enclosure [list [expr ($M2_stich_length - $V2_width) / 2] [expr ($M2_stich_width - $V2_height) / 2]] \
    -upper_enclosure [list [expr ($M3_width - $V2_width) / 2] 0.03] \
    -is_excluded_for_signal_route

# We create the master rules for the custom vias, which will make sure that they are aligned to the tracks automatically
set_pg_via_master_rule pg_via01_mrule -contact_code {VIA01_PG} -via_array_dimension {1 1} -track_alignment track
set_pg_via_master_rule pg_via12_mrule -contact_code {VIA12_PG} -via_array_dimension {1 1} -track_alignment track
set_pg_via_master_rule pg_via23_mrule -contact_code {VIA23_PG} -via_array_dimension {1 1} -track_alignment track

# For some reason, Fusion will not create an array of vias by default on VIA7 and VIA8.
# Therefore, we create additional via master rules here for the those cut layers
set_pg_via_master_rule pg_via45_mrule -contact_code VIA45_big_BW76_UW76 -via_array_dimension {2 1}
set_pg_via_master_rule pg_via56_mrule -contact_code VIA56_big_BW76_UW76 -via_array_dimension {2 1}
set_pg_via_master_rule pg_via67_mrule -contact_code VIA67_big_BW76_UW76 -via_array_dimension {2 1}
set_pg_via_master_rule pg_via78_mrule -contact_code VIA78_big_BW76_UW76 -via_array_dimension {2 1}
set_pg_via_master_rule pg_via89_mrule -contact_code VIA89_big_BW76_UW76 -via_array_dimension {2 1}

set_pg_via_master_rule pg_via56_fll_mrule -contact_code VIA56_big_BW76_UW76 -via_array_dimension {2 5}
set_pg_via_master_rule pg_via67_fll_mrule -contact_code VIA67_big_BW76_UW76 -via_array_dimension {2 5}
set_pg_via_master_rule pg_via78_fll_mrule -contact_code VIA78_big_BW76_UW76 -via_array_dimension {2 5}
set_pg_via_master_rule pg_via89_fll_mrule -contact_code VIA89_big_BW76_UW76 -via_array_dimension {2 5}
set_pg_via_master_rule pg_via910_fll_mrule -contact_code VIA910_1cut -via_array_dimension {2 5}
set_pg_via_master_rule pg_via1011_fll_mrule -contact_code VIA1011_1cut -via_array_dimension {2 5}

################
## PG Regions ##
################

set pg_ring_width 2 ;           # The max width for M4
set pg_ring_spacing_M14 0.585 ; # The min spacing for M14, for width of 2um
set pg_ring_blockage_M12 1.35 ; # The I/O pads have a blockage of 1.35um around them on M12
set pg_ring_offset [expr -(2 * $pg_ring_width + $pg_ring_blockage_M12 + $pg_ring_blockage_M12)]
set pg_ring_strap_offset [expr $pg_ring_offset - $pg_ring_width]

# This is just the entire core region on the top-level
create_pg_region -core core_pg_region

# We create a PG region for the rings *inside* of the core region (the `expand` has a negative value)
create_pg_region -core -expand [list $pg_ring_offset $pg_ring_offset] ring_pg_region

# We create a PG region for the vertical straps, in order to not interfere with the vertical rings
create_pg_region -core -expand [list 0 $pg_ring_strap_offset] top_strap_ver_pg_region
create_pg_region -core -expand [list $pg_ring_strap_offset 0] top_strap_hor_pg_region

# We create a PG region for each of the block instances
foreach block [get_all_block_insts] {
    set block_name [get_attribute [get_cells $block] name]
    # We create a PG region for the rings *inside* of the core region (the `expand` has a negative value)
    create_pg_region -block [get_cells $block] block_${block_name}_pg_region
}

#####################
## I/O Power Ring  ##
#####################

# Create the PG ring pattern
create_pg_ring_pattern pg_ring_pattern \
    -parameters {hor_layer ver_layer width spacing} \
    -horizontal_layer {@hor_layer} -vertical_layer {@ver_layer} \
    -horizontal_width {@width} -vertical_width {@width} \
    -horizontal_spacing {@spacing} -vertical_spacing {@spacing} \
    -corner_bridge true

# For every layer pair we create a PG strategy for the ring with the same ring width
foreach layer_pair {{M10 M11} {M12 M13} {M14 M13}} {
    set hor_layer [linVPPdex $layer_pair 0]
    set ver_layer [lindex $layer_pair 1]

    set_pg_strategy pg_ring_${hor_layer}_${ver_layer} \
        -pg_regions ring_pg_region \
        -pattern [subst {
            {name: pg_ring_pattern}
            {nets: {VDD VSS}}
            {parameters: $hor_layer $ver_layer $pg_ring_width $pg_ring_blockage_M12}
        }]
}

############################
## I/O Power connections  ##
############################

# Create a PG pattern to connect the I/O power pads to the rings
# `-layers`: Only on of the layers is actually used for the straps but we need to specify both
# `-pin_conn_type`: The macro pins qualify as scattered pins
# `-via_rule`: Disable via creation on the macro pins themself
create_pg_macro_conn_pattern ring_connect_pattern \
    -parameters {hor_layer ver_layer pin_layer} \
    -layers {@hor_layer @ver_layer} \
    -width 0.72 \
    -pin_layers {@pin_layer} \
    -pin_conn_type scattered_pin \
    -via_rule {{intersection: all} {via_master: NIL}}

# no need for via rule here since line 138 takes care of that
# top and bottom power IO connected using M9, M11 and M13
set_pg_strategy pg_ring_connect_M9 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M8 M9 M9}} \
    -macros [get_cells -of_references {PVDD1*_V}]

set_pg_strategy pg_ring_connect_M11 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M10 M11 M11}} \
    -macros [get_cells -of_references {PVDD1*_V}]

set_pg_strategy pg_ring_connect_M13 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M12 M13 M13}} \
    -macros [get_cells -of_references {PVDD1*_V}]


# left and right power IO connected using M10, M12 and M14
set_pg_strategy pg_ring_connect_M10 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M10 M9 M10}} \
    -macros [get_cells -of_references {PVDD1*_H}]

set_pg_strategy pg_ring_connect_M12 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M12 M11 M12}} \
    -macros [get_cells -of_references {PVDD1*_H}]

set_pg_strategy pg_ring_connect_M14 \
    -pattern {{name: ring_connect_pattern} \
              {nets: {VDD VSS}} \
              {parameters: M14 M13 M14}} \
    -macros [get_cells -of_references {PVDD1*_H}]

#######################
## M0 std cell rails ##
#######################

# Create the std rail pattern on M0
create_pg_std_cell_conn_pattern pg_rail_pattern -layers M0

# Create the rail strategy for the top level which excludes the blocks
set_pg_strategy top_rail_pg_strategy \
    -core -blockage [subst {blocks: [get_all_block_insts]}] \
    -pattern {{pattern: pg_rail_pattern} {nets: {VSS VDD}}}

# Create the rail strategy for the blocks itself
set_pg_strategy block_rail_pg_strategy \
    -pg_regions [get_pg_regions block*] \
    -pattern {{pattern: pg_rail_pattern} {nets: {VSS VDD}}}

# We don't want to connect the std cell rails to the existing PG rings
# The vias to the M3 straps will be created later
set_pg_strategy_via_rule rail_pg_strategy_via_rule \
    -via_rule {{intersection: undefined} {via_master: {NIL}}}


#####################
##  Mesh Patterns  ##
#####################

set M3_pg_mesh_width 0.04; # From the ARM PG recommendations
set M3_pg_mesh_pitch [expr 64 * [get_attribute [get_layers M3] pitch]]; # Place a VDD/VSS strap every 64/2 M3 tracks

set M4_pg_mesh_width 0.076; # Kind of arbitrary chosen; one of the allowed widths for My layers
set M4_pg_mesh_pitch [expr 64 * [get_attribute [get_layers M4] pitch]]; # Place a VDD/VSS strap every 64/2 M4 tracks
set M4_pg_mesh_macro_pitch 1.368; # Place a VDD/VSS strap every 8 M4 tracks for the macros

set M6_pg_mesh_width 0.9; # Same as FLL terminals
set M6_pg_mesh_pitch 2.2; # Same as FLL terminals

set M11_pg_mesh_width 0.279; # Kind of arbitrary chosen (spacing threshold value); ~4x the M7 width
set M11_pg_mesh_pitch [get_attribute [get_grids block_grid] x_step]; # Should align to the block grid

set M12_M13_pg_mesh_width 1.35; # Kind of arbitrary chosen (spacing threshold value); ~4x the M10 width
set M12_M13_pg_mesh_pitch [expr 64 * [get_attribute [get_layers M12] pitch]]; # every Nth track
set M12_M13_pg_mesh_offset 0; # Offset for cleaner mesh

set M14_M15_pg_mesh_width 10.8 ; # Maximum width
set M14_M15_pg_mesh_pitch 28   ; # This should prevent violate the max density rule Mr.DN.2.1

# Create straps on M3, M4, M11 for the block-level mesh
create_pg_mesh_pattern M3_M4_pg_mesh_pattern -layers [subst {
        {{vertical_layer: M3}{trim: true}{track_alignment: track}
         {width: $M3_pg_mesh_width} {spacing: interleaving} {pitch: $M3_pg_mesh_pitch } {offset: [expr $M3_pg_mesh_pitch / 4]}}
        {{horizontal_layer: M4}{trim: true}{track_alignment: track}
         {width: $M4_pg_mesh_width} {spacing: interleaving} {pitch: $M4_pg_mesh_pitch } {offset: [expr $M4_pg_mesh_pitch / 4]}}
    }] -via_rule { \
        {{layers: M3}{layers: M4 }{via_master: VIA34_big_BW40_UW76 default}} \
        {{intersection: undefined}{via_master: NIL}}
    }

# Create horizontal straps on top of the FLL on M6
create_pg_mesh_pattern M6_pg_mesh_pattern -layers [subst {
        {{horizontal_layer: M6} {width: $M6_pg_mesh_width} {spacing: interleaving} {pitch: $M6_pg_mesh_pitch}}
    }]

# Macro Connection Patterns
create_pg_macro_conn_pattern SRAM_conn_pattern \
    -pin_conn_type long_pin \
    -layers M4 \
    -direction horizontal \
    -width $M4_pg_mesh_width \
    -pitch $M4_pg_mesh_macro_pitch \
    -spacing $M4_pg_mesh_macro_pitch \
    -pin_layers M3

# The M11 mesh/straps are shared between the block and top level
create_pg_mesh_pattern M11_pg_mesh_pattern -layers [subst {
        {{vertical_layer:   M11} {width: $M11_pg_mesh_width} {spacing: interleaving} {pitch: $M11_pg_mesh_pitch} {offset: [expr $M11_pg_mesh_pitch / 4]}}
    }]

# Create additional straps on M12 to M15 for the top-level mesh
create_pg_mesh_pattern M12_pg_mesh_pattern -layers [subst {
        {{horizontal_layer: M12} {width: $M12_M13_pg_mesh_width} {spacing: interleaving} {pitch: $M12_M13_pg_mesh_pitch} {offset: $M12_M13_pg_mesh_offset}}
    }] -via_rule {{{intersection: undefined}{via_master: NIL}}}

create_pg_mesh_pattern M13_pg_mesh_pattern -layers [subst {
        {{vertical_layer:   M13} {width: $M12_M13_pg_mesh_width} {spacing: interleaving} {pitch: $M12_M13_pg_mesh_pitch} {offset: $M12_M13_pg_mesh_offset}}
    }] -via_rule {{{intersection: undefined}{via_master: NIL}}}

create_pg_mesh_pattern M14_pg_mesh_pattern -layers [subst {
        {{horizontal_layer: M14} {width: $M14_M15_pg_mesh_width} {spacing: interleaving} {pitch: $M14_M15_pg_mesh_pitch}}
    }] -via_rule {{{intersection: undefined}{via_master: NIL}}}

create_pg_mesh_pattern M15_pg_mesh_pattern -layers [subst {
        {{vertical_layer:   M15} {width: $M14_M15_pg_mesh_width} {spacing: interleaving} {pitch: $M14_M15_pg_mesh_pitch}}
    }] -via_rule {{{intersection: undefined}{via_master: NIL}}}


#######################
##  Mesh Strategies  ##
#######################

set all_sram_macros [get_object_name [get_cells -hierarchical -filter {design_type==macro && is_memory_cell}]]

# We create the M3/M4 mesh on both block and top level, but separate PG
# regions i.e. not aligned
set_pg_strategy M3_M4_top_mesh_pg_strategy \
    -pattern {{pattern: M3_M4_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -core \
    -blockage [subst { {{blocks: [get_all_block_insts]}} {{macros: $all_sram_macros}}} ] \
    -tag M3_M4_mesh

set_pg_strategy M3_M4_block_mesh_pg_strategy \
    -pattern {{pattern: M3_M4_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -blockage [subst { {macros: $all_sram_macros}}] \
    -pg_regions [get_pg_regions block*] \
    -tag M3_M4_mesh

set_pg_strategy M6_FLL_mesh_pg_strategy \
    -pattern {{pattern: M6_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -macros [get_cells i_tsmc7_FLL]

# The M11 mesh/straps are aligned in block and top, we use the core PG region
set_pg_strategy M11_mesh_pg_strategy \
    -pattern {{pattern: M11_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -pg_regions top_strap_ver_pg_region -tag M11_mesh

# The M12 to M15 mesh is the whole core, but we only compile it on the top level
# because we don't want to deal with any alignment.
set_pg_strategy M12_pg_strategy \
    -pattern {{pattern: M12_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -pg_regions top_strap_hor_pg_region -tag M12_to_M13

set_pg_strategy M13_pg_strategy \
    -pattern {{pattern: M13_pg_mesh_pattern} {nets: {VSS VDD}}} \
   -pg_regions top_strap_ver_pg_region -tag M12_to_M13

set_pg_strategy M14_pg_strategy \
    -pattern {{pattern: M14_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -pg_regions top_strap_hor_pg_region -tag M14_to_M15

set_pg_strategy M15_pg_strategy \
    -pattern {{pattern: M15_pg_mesh_pattern} {nets: {VSS VDD}}} \
    -pg_regions top_strap_ver_pg_region -tag M12_to_M15


######################
##  Via Strategies  ##
######################

# From the M3 straps, we directly drop down to M0 to connect to the std cell rails
# using the custom vias (from the ARM manual)
set_pg_strategy_via_rule M3_to_M0_rails_pg_strategy_via_rule -via_rule { \
    {{{strategies: {M3_M4_block_mesh_pg_strategy M3_M4_top_mesh_pg_strategy}} {layers: M3}} \
     {{existing:   std_conn                                                 } {layers: M0}} \
     {via_master: {pg_via01_mrule pg_via12_mrule pg_via23_mrule}}} \
    {{intersection: undefined}{via_master: NIL}}}

# From the M4 straps, connect to the M3 pins of the SRAM macros
set_pg_strategy_via_rule M4_to_SRAM_pins_pg_strategy_via_rule -via_rule { \
    {{{strategies: M3_M4_block_mesh_pg_strategy} {layers: M4}} \
     {{macro_pins: {VDD VSS}} {layers: M3}} \
     {via_master: {VIA34_1cut_BW40_UW76 VIA34_big_BW40_UW76}}} \
    {{{strategies: M3_M4_top_mesh_pg_strategy} {layers: M4}} \
     {{macro_pins: {VDD VSS}} {layers: M3}} \
     {via_master: {VIA34_1cut_BW40_UW76 VIA34_big_BW40_UW76}}}}

# From the M6 straps, we connect to the FLL on the M5 terminals
set_pg_strategy_via_rule M6_to_FLL_pg_strategy_via_rule -via_rule { \
    {{{strategies: M6_FLL_mesh_pg_strategy} {layers: M6}} \
     {{macro_pins: {VDD VSS VDDA VSSA}} {layers: M5}} \
     {via_master: {VIA56_big_BW76_UW76}}} \
    {{intersection: undefined}{via_master: NIL}}}

# From the M11 straps, we connect down to the M6 on of the FLL
set_pg_strategy_via_rule M11_to_M6_pg_strategy_via_rule -via_rule { \
    {{{strategies: M11_mesh_pg_strategy   } {layers: M11}} \
     {{strategies: M6_FLL_mesh_pg_strategy} {layers: M6}} \
     {via_master: {pg_via56_fll_mrule pg_via67_fll_mrule pg_via78_fll_mrule pg_via89_fll_mrule pg_via910_fll_mrule pg_via1011_fll_mrule}}} \
    {{intersection: undefined}{via_master: NIL}}}

# From the M11 straps, we connect down to the M4 straps
set_pg_strategy_via_rule M11_to_M4_pg_strategy_via_rule -via_rule { \
    {{{strategies: M11_mesh_pg_strategy                                                                       } {layers: M11}} \
     {{strategies: {M3_M4_block_mesh_pg_strategy M3_M4_top_mesh_pg_strategy M4_to_SRAM_connection_pg_strategy}} {layers: M4}} \
     {via_master: {pg_via45_mrule pg_via56_mrule pg_via67_mrule pg_via78_mrule pg_via89_mrule VIA910_1cut_BW114 VIA1011_1cut}}} \
    {{intersection: undefined}{via_master: NIL}}}

# From the M11 straps, we also connect to the M10 and M12 rings
set_pg_strategy_via_rule M11_to_rings_pg_strategy_via_rule -via_rule { \
    {{{strategies: M11_mesh_pg_strategy   } {layers: M11}} \
     {{existing: ring} {layers: {M10 M12}}} \
     {via_master: {default}}} \
    {{intersection: undefined}{via_master: NIL}}}


# For all the mesh straps, we connect them to the existing PG rings,
# but only the layer above the specific mesh layer
# On every other intersection between mesh and the rings, we don't create vias

set_pg_strategy_via_rule mesh_to_ring_pg_strategy_via_rule -via_rule { \
    {{{strategies: M12_pg_strategy} {layers: M12}} {{existing: ring} {layers: M13}} {via_master: {default}}} \
    {{{strategies: M13_pg_strategy} {layers: M13}} {{existing: ring} {layers: M12}} {via_master: {default}}} \
    {{{strategies: M14_pg_strategy} {layers: M14}} {{existing: ring} {layers: M13}} {via_master: {default}}} \
    {{{strategies: M15_pg_strategy} {layers: M15}} {{existing: ring} {layers: M14}} {via_master: {default}}} \
    {{intersection: undefined}{via_master: NIL}}}

set_pg_strategy_via_rule mesh_to_mesh_pg_strategy_via_rule -via_rule { \
    {{{strategies: M12_pg_strategy} {layers: M12}} {{terminals: all}            {layers: M11}} {via_master: {default}}} \
    {{{strategies: M12_pg_strategy} {layers: M12}} {{existing: strap}           {layers: M11}} {via_master: {default}}} \
    {{{strategies: M12_pg_strategy} {layers: M12}} {{strategy: M13_pg_strategy} {layers: M13}} {via_master: {default}}} \
    {{{strategies: M13_pg_strategy} {layers: M13}} {{strategy: M14_pg_strategy} {layers: M14}} {via_master: {default}}} \
    {{{strategies: M14_pg_strategy} {layers: M14}} {{strategy: M15_pg_strategy} {layers: M15}} {via_master: {default}}} \
    {{intersection: undefined}{via_master: NIL}}}
