# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

########################
##  Global Variables  ##
########################

set PDDIR   [file normalize [file join [file dirname [info script]] ../../..]]
set FCDIR   [file normalize [file join $PDDIR tsmc7/fusion]]
set DPDIR   [file normalize [file join $PDDIR tsmc7/fusion/design_planning]]
set TECHDIR [file normalize [file join $PDDIR tsmc7/technology]]
set CONSTS_DIR [file normalize [file join $PDDIR tsmc7/constraints]]
set HANDOFF_DIR [file join [file readlink /home/fischeti/picobello_handoff] handoff]
set DELIV_DIR [file join [file readlink /home/fischeti/picobello_handoff] deliverables]
set FINAL_RUNS_DIR [file join [file readlink /home/fischeti/picobello_handoff] final_runs]
set TECHFILE PRTF_ICC2_N7_15M_1X1Xa1Ya5Y2Yy2Yx2R_UTRDL_M1P57_M2P40_M3P44_M4P76_M5P76_M6P76_M7P76_M8P76_M9P76_H240.13_3a.tf
set LAYERMAPFILE PRTF_ICC2_N7_gdsout_15M_1X_h_1Xa_v_1Ya_h_5Y_vhvhv_2Yy2Yx2R.13_3a.map
set LAYERMAPFILE_CHIP_TOP /usr/pack/tsmc-7-kgf/dz/fusion/1P15M_1X_h_1Xa_v_1Ya_h_5Y_vhvhv_2Yy2Yx2R_UT-AlRDL_RF__CRN7_v1.0_2p1a/PRTF_ICC2_N7_gdsout_15M_1X_h_1Xa_v_1Ya_h_5Y_vhvhv_2Yy2Yx2R.13_3a.chipboundary.map
set ICV_RUNSET /usr/pack/tsmc-7-kgf/tsmc/util/drc/icv/drc/V14_1a/LOGIC_TopMr_DRC/ICVN7FF_15M_1X1Xa1Ya5Y2Yy2Yx2R_001.14_1a.encrypt
set SBOCVDIR  /usr/pack/tsmc-7-kgf/tsmc/digital/Front_End/SBOCV/CCS

lappend search_path $TECHDIR/ndm

########################
##  Design Variables  ##
########################

# Set design specific variables if `DESIGN` is set
if {[info exists DESIGN]} {
    set DESIGN_HANDOFF $HANDOFF_DIR/latest/$DESIGN
}

##########################
##  Global app options  ##
##########################

# Supress renaming warnings
suppress NDMUI-736

# Always morph design on save as (renames block before saving)
set_app_options -name design.morph_on_save_as -value true

# Use 3 significant digits to show picosecond-accurate timing figures
set_app_options -name shell.common.report_default_significant_digits -value 3


########################
##  Helper functions  ##
########################

# Analyze all benderized source files.
proc analyze_bender {{targets {rtl tsmc7}} {directory ""}} {
    global PDDIR search_path
    exec mkdir -p tmp

    # Construct Bender command
    set cmd {bender script synopsys}
    foreach target $targets {
        lappend cmd -t $target
    }

    if {$directory ne ""} {
        lappend cmd -d $directory
    } else {
        # By default, use the open-source RTL directory
        lappend cmd -d $PDDIR/..
    }

    # Execute Bender command
    exec {*}$cmd > tmp/analyze.tcl 2> /dev/stdin

    # Analyze sources
    source tmp/analyze.tcl
}

# Optionally sources a script for a design if it exists.
proc source_optional {script_path} {
    if {[file exists $script_path]} {
        source $script_path
        log SCRIPT "Sourced optional script: $script_path"
    } else {
        warn "Optional script not found: $script_path"
    }
}

# Sets the size_only attributes for tc_clk cells
proc set_tc_clk_size_only {} {
    set tc_clk_size_only_cells [get_cells -quiet -hierarchical -filter tc_clk_size_only]
    if {[sizeof_collection $tc_clk_size_only_cells] > 0} {
        log TC_CLK "Setting size_only attribute for [sizeof_collection $tc_clk_size_only_cells] tc_clk cells"
        set_size_only $tc_clk_size_only_cells -enable_constant_gate_removal -enable_constant_propagation -enable_unloaded_gate_removal -all_instances
    } else {
        log TC_CLK "No tc_clk cells found to set size_only attribute"
    }
}

###################
##  Constraints  ##
###################

# Create modes
proc create_modes {modes} {
    foreach mode $modes {create_mode $mode}
}

# Those are all the possible corners for TSMC7 we currently have
set default_corners {}
lappend default_corners [dict create process ssgnp VDD 0.675 temp -40 rc cworst_CCworst_T VDDIO 1.62]
lappend default_corners [dict create process ssgnp VDD 0.675 temp   0 rc cworst_CCworst_T VDDIO 1.62]
lappend default_corners [dict create process ssgnp VDD 0.675 temp 125 rc cworst_CCworst_T VDDIO 1.62]
lappend default_corners [dict create process ffgnp VDD 0.825 temp -40 rc cbest_CCbest_T   VDDIO 1.98]
lappend default_corners [dict create process ffgnp VDD 0.825 temp   0 rc cbest_CCbest_T   VDDIO 1.98]
lappend default_corners [dict create process ffgnp VDD 0.825 temp 125 rc cbest_CCbest_T   VDDIO 1.98]
lappend default_corners [dict create process tt    VDD 0.75  temp  25 rc typical          VDDIO 1.8 ]
lappend default_corners [dict create process tt    VDD 0.75  temp  85 rc typical          VDDIO 1.8 ]

# Create the corner based on the given parameters
proc create_corners {corner_dicts {with_io true}} {

    foreach corner_dict $corner_dicts {

        set vdd_str "[string map {"." "p"} [dict get $corner_dict VDD]]v"
        set temp_str "[string map {"-" "m" "." "p"} [dict get $corner_dict temp]]c"

        if {![dict exists $corner_dict VSS]} {dict set corner_dict VSS 0.0}
        if {![dict exists $corner_dict VSSIO]} {dict set corner_dict VSSIO 0.0}

        # Create corner
        set corner_name [dict get $corner_dict process]_${vdd_str}_${temp_str}
        create_corner $corner_name

        # Set voltages
        set_voltage -corner $corner_name -object_list [get_supply_nets VDD] [dict get $corner_dict VDD]
        set_voltage -corner $corner_name -object_list [get_supply_nets VSS] [dict get $corner_dict VSS]

        # Operating conditions
        if {!$with_io} {
            set_operating_conditions ${corner_name}_[dict get $corner_dict rc] \
                -library tcbn07_bwph240l8p57pd_base_ulvt${corner_name}_[dict get $corner_dict rc]_ccs
        } else {
            set_voltage [exec echo $VDDIO | tr p . | tr M -] -corner $corner -object_list [get_supply_nets VDDIO]
            set_voltage [exec echo $VSSIO | tr p . | tr M -] -corner $corner -object_list [get_supply_nets VSSIO]
            set vddio_str "[string map {"." "p"} [dict get $corner_dict VDDIO]]v"
            set_operating_conditions ${process}${vdd_str}${vddio_str}${temp_str}c \
                -library tphn7_18gpio_univ${process}${vdd_str}${vddio_str}${temp_str}c
        }

        # Parasitic parameters
        set_parasitic_parameters -early_spec [dict get $corner_dict rc] -late_spec [dict get $corner_dict rc]
    }
}

# Create all the scenarios. By default, creates all combinations of all modes and all corners.
proc create_scenarios {{modes "*"} {corners "*"}} {
    foreach mode [get_object_name [get_modes $modes]] {
        foreach corner [get_object_name [get_corners $corners]] {
            create_scenario -name ${mode}_${corner} -mode $mode -corner $corner
        }
    }
}

# Apply mode, corner and scenario specific constraints
# They are split into two files:
# 1) The `base.sdc` file, which contains the constraints that are applicable to all scenarios and corners
# 2) The `mode.sdc` e.g. `func.sdc` file, which contains mode-specific constraints such as
# clock period, case analysis for test pins etc.
proc apply_constraints {constraints_dir} {

    foreach mode [get_object_name [all_modes]] {
        current_mode $mode
        source [file join $constraints_dir ${mode}.sdc]

        foreach scenario [get_object_name [get_scenarios -mode $mode]] {
            current_scenario $scenario
            current_corner [get_attribute [current_scenario] corner]
            source [file join $constraints_dir base.sdc]
        }
    }
}

# Read in OCV data
proc read_pocv_data {} {
    remove_ocvm
    foreach corner [get_object_name [all_corners]] {
        set delay_type [expr {[string match "ffgnp*" $corner] ? "hold" : "setup"}]
        # Read in AOCV files and convert them to POCV
        foreach lib [get_object_name [get_libs tcbn07*]] {
            set ocv_file [glob $::SBOCVDIR/${lib}*/${corner}*/*/*${delay_type}*]
            log "OCV" "Reading AOCV file for library $lib and corner $corner: $ocv_file"
            read_ocvm -corner $corner $ocv_file
        }
        log "OCV" "Converting AOCV to POCV for corner $corner"
        convert_aocv_pocv -corners $corner
    }
}

####################
##  Host options  ##
####################

# Converts scratch paths to globally accessible `/usr/scratch/host_name/` paths
proc convert_scratch_path {path} {
    if {![string match "/scratch/*" $path]} { error "Invalid path: Must start with /scratch/" }
    set host_name [lindex [split [exec hostname] "."] 0];
    return [string map [list "/scratch/" "/usr/scratch/$host_name/"] $path]
}

# Set 16 cores by default (1 license)
set_host_options -max_cores 16

# Set 128 cores for ICV validator
set_host_options -max_cores 128 -target ICV

# This host configuration can distribute some tasks to the localhost,
# which is useful for block-level tasks without a lot of parallelism.
# Example usage:
# > some_distributable_task -args ... -host_options local_host
set_host_options -name local_host -submit_command "sh" -max_cores 16 -work_dir $DPDIR/work_dir

# A helper function which returns a list of hosts with the lowest load average
# and enough RAM.
# Example usage:
# > set_host_options -name larin_hosts4 -max_cores 16 [get_opt_larain_hosts 4 100]
# > some_distributable_task -args ... \
#     -host_options larin_hosts4 \
#     -work_dir [convert_scratch_path $curr_work_dir]
proc get_opt_larain_hosts {{num_hosts 4} {min_ram_gb 100}} {
    set min_ram [expr {$min_ram_gb * 1024}]
    set servers {}

    foreach line [split [exec cload larain] "\n"] {
        if {[regexp {^(larain([1-9]|10))\s+.*load average:\s+([0-9]+\.[0-9]+)} $line _ server_name load_avg]} {
            if {[catch {exec ssh $server_name "free -m | awk 'NR==2 {print int(\$7)}'"} available_ram]} {set available_ram 0}
            if {$available_ram >= $min_ram} {lappend servers [list $server_name $load_avg $available_ram]}
        }
    }

    return [lmap entry [lrange [lsort -real -index 1 $servers] 0 [expr {$num_hosts - 1}]] {lindex $entry 0}]
}

#####################
##  Checkpointing  ##
#####################

proc pause {{message "Hit Enter to continue ==> "}} {
    puts -nonewline $message
    flush stdout
    gets stdin
}

# This helper function wraps a stage with a checkpoint,
# logs the stage name and redirects the output to a log file
proc eval_stage {full_stage_name command_block} {

    # Extract the actual stage name without handoff suffix
    lassign [split $full_stage_name "."] stage handoff

    # Set report directory and log file name
    set num_dirs [llength [lsearch -not -all -inline [glob -nocomplain -directory $::REPDIR *] *${stage}*]]
    set ::STAGE_REPDIR [file join $::REPDIR "${num_dirs}_${stage}"]
    exec mkdir -p $::STAGE_REPDIR
    set logfile [file join $::STAGE_REPDIR $stage.log]

    eval_checkpoint $full_stage_name {
        redirect -tee -file $logfile {
            log_stage "$full_stage_name"

            # Execute the command block
            uplevel 1 $command_block

            # Summarize error/warning messages
            report_msg -summary
            print_message_info -ids * -summary
        }
    }
}

############
##  Runs  ##
############

# Sets the global design variables
proc set_global_design_variables {} {
    global REPDIR LIBDIR OUTDIR

    set TIMESTAMP [exec date +%Y%m%d_%H%M%S]

    # Use the timestamp as the run name if `RUN_NAME` is not set
    if {![info exists ::RUN_NAME]} {set ::RUN_NAME ${::DESIGN}_${TIMESTAMP}}

    set REPDIR $::FCDIR/reports/$::RUN_NAME
    set LIBDIR $::FCDIR/save/$::RUN_NAME
    set OUTDIR $::FCDIR/out/$::RUN_NAME
    exec mkdir -p $REPDIR
    exec mkdir -p $OUTDIR
    exec mkdir -p $::FCDIR/save

    # Log/store the design parameters if the log file does not exist yet
    set log_file [file join $REPDIR 0_init_params.log]
    if {![file exists $log_file]} {
        set log_data "DESIGN=$::DESIGN\n"
        append log_data "TIMESTAMP=$TIMESTAMP\n"
        append log_data "RUN_NAME=$::RUN_NAME\n"
        append log_data "REPDIR=$REPDIR\n"
        append log_data "LIBDIR=$LIBDIR\n"
        append log_data "OUTDIR=$OUTDIR\n"
        set fp [open $log_file "w"]
        puts $fp $log_data
        close $fp
    }
}

# Redefine variables from an open design
proc redefine_global_design_variables {} {
    global DESIGN REPDIR LIBDIR OUTDIR

    set DESIGN [get_attribute [current_block] name]

    set lib_name [get_object_name [current_lib]]
    set lib_path $::FCDIR/save/$lib_name

    # Resolve it is a symlink
    if {[file type $lib_path] eq "link"} {
        set lib_path [file normalize [file readlink $lib_path]]
    }

    set lib_name [file tail $lib_path]

    set OUTDIR $::FCDIR/out/$lib_name
    set REPDIR $::FCDIR/reports/$lib_name
    set LIBDIR $::FCDIR/save/$lib_name

    log LIB "Redefined global design variables:"
    log LIB "  DESIGN=$DESIGN"
    log LIB "  REPDIR=$REPDIR"
    log LIB "  LIBDIR=$LIBDIR"
    log LIB "  OUTDIR=$OUTDIR"
}

# Creates symlinks `my_design_latest` to the latest run,
# as well as `my_design_<RUN_DESC>` if `RUN_DESC` is set
proc create_run_symlinks {} {
    global FCDIR REPDIR OUTDIR LIBDIR
    global DESIGN RUN_NAME RUN_DESC

    # Create symlinks
    exec ln -sfn $REPDIR $FCDIR/reports/${DESIGN}_latest
    exec ln -sfn $LIBDIR $FCDIR/save/${DESIGN}_latest
    exec ln -sfn $OUTDIR $FCDIR/out/${DESIGN}_latest
    log LIB "Created symlinks $FCDIR/reports/${DESIGN}_latest for $REPDIR"
    log LIB "Created symlinks $FCDIR/save/${DESIGN}_latest for $LIBDIR"
    log LIB "Created symlinks $FCDIR/out/${DESIGN}_latest for $OUTDIR"

    if {[info exists RUN_DESC]} {
        exec ln -sfn $REPDIR $FCDIR/reports/${DESIGN}_${RUN_DESC}
        exec ln -sfn $LIBDIR $FCDIR/save/${DESIGN}_${RUN_DESC}
        exec ln -sfn $OUTDIR $FCDIR/out/${DESIGN}_${RUN_DESC}
        log LIB "Created symlinks $FCDIR/reports/${DESIGN}_${RUN_DESC} for $REPDIR"
        log LIB "Created symlinks $FCDIR/save/${DESIGN}_${RUN_DESC} for $LIBDIR"
        log LIB "Created symlinks $FCDIR/out/${DESIGN}_${RUN_DESC} for $OUTDIR"
    }
}

# Get major, minor, and patch version numbers from a version string
proc get_version_numbers {version} {
    # Match vX.Y.Z, vX.Y, vX, X.Y.Z, X.Y, X
    if {[regexp {^v?(\d+)(?:\.(\d+))?(?:\.(\d+))?} $version _ major minor patch]} {
        if {$minor eq ""} { set minor 0 }
        if {$patch eq ""} { set patch 0 }
        return [list $major $minor $patch]
    } else {
        error "Invalid version format: $version. Expected format: X[.Y[.Z]]"
    }
}

# Lists all the runs in the reports directory
proc get_prev_run_names {{type d}} {
  global FCDIR
  set runs [glob -nocomplain -types $type $FCDIR/reports/*]
  set runs [lmap run $runs {string map [list "$FCDIR/reports/" ""] $run}]
  return $runs
}

# Creates QoR comparison report of specified runs
proc create_qor_comparison {runs} {
  global FCDIR
  foreach run $runs {
    lappend run_dir $FCDIR/reports/$run/qor_data
  }
  compare_qor_data -run_locations $run_dir -run_names $runs -output $FCDIR/reports/compare_qor_data -force
  view_qor_data -location $FCDIR/reports/compare_qor_data
}

###############
##  Signoff  ##
###############

# Read in the DRC report from Calibre
proc import_calibre_drcs {} {
    global OUTDIR DESIGN
    set CALIBRE_OUTDIR [string map {"fusion" "calibre/drc"} $OUTDIR]

    remove_drc_error_data ${DESIGN}.calibre.err -force
    read_drc_error_file -drc_type calibre -error_data ${DESIGN}.calibre.err \
         -file ${CALIBRE_OUTDIR}/${DESIGN}.drc.results
    open_drc_error_data ${DESIGN}.calibre.err
}

# Initialize in-design ICValidator
proc init_icv {{enable_live_drc true}} {
    global ICV_RUNSET TECHDIR LAYERMAPFILE FCDIR

    # Set up all the runsets, layer map and GDS files
    set_app_options -name signoff.check_drc.runset -value $ICV_RUNSET
    set_app_options -name signoff.create_metal_fill.runset -value $ICV_RUNSET
    set_app_options -name signoff.physical.layer_map_file -value $TECHDIR/ndm/$LAYERMAPFILE
    set_app_options -name signoff.physical.merge_stream_files -value [glob $TECHDIR/gds/*.gds*]
    set_app_options -name signoff.check_drc.run_dir -value $FCDIR/icv_work_dir
    set_app_options -name signoff.check_drc.read_layout_views -value {*}
    # We need to write out the double patterned layers twice, once colored and once uncolored.
    # This is because some DRC rules are defined on the uncolored layers
    set_app_options -name signoff.check_drc.user_defined_options -value "-ndm_default_layers [get_attribute [get_layers VIA1] layer_number]"
    set_app_options -name signoff.check_drc.excluded_cell_types -value macro

    if {$enable_live_drc} {
        set_app_options -name signoff.check_drc_live.runset -value $ICV_RUNSET
        set_app_options -name signoff.check_drc_live.run_dir -value $FCDIR/live_icv_work_dir
        # Initialize ICV
        signoff_init_live_drc
    }
    # Save to disk is required as ICV reads from disk file instead of memory
    save_block
}

############
##  Math  ##
############

# Converts frequency values in unit Hertz to tech library time units
# (1ns in TSMC7 with PDK 1.0.3).
proc freq_to_period {freq_hz} {
    return [expr 1e9/$freq_hz] ; # Time unit is 1 ns
}

# greatest common divisor (GCD)
proc gcd {a b} {
    while {$b != 0} {
        set temp $b
        set b [expr {$a % $b}]
        set a $temp
    }
    return $a
}

# least common multiple (LCM) of a list
proc lcm {values} {
    if {[llength $values] == 0} { error "Input list is empty!" }

    # Scale by 1000 to avoid floating point errors and convert to integer
    set values [lmap val $values {expr {int($val * 1000)}}]

    set result [lindex $values 0]
    foreach val [lrange $values 1 end] {
        set result [expr {($result * $val) / [gcd $result $val]}]
    }
    return [expr {double($result) / 1000}]
}

###############
##  Logging  ##
###############

set CBLACK   [exec tput setaf 0]
set CRED     [exec tput setaf 1]
set CGREEN   [exec tput setaf 2]
set CYELLO   [exec tput setaf 3]
set CBLUE    [exec tput setaf 4]
set CMAGENTA [exec tput setaf 5]
set CCYAN    [exec tput setaf 6]
set CWHITE   [exec tput setaf 7]
set CBOLD    [exec tput bold]
set CRST     [exec tput sgr0]

proc log_stage {name} {
    global CBOLD
    global CWHITE
    global CGREEN
    global CRST
    global REPDIR
    puts ""
    puts "${CBOLD}${CWHITE}###${CRST}"
    puts ""
    puts "${CBOLD}${CWHITE}\[[exec date +%Y-%m-%d\ %H:%M:%S]\] Starting stage ${CGREEN}${name}${CRST}"
    puts ""
    puts "${CBOLD}${CWHITE}###${CRST}"
    puts ""
}

proc log {entity msg} {
    global CBOLD
    global CWHITE
    global CRST
    puts "${CBOLD}${CWHITE}\[${entity}\]: ${CRST}$msg"
}

proc warn {msg} {
    global CBOLD
    global CYELLO
    global CRST
    puts "${CBOLD}${CYELLO}${msg}${CRST}"
}

proc error {msg} {
    global CBOLD
    global CRED
    global CRST
    puts "${CBOLD}${CRED}${msg}${CRST}"
}

# Prints a list with newlines instead of spaces
proc puts_nl {msg} {
    if {[llength $msg] == 0} { return }
    puts [join $msg "\n"]
}

# Prints a collection with newlines instead of spaces
proc puts_nl_col {collection} {
    if {[sizeof_collection $collection] == 0} { return }
    puts [join [get_object_name $collection] "\n"]
}
