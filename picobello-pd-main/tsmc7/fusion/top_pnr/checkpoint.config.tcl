# Copyright (c) 2025 ETH Zurich.
# Manuel Eggimann <meggimman@iis.ee.ethz.ch>
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

# Extracts thte stages from a flow script, which are wrapped in eval_stage
proc extract_stages {file} {
  # Extract stages using a more direct approach
  set stages [regexp -all -inline {eval_stage\s+(\w+)} [read [open $file r]]]
  set stage_names [lmap {full stage} $stages {set stage}]
  return $stage_names
}

global checkpoint_list
set checkpoint_list [extract_stages "pnr.tcl"]

# A list of all checkpoints after which a new block label (copy of the your
# block) should be created. The checkpoint action defined below will save your
# block after checkpoint execution using the label name
# "<checkpoint_name>" where idx is the position of the checkpoint in
# the $checkpoint_list and <checkpoin_name is the name of your checkpoint. E.g.
# the block label created after the elaborate step would be "elaboration".
set save_blocks_on_checkpoints {
    floorplan
    io_bumps
    powergrid
}

# A list of checkpoints/wildcard patterns
# Insert a TCL pause (hit any key to proceed or # Ctrl+C to prematurely stop
# script exection) after the following stages (you can use wildcards* to specify
# multiple checkpoint without listing them all)
global PAUSE_AFTER_STAGE
if {![info exists PAUSE_AFTER_STAGE]} {set PAUSE_AFTER_STAGE {}}

# A single checkpint name
# Skip all checkpoints up to (and including) the following stage. Comment it out
# if you want to start from the beginning.
global FROM_STAGE
# set FROM_STAGE {some_stage}

# A list of checkpoints/wildcard patterns to be skipped. Use this variable if
# you want to skip a couple of specific checkpoints. There are two flows, which
# by default skip some of the stages:
# * NDM: Will do `import_library`, but skip all stages until `placement`
# * RTL: Will skip `import_library`, and create a new library from scratch with
#        `setup_library`, do `analysis`, `elaboration`, `constraints` and
#        `synthesis`
# The type of flow can be set in the Makefile by setting the variable
# `HANDOFF` to either `NDM` or `RTL` (the default is `NDM`). If you set `HANDOFF`
# to `CUSTOM`, you can also specify your own set of stages to skip.
global HANDOFF SKIP_STAGES DELIVER
if {![info exists HANDOFF]} {set HANDOFF ndm}
if {![info exists SKIP_STAGES]} {set SKIP_STAGES {}}
if {![info exists DELIVER]} {
    if {![info exists FROM_STAGE] || $FROM_STAGE ne "deliver"} {
        lappend SKIP_STAGES deliver
    }
}

switch $HANDOFF {
    ndm {
        lappend SKIP_STAGES {*}[list \
            setup_library \
            analysis \
            elaboration \
            constraints \
            synthesis \
            export
        ]
    }
    rtl {
        lappend SKIP_STAGES {*}[list \
            import_library
        ]
    }
    default {
        error "Unknown HANDOFF type: $HANDOFF. Please set it to either `ndm`, `rtl`."
    }
}

# A single checkpoint name
# Exit the script after the specified checkpoint. This is useful if you want to
# run a single stage (e.g. elaboration) for CI checks and then exit the script.
# This can be achieved by setting TO_STAGE from the Makefile e.g.
global TO_STAGE
if {![info exists TO_STAGE]} {set TO_STAGE [lindex $checkpoint_list end]}

# A single checkpoint name:
# If FROM_LABEL is not overriden, FROM_STAGE will load the block
# label of the corresponding checkpoint (e.g. after_elaborate.design). However
# you can specify your own label if needed.
#
# global FROM_LABEL
# set FROM_LABEL elaboration

# A list of checkpoints/checkpoint wildcard patterns:
# Send a noticiation mail once the specified checkpoints finished. (you can use wildcards* to specify
# multiple checkpoints without listing them all). Skipped checkpoints using the
# FROM_STAGE feature will not send a notification regardless the
# wildcards you use in this pattern (i.e. you can write "*" to be notified about
# every non-skipped checkpoint). Makes use of unix mailx to
# send the mails.
set notify_on_checkpoint_done {}

# A TCL list of IIS Unix usernames or mail addresses to notify. Use empty list
# to disable sending mails (or empty $notify_on_checkpoint_done list).
set notified_users {} ; # E.g. {meggiman some_other_dude@gmail.com}

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
    global DESIGN
    set label [string map -nocase {. _} [get_current_checkpoint -name]]
    save_block -as $DESIGN/$label -force
}

create_checkpoint_action load_from_label {
    global FROM_LABEL FROM_STAGE
    global DESIGN LIBDIR
    global checkpoint_list
    puts "Opening library $LIBDIR"
    open_lib $LIBDIR
    if ![info exists FROM_LABEL] {
        if ![info exists FROM_STAGE] {
            error "Cannot load from label because the FROM_LABEL variable is not set. Please specify the load label."
            exit
        } else {
            set idx [lsearch $checkpoint_list $FROM_STAGE]
            if {$idx <= 0} {
                error "Checkpoint $FROM_STAGE not found in checkpoint list. Please check the FROM_STAGE variable."
            }
            set prev_checkpoint [lindex $checkpoint_list [expr {$idx - 1}]]
            set block $DESIGN/$prev_checkpoint
        }
    } else {
        set block $DESIGN/$FROM_LABEL
    }
    puts "Restoring block from $block "
    open_block $block
    link_block
}

create_checkpoint_action skip_checkpoint {
    warn "Skipping step [get_current_checkpoint -name]"
}

create_checkpoint_action send_mail_when_done {
    set stage_name [get_current_checkpoint -name]
    if {[llength $notified_users] > 0} {
        exec echo "Stage $stage_name on [exec hostname] finished. You can find the current checkpoint history in the attachment." | mail -a checkpoint/checkpoint_history.rpt  -s "Fusion Compiler $stage_name Stage Finished" $notified_users
    }
}

##################################
## Associate Checkpoint Actions ##
##################################
# Enable saving of blocks on enabled checkpoints
associate_checkpoint_action -enable save_block -after $save_blocks_on_checkpoints

# Associate the send mail notification
associate_checkpoint_action -enable send_mail_when_done -after $notify_on_checkpoint_done

# Associate the pause action with the requested checkpoints
associate_checkpoint_action -enable pause -after $PAUSE_AFTER_STAGE

# Associate the exit action with the requested checkpoint
associate_checkpoint_action -enable exit -after $TO_STAGE

# Skip some of the checkpoints if enabled
associate_checkpoint_action -enable skip_checkpoint -replace $SKIP_STAGES
if [info exists FROM_STAGE] {
    # Skip all checkpoints up to the $FROM_STAGE
    set checkpoints_to_skip [lrange $checkpoint_list 0 [expr [lsearch $checkpoint_list $FROM_STAGE] - 1]]
    associate_checkpoint_action -enable skip_checkpoint -replace $checkpoints_to_skip

    # Disable block saving up to the $FROM_STAGE
    associate_checkpoint_action -disable save_block -after $checkpoints_to_skip
    associate_checkpoint_action -disable pause -after $checkpoints_to_skip
    associate_checkpoint_action -disable send_mail_when_done -after $checkpoints_to_skip

    # Associating pause action with selected checkpoints
    associate_checkpoint_action -enable load_from_label -before $FROM_STAGE
}

reset_checkpoints

proc reset_checkpoint_actions {} {
    foreach action {pause save_block load_from_label skip_checkpoint send_mail_when_done} {
        associate_checkpoint_action -disable $action -before *
        associate_checkpoint_action -disable $action -replace *
        associate_checkpoint_action -disable $action -after *
    }
    source checkpoint.config.tcl
}
