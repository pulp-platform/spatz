# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

open_lib $block_libfilename
open_block $block_refname

# TODO(fischeti): pass `FCDIR` variable
source $FCDIR/scripts/fp_utils.tcl

# legalize_placement
reset_placement

set DESIGN_HANDOFF $HANDOFF_DIR/latest/${block_refname_no_label}

# Source the PG strategies
source $DESIGN_HANDOFF/powergrid/pg_strategy.tcl

# Compile the PG
source $DESIGN_HANDOFF/powergrid/compile_pg.tcl

derive_pg_mask_constraint

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

write_gds \
    -compress \
    -units 2000 \
    -output_pin all -long_names \
    -layer_map $TECHDIR/ndm/$LAYERMAPFILE \
    -merge_files [glob $TECHDIR/gds/*.gds*] \
    -merge_gds_top_cell $block_refname_no_label \
    -allow_design_mismatch \
    -connect_below_cut_metal \
    -skip_unplaced_cells \
    -write_default_layers VIA1 \
    $OUTDIR/$block_refname_no_label.gds.gz

close_lib -force
