# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

##################
##  Initialize  ##
##################

set FCDIR [file dirname [file dirname [file normalize [info script]]]]
source $FCDIR/scripts/utils.tcl
source $DPDIR/dp_utils.tcl
set_global_design_variables

#####################
##  Setup Library  ##
#####################

eval_stage setup_library {

    # Open the library if it already exists
    if {[file exists $LIBDIR]} {
        open_lib $LIBDIR
    # Otherwise we create a new library
    } else {

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

        # Add I/O pad, FLL ndms
        set other_libs [list tphn7_18gpio_univ.ndm tsmc7_FLL.ndm]

        # Resolve path of reference libraries
        set ref_libs [lmap lib [concat $std_libs $mem_libs $other_libs] {file readlink $TECHDIR/ndm/$lib}]

        # Create the library
        create_lib $LIBDIR -technology $TECHFILE -ref_libs $ref_libs

        # Read in the parasitic files
        foreach RC {typical cworst_CCworst_T cbest_CCbest_T} {
            read_parasitic_tech \
                -tlup $TECHDIR/tlup/cln7_1p15m_1x1xa1ya5y2yy2yx2r_mim_ut-alrdl_${RC}.nxtgrd \
                -layermap $TECHDIR/tlup/PRTF_ICC2_N7_starrc_15M_1X1Xa1Ya5Y2Yy2Yx2R.13_3a.map \
                -name ${RC}
        }

        # We don't plan to check with Formality, so we disable the SVF
        set_svf -off

        # Save the library
        save_lib

        # Create the symlinks of the new lib (see `utils.tcl`)
        create_run_symlinks
    }
}

###################
##  Analyze RTL  ##
###################

eval_stage analysis {

    # Read SystemVerilog RTL attributes
    set_app_options -name hdlin.sverilog.enable_rtl_attributes -value true

    # Analyze the RTL from the open-source and internal PD repositories
    analyze_bender {rtl asic tsmc7 cva6 cv64a6_imafdchsclic_sv39_wb snitch_cluster pb_gen_rtl}
}

###################
##  Elaboration  ##
###################

eval_stage elaboration {

    # TODO(fischeti): Remove this once all black boxes are resolved
    # currently, the `unread` module throws an error
    set_attribute  -objects [get_mismatch_types empty_logic_module] -name action(default) -value ignore

    elaborate picobello_chip
    set_top_module $DESIGN

    change_names -rules verilog -hierarchy

    save_block -as $DESIGN/elaboration
    save_lib
}

###################
##  Constraints  ##
###################

eval_stage constraints {

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

    # Load the UPF file
    load_upf $PDDIR/tsmc7/constraints/$DESIGN.upf
    commit_upf

    # Set cell purposes
    source $FCDIR/scripts/set_lib_cell_purpose.tcl

    # Set the std cell placement rules
    foreach_in_collection lib [get_libs tcbn07*] {
        source $FCDIR/scripts/[get_object_name $lib]_par_icc2.tcl
    }

    # Set general app options
    source $FCDIR/scripts/app_options.tcl

    # Set size_only attributes for tc_clk cells, because RTL attributes are not set
    # even if `hdlin.sverilog.enable_rtl_attributes` is set to true!
    set_tc_clk_size_only

    # Create the scenarios and source the constraints
    create_modes {func fast slow}; # 1GHz, 1.2GHz, 100MHz modes
    create_corners $default_corners
    create_scenarios
    read_pocv_data

    # Defines all pins that should be constrained
    source ${CONSTRAINT_FOLDER}/picobello_instances.sdc

    # Actually source the constraints
    apply_constraints $CONSTRAINT_FOLDER

    # Activate the scenarios
    source $DPDIR/active_scenarios.tcl

    save_block -as $DESIGN/constraints
    save_lib
}

#####################
##  Commit Blocks  ##
#####################

eval_stage commit_blocks {

    # Add the instances to the budget options
    set_budget_options -add_blocks [get_all_block_insts]

    # Split the constraints
    split_constraints -nosplit -output $DPDIR/split -force

    # reset upf after split_constraints, `split_constraints` generates
    # new UPF files for each block (incl. top) that are sourced later
    reset_upf

    # Create new libraries for each subblock
    foreach tile_ref $block_refs {
        set block_lib $LIBDIR/$tile_ref
        copy_lib -to_lib $block_lib -no_designs
        set_attribute -object [get_lib $block_lib] -name use_hier_ref_libs -value true
    }

    save_lib -all

    # Commit all blocks
    foreach tile_ref $block_refs {
        commit_block -library $tile_ref $tile_ref
    }

    set top_block [current_block]
    set remove_blocks [remove_from_collection [get_blocks -hierarchical] [get_child_blocks]]

    # Remove the parent blocks from the child blocks
    foreach block_ref $block_refs {
        current_lib $block_ref
        foreach_in_collection block $remove_blocks {
            set_ref_libs -remove [get_attribute $block lib]
        }
        current_block $top_block
    }
    current_block $top_block

    # Add child block reference to parent block
    foreach inst [get_all_block_insts] {
        set ref_lib_name $LIBDIR/[get_attribute [get_cells $inst] ref_lib_name]
        # Check to see if ref lib is already linked in the case of MIBs
        if {[lsearch [get_attribute [get_libs $LIBDIR] ref_libs] $ref_lib_name] < 0} {
            set_ref_libs -library $LIBDIR -add $ref_lib_name
        }
    }

    # Set the file that contains the constraint mapping
    set_constraint_mapping_file $DPDIR/split/mapfile
    report_constraint_mapping_file

    # And re-load the constraints for all blocks
    load_block_constraints -all_blocks \
        -type SDC -type UPF -type CLKNET \
        -host_options local_host

    # Save all blocks
    save_block $DESIGN/constraints.design -hierarchical -as $DESIGN/commit_blocks
    save_lib -all
}

#######################
##  Initial Compile  ##
#######################

eval_stage initial_compile {

    # Enable Multibit
    set ENABLE_MULTIBIT true

    # Set QoR strategy
    set_qor_strategy -stage compile_initial -metric timing

    # Save libraries before running block scripts
    save_block -hierarchical -as $DESIGN/initial_compile
    save_lib -all

    # Run the block compilation script on all the sub blocks distributed
    run_block_script -script $DPDIR/compile_block.tcl \
        -blocks $block_refs \
        -host_options local_host

    # Set the child blocks as non-editable for the top-level compilation
    set_editability -from_level 1 -value false

    # Disable automatic floorplanning?? From the reference flow
    set_app_options -name compile.auto_floorplan.enable -value false

    # Only compile to initial_map without floorplan info
    compile_fusion -to initial_map

    # Revert the editability
    set_editability -from_level 1 -value true

    # Connect the PG net
    connect_pg_net -automatic -all_blocks

    # Create the initial block abstract views (w/o timing information yet)
    create_abstract -placement \
        -blocks [get_child_blocks] \
        -force_recreate \
        -host_options local_host

    # Save the block and library
    save_lib -all
}

#####################
##  Floorplanning  ##
#####################

eval_stage floorplan {

    # Create the floorplan
    source $DPDIR/floorplan.tcl

    # Place the I/O pads
    source $DPDIR/io.tcl
}

#####################
##  Block Shaping  ##
#####################

eval_stage block_shaping {

    # Check the design
    check_design -checks dp_pre_block_shaping

    # Run the block shaping
    source $DPDIR/block_shaping.tcl

    # Check the block shaping
    report_block_shaping -core_area_violations -overlap -flyline_crossing

    # Save block and libs
    save_block -hierarchical -as $DESIGN/block_shaping
    save_lib -all
}

eval_stage manual_block_shaping {

    # Shape and place the blocks with the custom script
    source $DPDIR/block_placement.tcl

    # Reset the placement before placing the special cells
    # This is usually done by `shape_blocks`, but for manual placement
    # we "need" to do it manually
    foreach block $block_refs {
    set_working_design -push [get_block_inst $block 0]
    reset_placement

    set_working_design -pop -level 0
    }

    if {[sizeof_collection [get_placement_blockages -quiet -hierarchical]] > 0} {
        # Remove the placement blockages
        remove_placement_blockages [get_placement_blockages -hierarchical]
    }

    # Create new labels for every block
    save_block -hierarchical -as $DESIGN/block_shaping
    save_lib -all
}

#########################
##  Initial placement  ##
#########################

eval_stage initial_place {

    # Check the design for macro placement
    check_design -checks dp_pre_macro_placement

    # We load the pin constraints for the blocks
    if {[file exists $DPDIR/pin_locations.tcl]} {
        read_pin_constraints -file_name $DPDIR/pin_locations.tcl
    } else {
        source $DPDIR/pin_constraints.tcl
    }

    # Source the macro placement script
    source $DPDIR/place_macros.tcl

    # Add boundary and tap cells before the placement
    source $FCDIR/scripts/special_cells.tcl

    # Running preliminary top level placement
    set_editability -from_level 1 -value false
    set_app_options -name place.coarse.fix_hard_macros -value true
    create_placement -floorplan
    set_editability -from_level 1 -value true

    # Run preliminary pin assignment
    # There is no timing information available yet, so we disable
    # timing-driven placement for the preliminary pin assignment
    set_app_options -name route.global.timing_driven -value false
    set_app_options -name plan.pins.fast_route -value true
    place_pins
    set_app_options -name plan.pins.fast_route -value false
    set_app_options -name route.global.timing_driven -value true

    # Save the libraries
    save_block -hierarchical -as $DESIGN/initial_place
    save_lib -all

    # Run the placement on all the blocks
    run_block_script -script $DPDIR/place_block.tcl \
        -blocks $block_refs \
        -host_options local_host \
        -work_dir $DPDIR/work_dir/place_block \
        -var_list [list [list FCDIR] [list $FCDIR]]

    # Running top-level macro placement
    set_editability -from_level 1 -value false
    create_placement -floorplan
    set_editability -from_level 1 -value true

    # Remove the preliminary pin assignment
    remove_block_pins

    # Fix all shaped blocks and macros
    set_attribute [get_hard_macros] physical_status fixed
    set_attribute [get_cells [get_all_block_insts]] physical_status fixed

    # Save block and libs
    save_lib -all
}

##################
##  Power Grid  ##
##################

eval_stage powergrid {

    # Check the design for power grid
    check_design -checks dp_pre_power_insertion

    # Set the PG creation mode
    set_pg_routing_mode -mode default

    # Set the PG strategy
    source $DPDIR/powergrid.tcl

    # Create a new label for the block power grid
    # Before running block-level PG compilation
    # save_block -hierarchical -as $DESIGN/powergrid
    save_lib -all

    # Characterize the powergrid for all the blocks
    characterize_block_pg \
        -output_directory block_pg \
        -compile_pg_script $DPDIR/compile_pg.tcl

    # Create the PG on all blocks
    set_constraint_mapping_file block_pg/pg_mapfile
    run_block_compile_pg -host_options local_host

    # Do a couple of checks
    check_pg_drc -ignore_std_cells
    check_pg_connectivity -check_std_cell_pins none
    check_mv_design

    # Save the libraries
    save_lib -all
}

############################
##  Clock Trunk Planning  ##
############################

eval_stage clock_trunk {

    # Place the clock pins manually, which is required for MIBs
    source $DPDIR/pin_constraints.tcl
    place_pins -pins $mib_block_clock_pins

    set_constraint_mapping_file $FCDIR/../constraints/ctp_mapfile

    # Pre-checks
    check_design -checks dp_pre_clock_trunk_planning

    # Set the blocks as editable for the clock trunk planning
    set_editability -from_level 1 -value true

    # Plan only the `soc_clk` in the design
    set clocks_plan soc_clk

    # Following app options are used to prevent pre-routes check during running block level CTP scripts.
    # This will create Error due to some cells not having legal location due to pnet check.
    set_app_options -name place.legalize.enable_prerouted_net_check -value false
    set_app_options -name opt.common.do_physical_checks -value never

    # Associate block and top clocks which are connected in the design,
    # and create new top-level clocks where connections cannot be found.
    set_block_to_top_map -auto_clock all

    # Run the clock trunk synthesis on each block
    synthesize_clock_trunk_endpoints \
        -blocks [get_child_blocks] \
        -clocks $clocks_plan \
        -host_options local_host

    # Use the "new" flow that can take multiple hierarchy levels into account when creating the clock trunks
    set_app_options -name plan.clock_trunk.flow_control -value optimize_subblocks
    set_app_options -name plan.clock_trunk.enable_new_flow -value true
    set_app_options -name top_level.optimize_subblocks -value all
    set_app_options -name cts.common.enable_subblock_optimization -value true
    set_app_options -name cts.common.user_instance_name_prefix -value TOP_CTP_

    remove_topological_constraints -all

    set_block_to_top_map -auto_clock all

    # Top-level sanity check
    check_design_for_clock_trunk_planning -clock [get_clocks $clocks_plan]

    # THO-CTP flow
    synthesize_clock_trunk_setup_hier_context -init -host_option localhost
    synthesize_clock_trunks -clock [get_clocks $clocks_plan] -from pin_constr_generation -to read_pin_constr_and_place_pins
    synthesize_clock_trunk_setup_hier_context -commit -host_option localhost

    # Write out some reports
    report_clock_trunk_qor -clock $clocks_plan

    save_lib -all
}

#####################
##  Pin Placement  ##
#####################

eval_stage pin_placement {

    # Source the pin constraints
    if {[file exists $DPDIR/pin_locations.tcl]} {
        read_pin_constraints -file_name $DPDIR/pin_locations.tcl
    } else {
        source $DPDIR/pin_constraints.tcl
    }

    # Run some pre-checks
    check_design -checks dp_pre_pin_placement

    # Check the alignment of MIBs (i.e. cluster, and L2 tile)
    check_mib_alignment

    # Block pin assignment.
    place_pins

    # Fix the pin placement
    foreach block_ref $block_refs {
        set_block_pins_status $block_ref fixed
    }

    # Check pin placement
    check_pin_placement \
        -alignment true -pre_route true \
        -sides true -stacking true \
        -pin_spacing true -layers true

    report_pin_placement

    save_block -hierarchical -as $DESIGN/pin_placement
    save_lib -all
}

###################
##  Top Compile  ##
###################

eval_stage top_compile {

    # create abstract and load the block constraints for bottom level blocks
    create_abstract \
        -timing_level full_interface \
        -blocks [get_child_blocks] \
        -read_only \
        -host_options local_host

    save_lib -all

    set_app_options -name compile.auto_floorplan.enable -value false

    # Run `logic_opto` on top level
    set_editability -from_level 1 -value false
    compile_fusion -from logic_opto -to logic_opto
    set_editability -from_level 1 -value true

    connect_pg_net -automatic -all_blocks

    save_block -as $DESIGN/top_compile
    save_lib -all
}

########################
##  Timing Budgeting  ##
########################

eval_stage timing_budget {

    # Do some pre-checks
    check_design -checks dp_pre_create_timing_abstract

    # Create `estimate_timing` abstracts of all child blocks
    create_abstract -estimate_timing \
        -all_blocks \
        -timing_level full_interface \
        -host_options local_host

    report_abstracts

    # Some more pre-checks
    check_design -checks dp_pre_timing_estimation

    # Estimate the timing of the top-level design
    estimate_timing -host_options local_host

    file mkdir $REPDIR/timing_budget
    redirect -file $REPDIR/timing_budget/post_estimated_timing.rpt     {report_timing -corner estimated_corner -mode [all_modes]}
    redirect -file $REPDIR/timing_budget/post_estimated_timing.qor     {report_qor    -corner estimated_corner}
    redirect -file $REPDIR/timing_budget/post_estimated_timing.qor.sum {report_qor    -summary}

    # Pre-budgeting checks
    check_design -checks dp_pre_budgeting

    # Add blocks to budgets
    set_budget_options -add_blocks [get_all_block_insts]

    # Compute the block budgets
    compute_budget_constraints -setup_delay -boundary -latency_targets actual -balance true

    # Write out the budgets
    write_budgets -output $DPDIR/block_budgets -force -nosplit

    redirect -file $REPDIR/timing_budget/report_budget.latency {report_budget -latency}
    report_budget -html_dir $REPDIR/timing_budget/budget.html

    # Write out the budget constraints
    write_script -include budget -force -output $REPDIR/timing_budget/budget_constraints

    save_lib -all

    # Load the budgets into the blocks
    run_block_script \
        -script $DPDIR/load_block_budgets.tcl \
        -blocks $block_refs \
        -host_options local_host -force \
        -work_dir $DPDIR/work_dir/load_block_budgets \
        -var_list [list [list block_budget_dir] [list $DPDIR/block_budgets]]

    # Create the read-only timing abstract of the sub blocks, preserving the full interface timing
    create_abstract \
        -timing_level full_interface \
        -blocks [get_child_blocks] \
        -host_options local_host

    save_block -as $DESIGN/timing_budget
    save_lib -all
}

##############
##  Export  ##
##############

eval_stage export {

    # Change names and save block
    change_names -rules verilog -hierarchy
    save_block -as $DESIGN/export

    set TOP_OUTDIR $OUTDIR/$DESIGN
    file mkdir $TOP_OUTDIR

    # Save UPF file
    save_upf $TOP_OUTDIR/$DESIGN.upf

    # writes multiple files to the specified directory.
    # It writes mode_{mode_name}.tcl for mode specific info, corner_{corner_name}.tcl for corner specific info,
    # design.tcl for non-mode or corner specific info, cts.tcl for cts options and top.tcl that sources all scripts.
    write_script -force -output $TOP_OUTDIR/scripts

    # Write out SDC files
    file mkdir $TOP_OUTDIR/sdc
    foreach_in_collection scn [all_scenarios] {
        current_scenario $scn
        set scn_name [get_object_name $scn]
        write_sdc -compress gzip -output $TOP_OUTDIR/sdc/${scn_name}.sdc
    }

    # Write out floorplan and DEF files
    write_floorplan \
        -format icc2 \
        -def_version 5.8 \
        -force \
        -read_def_options {-add_def_only_objects {all} -skip_pg_net_connections} \
        -exclude {scan_chains fills pg_metal_fills routing_rules} \
        -net_types {power ground} \
        -include_physical_status {fixed locked} \
        -output $TOP_OUTDIR/floorplan

    file mkdir $TOP_OUTDIR/netlist

    # Write out the netlist for post-synthesis simulation
    write_verilog \
    -compress gzip \
    -exclude { \
        scalar_wire_declarations \
        leaf_module_declarations \
        pg_objects \
        end_cap_cells \
        well_tap_cells \
        filler_cells \
        pad_spacer_cells \
        physical_only_cells \
        cover_cells
    } -hierarchy all \
    -top_module_first \
    $TOP_OUTDIR/netlist/$DESIGN.v

    # Write out the preferred pin locations
    write_pin_constraints \
        -file_name $TOP_OUTDIR/pin_locations.tcl.tcl \
        -physical_pin_constraint {side | offset | layer} \
        -from_existing_pins

    # Also update the tracked pin locations file
    file delete $DPDIR/pin_locations.tcl
    file copy $TOP_OUTDIR/pin_locations.tcl.tcl $DPDIR/pin_locations.tcl

    # Also write out the `split`, `block budgets` and `block_pg` directory for reference
    file copy -force $DPDIR/split $TOP_OUTDIR/split
    file copy -force $DPDIR/block_budgets $TOP_OUTDIR/block_budgets
    file copy -force $DPDIR/block_pg $TOP_OUTDIR/block_pg

    # Remove estimated corner from top-level.
    if {[llength [get_corners estimated_corner -quiet]] != 0} {
        remove_corners estimated_corner
    }

    # Remove constraint mapping file.
    set_constraint_mapping_file -reset

    save_lib

    # Write out (the same) information for all the blocks
    run_block_script \
        -script $DPDIR/block_export.tcl \
        -blocks $block_refs \
        -host_options local_host \
        -work_dir $DPDIR/work_dir/block_export \
        -var_list [list [list OUTDIR] [list $OUTDIR] [list DPDIR] [list $DPDIR]]

    # Set editablity false for all blocks
    set_editability -value false -blocks [get_block -hier]
    save_block
    report_editability -blocks [get_block -hier]

    # Copy the entire library and the block for the NDM handoff
    file mkdir $TOP_OUTDIR/ndm
    copy_lib -from [current_lib] -to $TOP_OUTDIR/ndm/$DESIGN -no_design
    copy_block -from [current_block] -to $TOP_OUTDIR/ndm/$DESIGN:$DESIGN/logic_opto.design
    save_lib $TOP_OUTDIR/ndm/$DESIGN
    close_lib $TOP_OUTDIR/ndm/$DESIGN
}
