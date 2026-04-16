# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

open_block -read $block_libfilename:$block_refname

set BLOCK_OUTDIR $OUTDIR/$block_refname_no_label
set BLOCK_LIBDIR $BLOCK_OUTDIR/ndm/$block_refname_no_label
file delete -force $BLOCK_OUTDIR
file mkdir $BLOCK_OUTDIR

# Save the UPF
save_upf $BLOCK_OUTDIR/$block_refname_no_label.upf

# writes multiple files to the specified directory.
# It writes mode_{mode_name}.tcl for mode specific info, corner_{corner_name}.tcl for corner specific info,
# design.tcl for non-mode or corner specific info, cts.tcl for cts options and top.tcl that sources all scripts.
write_script -force -output $BLOCK_OUTDIR/scripts -exclude clock_latency

# Write out the floorplan and the DEF
write_floorplan -force \
  -output $BLOCK_OUTDIR/floorplan/bare \
  -include {die_area pins rows tracks routing_directions}

write_floorplan -force \
  -output $BLOCK_OUTDIR/floorplan/full \
  -read_def_options {-add_def_only_objects {all} -skip_pg_net_connections} \
  -exclude {scan_chains fills pg_metal_fills routing_rules} \
  -net_types {power ground} \
  -include_physical_status {fixed locked}

file mkdir $BLOCK_OUTDIR/netlist

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
  $BLOCK_OUTDIR/netlist/$block_refname_no_label.v

file mkdir $BLOCK_OUTDIR/powergrid
file copy -force $DPDIR/block_pg/${block_refname_no_label}_pg.tcl $BLOCK_OUTDIR/powergrid/pg_strategy.tcl
file copy -force $DPDIR/block_pg/copy_compile_pg.tcl $BLOCK_OUTDIR/powergrid/compile_pg.tcl

# Copy the entire library and the block for the NDM handoff
file mkdir $BLOCK_OUTDIR/ndm
copy_lib -from $block_libfilename -to $BLOCK_LIBDIR -no_design
copy_block -from $block_libfilename:$block_refname.design -to $BLOCK_LIBDIR:$block_refname_no_label/logic_opto.design
copy_block -from $block_libfilename:$block_refname.abstract -to $BLOCK_LIBDIR:$block_refname_no_label/logic_opto.abstract
copy_block -from $block_libfilename:$block_refname.frame -to $BLOCK_LIBDIR:$block_refname_no_label/logic_opto.frame
save_lib $BLOCK_LIBDIR

close_blocks -force
close_lib -all
