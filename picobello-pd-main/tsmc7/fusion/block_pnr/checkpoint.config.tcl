# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
# Manuel Eggimann <meggimann@iis.ee.ethz.ch>
#
# A brief Introduction on Checkpoints
#
# Fusion Compiler checkpoints, contrary to what the name might imply have
# nothing to do with saving a design state. The fusion compiler checkpoint
# system is just a TCL library to provide function hooks for different stages of
# your golden TCL implementation flow. I.e. you mark sections of your TCL script
# with a checkpoint name (by wrapping the TCL commands with eval_checkpoint
# <my_checkpoint_name> {<some tcl commands>}, see occamy_chip.tcl for example).
# The fusion compiler checkpoint TCL library then allows you to define actions
# (TCL commands) that are to be executoed 'before', 'after' or 'instead' of your
# TCL snippet wrapped in a checkpoint. These actions are typically defined in a
# separate 'checkpoint.config.tcl' file in your work directory. If such a file
# exists, fusion compiler will automatically source it at startup. The benefit
# of this system is that you can easily add customized behavior to your golden
# flow script without actually modifying the golden TCL. Besides checkpoint
# actions, the checkpoint system also allows you to define reports that should
# be created after certain checkpoints. These reports are then automatically
# collected and versioned in a checkpoint-<datetime> subdirectory without you
# having to manually control filenames and folder creation. Fusion compiler will
# also automatically generate a checkpoint_history.rpt (in a auto created subdir
# called 'checkpoint') where it logs runtime and memory consumption of each
# checkpoint executed in your current session. This is quite usefull if you want
# to know the runtimes of your individual PD stages.
#
# The whole checkpoint system is extensively documented in the Fusion Compiler
# User Guide chapter 1 ("Adding Changes to a Script With Checkpoints").
#
# The following 'checkpoint.config.tcl' makes use of this system to define a
# number of convenience actions and TCL variables to control
# - automatic saving of blocks after certain implementation steps
# - skipping stages of your PD flow and/or resuming from saved blocks
# - Pause script execution at user defined checkpoints
# - Notify users by e-mail on completion of user defined checkpoints
#
# All of these features can be conveniently controlled with the following set of
# TCL variabels

# Extracts the stages from a flow script, which are wrapped in eval_stage
proc extract_stages {file} {
  # Extract stages using a more direct approach
  set stages [regexp -all -inline {eval_stage\s+(\w+\.?\w+)} [read [open $file r]]]
  set stage_names [lmap {full stage} $stages {set stage}]
  return $stage_names
}

set stage_list [extract_stages "pnr.tcl"]

# Returns the main stage name (without substage) from a full stage name
# E.g. "elaboration.rtl_handoff" -> "elaboration"
proc get_mainstage {full_stage_name} {
    return [lindex [split $full_stage_name "."] 0]
}

# A list of all stages after which a new block label (copy of the your
# block) should be created. The checkpoint action defined below will save your
# block after checkpoint execution using the label name
# "<checkpoint_name>" where idx is the position of the checkpoint in
# the $stage_list and <checkpoint_name is the name of your checkpoint. E.g.
# the block label created after the elaborate step would be "elaboration".
set save_blocks_after_stages {
    floorplan
    powergrid
    placement
    cts
    route_auto
    route_opt
    finishing
}

# HANDOFF: There are three flows, which by default set additional `SKIP_STAGES` stages:
# * ndm: Will do `import_library`, but skip all stages until `placement`
# * rtl: Will skip `import_library`, and create a new library from scratch with
#        `setup_library`, do `analysis`, `elaboration`, `constraints` and
#        `synthesis`
# * bottom_up: Similar to `rtl` but will source manual constraints, floorplaning etc.,
# The type of flow can be set in the Makefile by setting the variable
# `HANDOFF` to either `ndm` or `rtl` or `bottom_up` (the default is `ndm`).
if {![info exists ::HANDOFF]} {set ::HANDOFF ndm}
if {![info exists ::SKIP_STAGES]} {set ::SKIP_STAGES {}}

switch $::HANDOFF {
    ndm {
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *.rtl_handoff]
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *.from_rtl]
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *.bottom_up]
    }
    rtl {
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *.ndm_handoff]
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *.bottom_up]
    }
    bottom_up {
        lappend ::SKIP_STAGES {*}[lsearch -all -inline $stage_list *handoff]
    }
    default {
        error "Unknown HANDOFF type: $::HANDOFF. Please set it to either `ndm`, `rtl` or `bottom_up`."
    }
}

# ECO_OPT: If not set, skip the eco_opt stage unless we are resuming from it
if {![info exists ::ECO_OPT] && (![info exists ::FROM_STAGE] || $::FROM_STAGE ne "eco_opt")} {
    lappend ::SKIP_STAGES {*}[list eco_opt]
}


# FROM_STAGE: A single checkpoint name:
# Skip all stages of the flow before `FROM_STAGE`.
if {[info exists ::FROM_STAGE]} {
    lappend ::SKIP_STAGES {*}[lrange $stage_list 0 [expr {[lsearch $stage_list ${::FROM_STAGE}*] - 1}]]
}

# PAUSE_AFTER_STAGE: A list of checkpoints/wildcard patterns
# Insert a TCL pause (hit any key to proceed or # Ctrl+C to prematurely stop
# script exection) after the following stages (you can use wildcards* to specify
# multiple checkpoint without listing them all)
if {![info exists ::PAUSE_AFTER_STAGE]} {set ::PAUSE_AFTER_STAGE {no_stage}}

# TO_STAGE: A single checkpoint name
# Exit the script after the specified checkpoint. This is useful if you want to
# run a single stage (e.g. elaboration) for CI checks and then exit the script.
# This can be achieved by setting TO_STAGE from the Makefile e.g.
# By default, we stop after the finishing stage.
if {![info exists ::TO_STAGE]} {set ::TO_STAGE finishing}
lappend ::SKIP_STAGES {*}[lrange $stage_list [expr {[lsearch $stage_list ${::TO_STAGE}*] + 1}] end]


# FROM_LABEL: A single checkpoint name:
# Loads the block from the specified label name. If not set, it will
# automatically be derived from the FROM_STAGE variable (i.e. it will load
# the block saved before the FROM_STAGE checkpoint)
if {[info exists ::FROM_STAGE] && ![info exists ::FROM_LABEL]} {
    set idx [lsearch $stage_list ${::FROM_STAGE}*]
    set ::FROM_LABEL [lindex [split [lindex $stage_list [expr {$idx - 1}]] .] 0]
}

# SKIP_STAGES: A list of checkpoints/wildcard patterns
# Skip the following stages (you can use wildcards* to specify multiple
# checkpoint without listing them all)
set ::SKIP_STAGES [lsort -unique $::SKIP_STAGES]
set active_stages [lminus $stage_list $::SKIP_STAGES]

###############################
## Create Checkpoint actions ##
###############################

create_checkpoint_action pause {
    pause
}

create_checkpoint_action exit {
    exit
}

create_checkpoint_action save_block {
    set label [get_mainstage [get_current_checkpoint -name]]
    save_block -hier -as $::DESIGN/$label -force
    save_lib -all
}

create_checkpoint_action load_from_label {
    puts "Opening library $::LIBDIR"
    open_lib $::LIBDIR
    puts "Restoring block from $::FROM_LABEL"
    open_block $::DESIGN/$::FROM_LABEL
    link_block
}

create_checkpoint_action skip_checkpoint {
    warn "Skipping step [get_current_checkpoint -name]"
}

##################################
## Associate Checkpoint Actions ##
##################################

# Enable saving of blocks on enabled checkpoints
associate_checkpoint_action -enable save_block -after [lmap stage $save_blocks_after_stages {set stage [lindex [lsearch -all -inline $active_stages $stage*] end]}]

# Associate the pause action with the requested checkpoints
associate_checkpoint_action -enable pause -after [lindex [lsearch -inline $active_stages ${::PAUSE_AFTER_STAGE}*] end]

# Associate the exit action with the requested checkpoint
associate_checkpoint_action -enable exit -after $::TO_STAGE

# Skip some of the checkpoints if enabled
associate_checkpoint_action -enable skip_checkpoint -replace $::SKIP_STAGES

# Load from block label if requested
if {[info exists ::FROM_STAGE]} {
    associate_checkpoint_action -enable load_from_label -before [lsearch -inline $active_stages ${::FROM_STAGE}*]
}

reset_checkpoints
