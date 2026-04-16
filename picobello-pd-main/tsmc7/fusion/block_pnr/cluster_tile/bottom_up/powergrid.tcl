# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Disclaimer: This script is just a basic example intended for
# backend trials and should not be used for actual tapeouts

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

#######################
## M0 std cell rails ##
#######################

# Create the std rail pattern on M0
create_pg_std_cell_conn_pattern pg_rail_pattern -layers M0

# Create the rail strategy for the top level which excludes the blocks
set_pg_strategy rail_pg_strategy -core \
    -pattern {{pattern: pg_rail_pattern} {nets: {VSS VDD}}}

############
##  Mesh  ##
############

set M3_pg_mesh_width 0.04; # From the ARM PG recommendations
set M3_pg_mesh_pitch [expr 64 * [get_attribute [get_layers M3] pitch]]; # Place a VDD/VSS strap every 64/2 M3 tracks

set M4_pg_mesh_width 0.076; # Kind of arbitrary chosen; one of the allowed widths for My layers
set M4_pg_mesh_pitch [expr 64 * [get_attribute [get_layers M4] pitch]]; # Place a VDD/VSS strap every 64/2 M4 tracks
set M4_pg_mesh_macro_pitch 1.368; # Place a VDD/VSS strap every 8 M4 tracks for the macros

set M11_pg_mesh_width 0.279; # Kind of arbitrary chosen (spacing threshold value); ~4x the M7 width
set M11_pg_mesh_pitch [lcm [list \
    [get_attribute [get_layers M5] pitch] \
    [get_attribute [get_site_defs unit] height]]]; # Should align to the block grid

# Create straps on M3, M4, M11 for the block-level mesh
create_pg_mesh_pattern pg_mesh_pattern -layers [subst {
        {{vertical_layer: M3}{trim: true}{track_alignment: track}
         {width: $M3_pg_mesh_width} {spacing: interleaving} {pitch: $M3_pg_mesh_pitch } {offset: [expr $M3_pg_mesh_pitch / 4]}}
        {{horizontal_layer: M4}{trim: true}{track_alignment: track}
         {width: $M4_pg_mesh_width} {spacing: interleaving} {pitch: $M4_pg_mesh_pitch } {offset: [expr $M4_pg_mesh_pitch / 4]}}
        {{vertical_layer:   M11}
         {width: $M11_pg_mesh_width} {spacing: interleaving} {pitch: $M11_pg_mesh_pitch} {offset: [expr $M11_pg_mesh_pitch / 4]}}
    }] -via_rule { \
        {{layers: M3}{layers: M4 }{via_master: VIA34_big_BW40_UW76 default}} \
        {{layers: M4}{layers: M11}{via_master: {pg_via45_mrule pg_via56_mrule pg_via67_mrule pg_via78_mrule pg_via89_mrule VIA910_1cut_BW114 VIA1011_1cut}}} \
        {{intersection: undefined}{via_master: NIL}}
    }

# The corresponding strategy for the mesh
set_pg_strategy mesh_pg_strategy -core -pattern {{pattern: pg_mesh_pattern} {nets: {VSS VDD}}} \

######################
##  Via Strategies  ##
######################

# From the M3 straps, we directly drop down to M0 to connect to the std cell rails
# using the custom vias (from the ARM manual)
set_pg_strategy_via_rule M3_to_M0_rails_pg_strategy_via_rule -via_rule { \
    {{{strategies: {mesh_pg_strategy}} {layers: M3}} \
     {{existing:   std_conn          } {layers: M0}} \
     {via_master: {pg_via01_mrule pg_via12_mrule pg_via23_mrule}}} \
    {{intersection: undefined}{via_master: NIL}}}

# From the M4 straps, connect to the M3 pins of the SRAM macros
set_pg_strategy_via_rule M4_to_SRAM_pins_pg_strategy_via_rule -via_rule { \
    {{{strategies: mesh_pg_strategy} {layers: M4}} \
     {{macro_pins: {VDD VSS}} {layers: M3}} \
     {via_master: {VIA34_1cut_BW40_UW76 VIA34_big_BW40_UW76}}}
    {{intersection: undefined}{via_master: NIL}}}


##################
##  Compile PG  ##
##################

# Compile the PG rails
compile_pg -strategies rail_pg_strategy

# Trim the rails before continuing with the power straps to prevent later dropping
# of vias in places where the rails are not supposed to be
trim_pg_mesh -layers M0

# Compile the PG mesh
compile_pg -strategies { \
    mesh_pg_strategy \
} -via_rule { \
    M3_to_M0_rails_pg_strategy_via_rule \
    M4_to_SRAM_pins_pg_strategy_via_rule
}

# Trim the lower layers of the mesh
trim_pg_mesh -layers {M3 M4}
