# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

##################
##  Initialize  ##
##################

set FCDIR [file dirname [file dirname [file normalize [info script]]]]
source $FCDIR/scripts/utils.tcl
set_global_design_variables

# The following flow contains all the stages that are needed to go from RTL to a
# fully routed design. Depending on the type of handoff (NDM, RTL, bottom-up), the
# specific stages that are executed may vary. The suffix of each stage (e.g. `.ndm_handoff`)
# determines if a stage needs to be run for a specific handoff type.
# The stages and handoff relation ship is shown in the table below:
#
# +-------------------------+-----+-----+-----------+
# | stages/handoff          | ndm | rtl | bottom-up |
# +-------------------------+-----+-----+-----------+
# | *.ndm_handoff           |  X  |     |           |
# | *.rtl_handoff           |     |  X  |           |
# | *.handoff               |  X  |  X  |           |
# | *.from_rtl              |     |  X  |     X     |
# | *.bottom_up             |     |     |     X     |
# +-------------------------+-----+-----+-----------+
#
# The enabling and disabling of stages is handled in `checkpoint.config.tcl`.

############################
##  Setup/Import Library  ##
############################

# Imports the library as an NDM from the design planning flow by copying it to
# a new run location, which differs from each run with a timestamp or optionally
# a user-defined run name that is passed as `RUN_NAME` argument.
eval_stage init_library.ndm_handoff {

    if {[file exists $DESIGN_HANDOFF/ndm]} {
        copy_lib -from $DESIGN_HANDOFF/ndm/$DESIGN -to $LIBDIR
        open_block $LIBDIR:$DESIGN/logic_opto
        link_block
    } else {
        puts "ERROR: Library $DESIGN_HANDOFF/ndm does not exist."
        exit 1
    }

    file mkdir $OUTDIR/svf
    set_svf $OUTDIR/svf/import.svf

    # Create the symlinks of the new lib (see `utils.tcl`)
    create_run_symlinks

    # Define the handoff version as a user attribute and get it from the latest handoff
    # directory.
    define_user_attribute -type string -classes lib -name handoff_version
    set_attribute [current_lib] handoff_version [file tail [file readlink $HANDOFF_DIR/latest]]

    # Save the library
    save_lib
}

# Creates an entirely new library from scratch.
eval_stage init_library.from_rtl {

    # Delete the old library if it exists
    if {[file exists $LIBDIR]} {
        file delete -force $LIBDIR
    }

    # Set the list of standard cells
    set std_libs [list \
            tcbn07_bwph240l11p57pd_base_lvt.ndm \
            tcbn07_bwph240l11p57pd_base_svt.ndm \
            tcbn07_bwph240l11p57pd_base_ulvt.ndm \
            tcbn07_bwph240l11p57pd_mb_lvt.ndm \
            tcbn07_bwph240l11p57pd_mb_svt.ndm \
            tcbn07_bwph240l11p57pd_mb_ulvt.ndm \
            tcbn07_bwph240l8p57pd_base_lvt.ndm \
            tcbn07_bwph240l8p57pd_base_svt.ndm \
            tcbn07_bwph240l8p57pd_base_ulvt.ndm \
            tcbn07_bwph240l8p57pd_mb_lvt.ndm \
            tcbn07_bwph240l8p57pd_mb_svt.ndm \
            tcbn07_bwph240l8p57pd_mb_ulvt.ndm \
            tsmc7_FLL.ndm ]

    # Add SRAM ndms
    set mem_libs [list \
        tsn7_spsbsram_128x38m4.ndm \
        tsn7_spsbsram_512x64m4.ndm \
        tsn7_l1cache_512x64m4.ndm \
        tsn7_spsbsram_128x256m2.ndm \
        tsn7_spsbsram_256x36m4.ndm \
        tsn7_spsbsram_2048x64m8.ndm \
        tsn7_spsbsram_256x45m4.ndm \
        tsn7_spsbsram_256x128m4.ndm \
        tsn7_spsbsram_256x136m4.ndm \
        tsn7_spsbsram_512x256m2.ndm \
        tsn7_spsbsram_1024x128m4.ndm \
        tsn7_spsbsram_128x40m4.ndm \
        tsn7_spsbsram_256x32m4.ndm \
        tsn7_spsbsram_256x64m4.ndm ]

    # Add I/O pad ndms
    set io_libs [list tphn7_18gpio_univ.ndm]

    # Resolve path of reference libraries
    set ref_libs [lmap lib [concat $std_libs $mem_libs $io_libs] {file readlink $TECHDIR/ndm/$lib}]

    # Create the library
    create_lib $LIBDIR -technology $TECHFILE -ref_libs $ref_libs

    # We don't plan to check with Formality, so we disable the SVF
    set_svf -off

    # Create the symlinks of the new lib (see `utils.tcl`)
    create_run_symlinks

    # Save the library
    save_lib
}


###################
##  Analyze RTL  ##
###################

eval_stage analysis.from_rtl {

    # Read SystemVerilog RTL attributes
    set_app_options -name hdlin.sverilog.enable_rtl_attributes -value true

    # Analyze the RTL from the open-source and internal PD repositories
    analyze_bender {rtl asic tsmc7 cva6 cv64a6_imafdcsclic_sv39 snitch_cluster pb_gen_rtl}
}

###################
##  Elaboration  ##
###################

eval_stage elaboration.from_rtl {

    # We elaborate the entire chip, but only set the design
    # as the top module, to make sure that parameters are
    # passed from the top to the lower modules.
    elaborate picobello_chip
    set_top_module $DESIGN

    change_names -rules verilog -hierarchy
}

###################
##  Constraints  ##
###################

eval_stage constraints.from_rtl {

    set_attribute [get_site_defs unit] symmetry {Y}
    set_attribute [get_site_defs unit] is_default true

    # Set the layer directions and offsets
    set_attribute [get_layers {M0 M2 M4 M6 M8 M10 M12 M14 AP}] routing_direction horizontal
    set_attribute [get_layers {M1 M3 M5 M7 M9 M11 M13 M15}] routing_direction vertical
    set_attribute [get_layers {M0 M2}] track_offset 0.02

    # Set technology settings
    set_technology -node 7

    # Ignore upper 4 layers for routing, since they are thicker and not
    # useful for routing, but will be used for the power grid
    set_ignored_layers -max_routing_layer M11

    # Read in the parasitic files
    foreach rc {typical cworst_CCworst_T cbest_CCbest_T} {
        read_parasitic_tech \
            -tlup $TECHDIR/tlup/cln7_1p15m_1x1xa1ya5y2yy2yx2r_mim_ut-alrdl_${rc}.nxtgrd \
            -layermap $TECHDIR/tlup/PRTF_ICC2_N7_starrc_15M_1X1Xa1Ya5Y2Yy2Yx2R.13_3a.map \
            -name ${rc}
    }

    # Set the std cell placement rules
    foreach_in_collection lib [get_libs tcbn07*] {
        source $FCDIR/scripts/[get_object_name $lib]_par_icc2.tcl
    }

    # Set size_only attributes for tc_clk cells, because RTL attributes are not set
    # even if `hdlin.sverilog.enable_rtl_attributes` is set to true!
    set_tc_clk_size_only
}

eval_stage constraints.rtl_handoff {

    # Load the UPF file
    load_upf $DESIGN_HANDOFF/$DESIGN.upf
    commit_upf

    # Source the constraints
    source $DESIGN_HANDOFF/scripts/top.tcl
}

eval_stage constraints.handoff {

    # Define the handoff version as a user attribute
    define_user_attribute -type string -classes lib -name handoff_version
    set_attribute [current_lib] handoff_version [file tail [file readlink $HANDOFF_DIR/latest]]

    save_lib
}

eval_stage constraints.bottom_up {

    # Load the UPF file
    load_upf ${CONSTS_DIR}/bottom_up/$DESIGN/$DESIGN.upf
    commit_upf

    # Create the modes, corners and scenarios
    create_modes {func}
    create_corners $default_corners false
    create_scenarios

    # Read in the POCV data
    read_pocv_data

    # Source the constraints
    apply_constraints $CONSTS_DIR/bottom_up/$DESIGN

    save_lib
}

###########################
##  Additional Settings  ##
###########################

eval_stage app_options {

    # Enable common app options
    source $FCDIR/scripts/app_options.tcl

    # Activate the proper scenarios
    source $FCDIR/scripts/active_scenarios.tcl

    # Source additional design-specific app options if needed
    source_optional $FCDIR/block_pnr/$DESIGN/app_options.tcl

    # Apply patches to the handoff (e.g. constraints) if needed
    source_optional $FCDIR/block_pnr/$DESIGN/handoff_patches.tcl

    save_lib
}

#################
##  Floorplan  ##
#################

eval_stage floorplan.handoff {

    # Reset the placement of all macro and std cells
    initialize_floorplan

    # Source the bare floorplan from the DP flow
    source $DESIGN_HANDOFF/floorplan/bare/fp.tcl

    # Place the macros
    source $FCDIR/block_pnr/$DESIGN/place_macros.tcl

    # Place boundary and tap cells
    source $FCDIR/scripts/special_cells.tcl
}

eval_stage floorplan.bottom_up {

    # Source the floorplan, including macro and pin placement
    source $FCDIR/block_pnr/$DESIGN/bottom_up/fp.tcl

    # Place boundary and tap cells
    source $FCDIR/scripts/special_cells.tcl
}

#################
##  Powergrid  ##
#################

eval_stage powergrid.handoff {

    # Source the PG strategies
    source $DESIGN_HANDOFF/powergrid/pg_strategy.tcl

    # Compile the PG
    source $DESIGN_HANDOFF/powergrid/compile_pg.tcl

}

eval_stage powergrid.bottom_up {

    # Compile the PG
    source $FCDIR/block_pnr/$DESIGN/bottom_up/powergrid.tcl
}

#################
##  Synthesis  ##
#################

eval_stage synthesis.from_rtl {

    # Enable Multibit
    set ENABLE_MULTIBIT true

    # Set the stage
    set_qor_strategy -stage compile_initial

    # Optionally retime some modules
    source_optional $FCDIR/block_pnr/$DESIGN/retiming.tcl

    # Optionally disable ungrouping for some modules
    source_optional $FCDIR/block_pnr/$DESIGN/no_ungroup.tcl

    # Compile until the initial mapping step
    compile_fusion -to logic_opto
}

#################
##  Placement  ##
#################

eval_stage placement {

    # Enable Multibit
    set ENABLE_MULTIBIT true

    # Set the stage
    set_qor_strategy -stage compile_initial

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label logic_opto -report_group mapped

    # Increase the number of threads for placement
    set_host_options -max_cores 32

    # Physical synthesis
    compile_fusion -from initial_place -to initial_opto

    # Set the stage
    set_qor_strategy -stage compile_final_place

    # Physical synthesis
    compile_fusion -from final_place -to final_opto

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label placed -report_group placed
}

############################
##  Clock Tree Synthesis  ##
############################

eval_stage cts {

    # Set the stage
    set_qor_strategy -stage cts -metric timing

    # Deactivate leakage and dnamic power analysis during CTS
    set leakage_scenarios [get_scenarios -filter active&&leakage_power]
    set_scenario_status -leakage_power false [get_object_name $leakage_scenarios]
    set dynamic_scenarios [get_scenarios -filter active&&dynamic_power]
    set_scenario_status -dynamic_power false [get_object_name $dynamic_scenarios]

    # pre-CTS reports
    check_clock_tree > $::STAGE_REPDIR/pre_cts.check_clock_tree
    report_clock_settings > $::STAGE_REPDIR/pre_cts.report_clock_settings

    # Mark the clock trees, the NDR clock routing rules are defined in `app_options.tcl`
    mark_clock_trees -routing_rules

    # Increase the number of threads for CTS
    set_host_options -max_cores 32

    # Synthesize the clock tree
    log "CTS" "build_clock"
    clock_opt -from build_clock -to build_clock

    # Route the clock tree
    log "CTS" "route_clock"
    clock_opt -from route_clock -to route_clock

    connect_pg_net

    # Propagate clocks and compute IO latencies for all scenarios for abstract creation
    # NOTE(fischeti): In case an abstract is created after CTS for top-level CTS, all
    # scenarios need to be enabled at this point
    synthesize_clock_trees -propagate_only
	compute_clock_latency -verbose

    # Set the stage
    set_qor_strategy -stage post_cts_opto

    # Final CTS round
    log "CTS" "final_opto"
    clock_opt -from final_opto -to final_opto
    connect_pg_net

    # Re-activate leakage and dnamic power analysis after CTS
    set_scenario_status -leakage_power true [get_object_name $leakage_scenarios]
    set_scenario_status -dynamic_power true [get_object_name $dynamic_scenarios]

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label cts -report_group cts
}


###############
##  Routing  ##
###############

eval_stage route_auto {

    # Antenna analysis and fixing
    source $FCDIR/scripts/PRTF_ICC2_N7_15M_Antenna.13_3a.tcl

    # Set the stage
    set_qor_strategy -stage route

    # Increase number of threads for routing
    set_host_options -max_cores 32

    # Route
    log "Routing" "route_auto"
    route_auto -reuse_existing_global_route true

    # Add redundant vias
    # TODO(fischeti): Check whether/when to add redundant vias
    # source $FCDIR/scripts/PRTF_ICC2_N7_DFM_via_swap_reference_command.13_1c.tcl

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label route_auto -report_group routed

    # Checking
    check_routes
}

############################
##  Routing Optimization  ##
############################

eval_stage route_opt {

    # Set the stage
    set_qor_strategy -stage post_route

    # Compute clock latency
    compute_clock_latency

    # Increase number of threads for routing
    set_host_options -max_cores 32

    # Routing optimization
    log "Routing Optimization" "hyper_route_opt"
    hyper_route_opt

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label hyper_route_opt -report_group routed

    # TODO(fischeti): Check whether/when to add redundant vias
    # add_redundant_vias

    # Incremental route_detail for fixing routing DRCs
    log "Routing Optimization" "route_detail"
    route_detail -incremental true -initial_drc_from_input true
    connect_pg_net

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label route_detail -report_group routed
}

######################
##  Timing Signoff  ##
######################

eval_stage eco_opt {

    # Append iteration to every label to enable multiple ECO opt runs
    # Without overwriting the previous ones
    set label eco_opt
    if {[llength [glob -nocomplain $LIBDIR/$DESIGN/*eco_opt*]] > 0} {
        append label [llength [glob $LIBDIR/$DESIGN/*eco_opt*]]
    }

    # Activate PBA
    set_app_options -name time.pba_optimization_mode -value exhaustive
    set_app_options -name time.report_use_pba_optimization_mode -value true

    # Enable all scenarios
    set same_clock_scenarios [get_object_name [get_scenarios {func_*}]]
    set_scenario_status -active          false [all_scenarios]
    set_scenario_status -active          true  $same_clock_scenarios
    set_scenario_status -setup           true  $same_clock_scenarios
    set_scenario_status -hold            true  $same_clock_scenarios
    set_scenario_status -max_transition  true  $same_clock_scenarios
    set_scenario_status -max_capacitance true  $same_clock_scenarios
    set_scenario_status -min_capacitance true  $same_clock_scenarios

    # Fix names in preparation for PrimeTime run
    change_names -rules verilog -hierarchy

    # Set LC, PT, and StarRC options
    set_host_options -max_cores 16 -target PrimeTime
    set_lc_options -exec_path /usr/pack/synopsys-2024.09-dz/lc/bin/lc_shell
    set_pt_options -pt_exec /usr/pack/primetime-2022.03-zr/pts/bin/pt_shell -work_dir $REPDIR/eco
    set_starrc_options -config $FCDIR/scripts/starrc_config_file

    # Final optimization and DRC cleaning
    eco_opt -pba_mode exhaustive -types {drc hold max_clock_transition} -hold_margin 0.005

    # Final route detail
    route_detail -incremental true

    # Update the timing again
    update_timing

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label $label -report_group routed

    # Save the block manually
    save_block -as $DESIGN/$label
}

#################
##  Finishing  ##
#################

eval_stage finishing {

    # Add DECAP cells
    create_stdcell_filler \
        -lib_cell [sort_collection -descending [get_lib_cells DCAP*] name] \
        -prefix decap

    connect_pg_net -automatic
    remove_stdcell_fillers_with_violation

    # Add filler cells
    set filler_cells [remove_from_collection [get_lib_cells FILL*] [get_lib_cells FILL1NOBCM*]]
    create_stdcell_filler \
        -lib_cell [sort_collection -descending $filler_cells name] \
        -prefix filler

    connect_pg_net -automatic
    remove_stdcell_fillers_with_violation
    check_empty_space

    # Replace abutting FILL1 cells with FILL1NOBCM cells
    set fill1_replace_list ""
    foreach_in_collection cell [get_lib_cells */*FILL1BWP*] {
        set from_filler [get_attribute $cell full_name]
        set to_filler [string map {"FILL1BWP" "FILL1NOBCMBWP"} $from_filler]
        lappend fill1_replace_list [list $from_filler $to_filler]
    }

    replace_fillers_by_rules \
        -replacement_rule illegal_abutment \
        -replace_abutment $fill1_replace_list \
        -illegal_abutment [get_lib_cells */*FILL1BWP*]

    # Replace tap cells with OD violations
    foreach_in_collection tap_cell [get_lib_cells -of_objects [get_cells tapfiller*]] {
        set tap_cell_name [get_object_name $tap_cell]
        replace_fillers_by_rules -replacement_rule {od_tap_distance} \
            -tap_cells $tap_cell \
            -left_violation_tap [string map {"TAPCELLBWP" "TAPCELLL1R2BWP"} $tap_cell_name] \
            -right_violation_tap [string map {"TAPCELLBWP" "TAPCELLL2R1BWP"} $tap_cell_name] \
            -both_violation_tap [string map {"TAPCELLBWP" "TAPCELLL1R1BWP"} $tap_cell_name] \
            -tap_distance_range {0.278 5000} \
            -adjacent_non_od_cells [get_lib_cells {*/FILL1BWP* */FILL1NOBCM* */FILL2BWP*}]
    }

    connect_pg_net -automatic

    # Create the cut metals for M1 and M2
    create_cut_metals
}

##############
##  Export  ##
##############

eval_stage export {

    change_names -rules verilog -hierarchy

    # Write out the LVS netlist
    write_verilog \
        -compress gzip \
        -exclude {scalar_wire_declarations \
                  leaf_module_declarations \
                  empty_modules \
                  physical_only_cells} \
        -force_reference [get_object_name [get_lib_cells DCAP*]] \
        -hierarchy all \
        $OUTDIR/$DESIGN.lvs.v

    log EXPORT "LVS netlist export complete"

    # Write out the GDSII
    write_gds \
        -compress \
        -units 2000 \
        -output_pin all -long_names \
        -layer_map $TECHDIR/ndm/$LAYERMAPFILE \
        -merge_files [glob $TECHDIR/gds/*.gds*] \
        -merge_gds_top_cell $DESIGN \
        -allow_design_mismatch \
        -connect_below_cut_metal \
        -write_default_layers VIA1 \
        $OUTDIR/$DESIGN.gds.gz


    log EXPORT "GDS export complete"

    # Change names for post-layout simulations
    define_name_rules sdf -restricted "{}()\[\]/:.,"
    change_names -rules sdf -hierarchy

    # Write out the post-layout netlist
    write_verilog \
        -compress gzip \
        -top_module_first \
        -exclude { all_physical_cells pg_objects } \
        $OUTDIR/$DESIGN.v.gz

    log EXPORT "PL-sim netlist export complete"

    # Write out the sdf
    write_sdf \
        -include [list cell_delays net_delays timing_checks input_port_nets output_port_nets ] \
        -compress gzip \
        -corner tt_0p75v_25c \
        -mode func \
        $OUTDIR/$DESIGN.sdf.gz

    log EXPORT "SDF export complete"
}

##############
##  Export  ##
##############

eval_stage delivery {

    lassign [get_version_numbers [get_attribute [current_block] handoff_version]] major minor patch

    # Create the general design deliverable directory, if it does not exist yet
    file mkdir $DELIV_DIR/$DESIGN

    # Increase the patch version until we we found a non existing directory
    while {[file exists $DELIV_DIR/$DESIGN/v${major}.${minor}.${patch}]} {
        incr patch
    }

    log Deliver "Creating new $DESIGN deliverable for version v${major}.${minor}.${patch} \
        in $DELIV_DIR/$DESIGN/v${major}.${minor}.${patch}"

    # Create the deliverable directory for this version
    set delivery $DELIV_DIR/$DESIGN/v${major}.${minor}.${patch}
    file mkdir $delivery

    log Deliver "Creating new frame and abstract for $DESIGN"

    if {[get_attribute [current_block] has_editable_abstract]} {
        remove_abstract
    }
    save_block -as $DESIGN/delivery -force
    create_frame -block_all M11 -remove_non_pin_shapes {{M1 all}}

    set abstract_scenarios [get_object_name [get_scenarios {func*}]]
    set_scenario_status -none                  [all_scenarios]
    set_scenario_status -active          true  $abstract_scenarios
    set_scenario_status -setup           true  $abstract_scenarios
    set_scenario_status -hold            true  $abstract_scenarios
    set_scenario_status -leakage_power   true  $abstract_scenarios
    set_scenario_status -dynamic_power   true  $abstract_scenarios
    set_scenario_status -max_transition  true  $abstract_scenarios
    set_scenario_status -max_capacitance true  $abstract_scenarios
    set_scenario_status -min_capacitance true  $abstract_scenarios

    create_abstract

    log Deliver "Copying library $DESIGN to $delivery"

    # Copy the entire library and the block for the NDM handoff
    set new_lib $delivery/ndm/$DESIGN
    file mkdir [file dirname $new_lib]
    copy_lib -from $LIBDIR -to $new_lib -no_design
    copy_block -from $LIBDIR:$DESIGN/delivery.design -to $new_lib:$DESIGN/delivery.design
    copy_block -from $LIBDIR:$DESIGN/delivery.abstract -to $new_lib:$DESIGN/delivery.abstract
    copy_block -from $LIBDIR:$DESIGN/delivery.frame -to $new_lib:$DESIGN/delivery.frame
    save_lib $new_lib
    close_lib $new_lib
    current_block $LIBDIR:$DESIGN/delivery.design

    foreach file [glob -directory $OUTDIR *.v *.v.gz *.gds.gz *.sdf *.sdf.gz] {
        log Deliver "Copying $file to $delivery"
        file copy $file $delivery
    }

    log Deliver "Symlinking $delivery to $DELIV_DIR/$DESIGN/v${major}.${minor} \
        and $DELIV_DIR/$DESIGN/v${major}"

    # Create a symlink to the major version
    file delete $DELIV_DIR/$DESIGN/v${major}
    file delete $DELIV_DIR/$DESIGN/v${major}.${minor}
    file link $DELIV_DIR/$DESIGN/v${major} $delivery
    file link $DELIV_DIR/$DESIGN/v${major}.${minor} $delivery
}
