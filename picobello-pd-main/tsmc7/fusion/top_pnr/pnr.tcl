# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

##################
##  Initialize  ##
##################

set DESIGN picobello_chip
set FCDIR [file dirname [file dirname [file normalize [info script]]]]
source $FCDIR/scripts/utils.tcl
source $DPDIR/dp_utils.tcl
set_global_design_variables

# Use a lot of cores for the top-level PnR
set_host_options -max_cores 64

############################
##  Setup/Import Library  ##
############################

# Imports the library as an NDM from the design planning flow by copying it to
# a new run location, which differs from each run with a timestamp or optionally
# a user-defined run name that is passed as `RUN_NAME` argument.
eval_stage import_library {

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
    define_user_attribute -type string -classes design -name handoff_version
    set_attribute [current_block] handoff_version [file tail [file readlink $HANDOFF_DIR/latest]]

    # Save the library
    save_lib
}

# Creates an entirely new library from scratch.
# Skipped for the `NDM` flow
eval_stage setup_library {

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
            tcbn07_bwph240l8p57pd_mb_ulvt.ndm ]

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

    # Read in the parasitic files
    foreach RC {typical cworst_CCworst_T cbest_CCbest_T} {
    read_parasitic_tech \
        -tlup $TECHDIR/tlup/cln7_1p15m_1x1xa1ya5y2yy2yx2r_mim_ut-alrdl_${RC}.nxtgrd \
        -layermap $TECHDIR/tlup/PRTF_ICC2_N7_starrc_15M_1X1Xa1Ya5Y2Yy2Yx2R.13_3a.map \
        -name ${RC}
    }

    file mkdir $OUTDIR/svf
    set_svf $OUTDIR/svf/import.svf

    # Create the symlinks of the new lib (see `utils.tcl`)
    create_run_symlinks

    # Define the handoff version as a user attribute
    define_user_attribute -type string -classes design -name handoff_version
    set_attribute [current_block] handoff_version [file tail [file readlink $HANDOFF_DIR/latest]]

    # Save the library
    save_lib
}

###################
##  Analyze RTL  ##
###################

# Skipped for the `NDM` flow
eval_stage analysis {

    # Read SystemVerilog RTL attributes
    set_app_options -name hdlin.sverilog.enable_rtl_attributes -value true

    # Analyze the RTL from the open-source and internal PD repositories
    analyze_bender {rtl asic tsmc7 cva6 cv64a6_imafdcsclic_sv39 snitch_cluster pb_gen_rtl}
}

###################
##  Elaboration  ##
###################

# Skipped for the `NDM` flow
eval_stage elaboration {

    # Skip elaboration of subblocks since we replace their references with the imported
    # deliverables. Elaborating the full netlist here is a waste of time.
    set_app_options -name hdlin.elaborate.black_box -value $block_refs

    # We elaborate the entire chip, but only set the design
    # as the top module, to make sure that parameters are
    # passed from the top to the lower modules.
    elaborate picobello_chip
    set_top_module $DESIGN

    change_names -rules verilog -hierarchy
}

######################
##  Link Subblocks  ##
######################

# Link against subblock deliverables
eval_stage link_subblocks {

    lassign [get_version_numbers [get_attribute [current_block] handoff_version]] major minor patch

    foreach block_ref $block_refs {
        set_ref_libs -remove ${block_ref}
        if {[file exists $FINAL_RUNS_DIR/${block_ref}/ndm/${block_ref}]} {
            set BLOCK_LIB $FINAL_RUNS_DIR/${block_ref}/ndm/${block_ref}
            set LABEL delivery
            log Linking "block reference $block_ref v${major}.${minor} from delivery."
        } else {
            set BLOCK_LIB $HANDOFF_DIR/v${major}.${minor}/${block_ref}/ndm/${block_ref}
            set LABEL logic_opto
            warn "Using handoff block abstract since no delivery found for $block_ref v${major}.${minor}."
        }

        # Re-add the block reference library
        set_ref_libs -add $BLOCK_LIB

        # Set the reference
        set_reference [get_block_inst ${block_ref}] -to_block $BLOCK_LIB:${block_ref}/${LABEL}.abstract
    }

    link_block -force -rebind

    set_editability -from_level 1 -value false
    set_dont_touch [get_all_block_insts]

    if {[llength [get_corners estimated_corner -quiet]] != 0} {
        remove_corners estimated_corner
    }

    save_lib
}

###########################
##  Additional Settings  ##
###########################

eval_stage app_options {

    # Enable common app options
    source $FCDIR/scripts/app_options.tcl

    save_lib
}

#################
##  Floorplan  ##
#################

eval_stage floorplan {

    # Source the (updated) floorplan
    source $DPDIR/floorplan.tcl

    # Remove the PG regions as well
    remove_pg_regions -all

    # Redo the block placement
    source $DPDIR/block_placement.tcl

    # Place the macros
    source $FCDIR/top_pnr/place_macros.tcl

    # Create placement and PG keepout around the blocks
    create_block_keepouts [get_cells [get_all_block_inst]]

    # Place boundary and tap cells again (will remove existing ones)
    source $FCDIR/scripts/special_cells.tcl

    # Add Tie cells manually (otherwise, Fusion tries to connect them to the PG)
    add_tie_cells -objects [get_pins -of_objects [get_nets {*Logic0* *Logic1*}]] \
        -tie_high_lib_cells [get_lib_cells TIEH*] \
        -tie_low_lib_cells [get_lib_cells TIEL*]
}

###################
##  I/O & Bumps  ##
###################

eval_stage io_bumps {

    # Source the IO placement script again
    source $FCDIR/top_pnr/io.tcl

    # Place the bump array and perform RDL routing
    # source $FCDIR/top_pnr/bumps.tcl

    # Place metrology cells
    source $FCDIR/top_pnr/dtcd_cells.tcl
}

#################
##  Powergrid  ##
#################

eval_stage powergrid {

    # Source the PG strategies
    source $FCDIR/top_pnr/powergrid.tcl

    # Compile the PG
    set NO_PG_REMOVE 1
    source $FCDIR/top_pnr/compile_pg.tcl
}

#################
##  Synthesis  ##
#################

# Skipped for the `NDM` flow
eval_stage synthesis {

    # Enable Multibit
    set ENABLE_MULTIBIT true

    # Set the stage
    set_qor_strategy -stage compile_initial

    # TODO(fischeti): Can be removed here if set to false after initial `compile_fusion`
    # in the design planning flow.
    set_app_options -name compile.auto_floorplan.initialize -value false

    # Compile until the initial mapping step
    compile_fusion -to logic_opto
}

#################
##  Placement  ##
#################

eval_stage placement {

    # Enable Multibit
    set ENABLE_MULTIBIT true

    # Re-apply the top-level timing constraints
    source $FCDIR/../constraints/picobello_chip_reconstraint.sdc

    # Set the stage
    set_qor_strategy -stage compile_initial

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label logic_opto -report_group mapped

    # TODO(fischeti): Can be removed here if set to false after initial `compile_fusion`
    # in the design planning flow.
    set_app_options -name compile.auto_floorplan.initialize -value false

    # Physical synthesis
    compile_fusion -from initial_place -to initial_opto
    save_block -as $DESIGN/initial_opto

    # Set the stage
    set_qor_strategy -stage compile_final_place

    # Physical synthesis
    compile_fusion -from final_place -to final_opto
    save_block -as $DESIGN/final_opto

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label placed -report_group placed
}

############################
##  Clock Tree Synthesis  ##
############################

eval_stage cts {

    # Set the stage
    set_qor_strategy -stage cts -metric timing

    # Reconstrain again (if needed)
    source $FCDIR/../constraints/picobello_chip_reconstraint.sdc

    # Deactivate leakage and dnamic power analysis during CTS
    set leakage_scenarios [get_scenarios -filter active&&leakage_power]
    set_scenario_status -leakage_power false [get_object_name $leakage_scenarios]
    set dynamic_scenarios [get_scenarios -filter active&&dynamic_power]
    set_scenario_status -dynamic_power false [get_object_name $dynamic_scenarios]

    # Block-level clock tree propagation
    set_block_to_top_map -auto_clock connected
    report_block_to_top_map > $REPDIR/cts/pre_cts.report_block_to_top_map

    # Promote clock tree exceptions from blocks to top
    promote_clock_data -latency_offset -auto_clock connected
    promote_clock_data -balance_points -auto_clock connected
    #promote_clock_data -port_latency   -auto_clock connected

    # Ignore the sub-blocks' internal timing paths
    set_timing_paths_disabled_blocks -all_sub_blocks

    # Based on ARM recommendations
    set_app_options -name cts.common.default_max_transition -value 0.08
    # set_app_options -name ccd.adjust_io_clock_latency -value false
    # set_app_options -name cts.common.max_net_length -value 100

    # We don't want to update the I/O clocks since they should always have a network latency of 0
    foreach_in_collection mode [all_modes] {
        current_mode $mode
        set_latency_adjustment_options -exclude_clocks {jtag_clk slink_clk dram_slink_clk ref_clk rtc_clk}
    }

    # pre-CTS reports
    check_clock_tree > $REPDIR/cts/pre_cts.check_clock_tree
    report_clock_settings > $REPDIR/cts/pre_cts.report_clock_settings

    # Mark the clock trees, the NDR clock routing rules are defined in `app_options.tcl`
    mark_clock_trees -routing_rules

    # Synthesize the clock tree
    log "CTS" "build_clock"
    clock_opt -from build_clock -to build_clock

    save_block -as $DESIGN/cts_build_clock

    # Route the clock tree
    log "CTS" "route_clock"
    clock_opt -from route_clock -to route_clock

    save_block -as $DESIGN/cts_route_clock

    connect_pg_net

    # Propagate clocks and compute IO latencies for all scenarios for abstract creation
    synthesize_clock_trees -propagate_only
	compute_clock_latency -verbose

    # Set the stage
    set_qor_strategy -stage post_cts_opto

    # Final CTS round
    log "CTS" "final_opto"
    clock_opt -from final_opto -to final_opto
    connect_pg_net

    save_block -as $DESIGN/cts_final

    # Re-activate leakage and dnamic power analysis after CTS
    set_scenario_status -leakage_power true [get_object_name $leakage_scenarios]
    set_scenario_status -dynamic_power true [get_object_name $dynamic_scenarios]

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label cts -report_group cts

    save_lib
}

###############
##  Routing  ##
###############

eval_stage route_auto {

    # Insert tie cells (because Fusion does not do it automatically on the top-level)
    # source $FCDIR/top_pnr/insert_tie_cells.tcl

    # Antenna analysis and fixing
    source $FCDIR/scripts/PRTF_ICC2_N7_15M_Antenna.13_3a.tcl

    # Set the stage
    set_qor_strategy -stage route

    set_app_options -name time.si_enable_analysis -value false

    # Route
    log "Routing" "route_auto"
    route_auto -reuse_existing_global_route true

    save_block -as $DESIGN/route

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

    set_app_options -name time.si_enable_analysis -value false

    # Compute clock latency
    compute_clock_latency

    # Routing optimization
    log "Routing Optimization" "hyper_route_opt"
    hyper_route_opt

    save_block -as $DESIGN/hyper_route_opt

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label hyper_route_opt -report_group routed

    # TODO(fischeti): Check whether/when to add redundant vias
    # add_redundant_vias

    # Incremental route_detail for fixing routing DRCs
    log "Routing Optimization" "route_detail"
    route_detail -incremental true -initial_drc_from_input true
    connect_pg_net

    save_block -as $DESIGN/route_opt_final

    # Write out the QOR data
    write_qor_data -output $REPDIR/qor_data -label route_detail -report_group routed
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

#####################
##  Final patches  ##
#####################

eval_stage final_patches {

    create_net -ground ESD
    create_net -ground POCCTRL

    set esd_pins [get_pins -of_objects [get_cells -filter {is_io}] -filter "name==ESD"]
    set pocctrl_pins [get_pins -of_objects [get_cells -filter {is_io}] -filter "name==POCCTRL"]

    disconnect_net $esd_pins
    disconnect_net $pocctrl_pins

    connect_net -net ESD $esd_pins
    connect_net -net POCCTRL $pocctrl_pins

    # Create the bumps again
    source $FCDIR/top_pnr/bumps.tcl

    # Remove not used GPIO ports
    remove_ports [index_collection [sort_collection [get_ports pad_gpio_io*] name] 7 31]

    # Remove cut metals inserted over the blocks
    set redundant_cut_metals ""
    foreach_in_collection block_inst [get_cells [get_all_block_insts]] {
        set redundant_cut_metals [add_to_collection $redundant_cut_metals [get_shapes -quiet -filter "layer_name==CM2:metal_cut" -within $block_inst]]
    }
    remove_shapes $redundant_cut_metals
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
                  physical_only_cells \
                  corner_cells \
                  pad_spacer_cells \
                  flip_chip_pad} \
        -force_reference [get_object_name [get_lib_cells DCAP*]] \
        -hierarchy all \
        -switch_view_list {design} \
        $OUTDIR/$DESIGN.lvs.v

    log EXPORT "LVS netlist export complete"

    # Write out the GDSII
    lassign [get_version_numbers [get_attribute [current_block] handoff_version]] major minor patch

    set gds_merge_files [list]
    foreach block_ref $block_refs {
        set gds_dir [file dirname [file dirname [get_attribute [get_libs ${block_ref}] source_file_name]]]
        if {[file exists $gds_dir/${block_ref}.gds.gz]} {
            lappend gds_merge_files $gds_dir/${block_ref}.gds.gz
        } else {
            error "No GDS file found for block reference $block_ref v${major}.${minor}."
        }
    }

    # DZ updated the GDS output to support multiple rules per layer
    set_app_options -name file.gds.enable_multiple_rules_per_layer -value true
    write_gds \
        -compress \
        -units 2000 \
        -output_pin all -long_names \
        -layer_map $LAYERMAPFILE_CHIP_TOP \
        -merge_files [concat [glob $TECHDIR/gds/*.gds*] $gds_merge_files] \
        -merge_gds_top_cell $DESIGN \
        -merge_overwrite_conflicting_cell \
        -allow_design_mismatch \
        -skip_unplaced_cells \
        -connect_below_cut_metal \
        -write_default_layers VIA1 \
        $OUTDIR/${DESIGN}.gds.gz

    log EXPORT "GDS export complete"

    write_verilog \
        -compress gzip \
        -exclude {scalar_wire_declarations \
                  leaf_module_declarations \
                  pg_objects \
                  end_cap_cells \
                  well_tap_cells \
                  filler_cells \
                  pad_spacer_cells \
                  physical_only_cells \
                  cover_cells} \
        -hierarchy all \
        -switch_view_list {design} \
        $OUTDIR/$DESIGN.v

    log EXPORT "PL-sim netlist export complete"
}
