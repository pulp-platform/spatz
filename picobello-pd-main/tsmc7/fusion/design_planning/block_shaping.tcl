# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

remove_groups
if {[sizeof_collection [get_shaping_constraints -hierarchical -quiet]] > 0} {
    remove_shaping_constraints [get_shaping_constraints -hierarchical -quiet]
}

# This is the mini configuration with only 4 clusters
if {[sizeof_collection [get_cells -hierarchical -filter ref_name==cluster_tile]] == 4} {
    # Create groups for every column
    create_group -name cluster_left -shaping [get_cells {i_picobello_top/gen_clusters_0* i_picobello_top/gen_clusters_1*}]
    create_group -name cluster_middle -shaping [get_cells {i_picobello_top/gen_clusters_2* i_picobello_top/gen_clusters_3*}]
    create_group -name uncore_right -shaping [get_cells {i_picobello_top/i_cheshire_tile i_picobello_top/i_mem_tile}]

    # Create a group for the whole core
    create_group -name all_columns -shaping [get_groups -shaping {cluster_left cluster_middle uncore_right}]

    # Create the shaping constraints
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping cluster_left]
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping cluster_middle]
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping uncore_right]
    create_shaping_constraint -type array_layout -array_layout east [get_groups -shaping all_columns]

    # Try to decrease the utilziation of the clusters
    create_shaping_constraint -type utilization -max_util 0.5 [get_blocks -hierarchical cluster_tile]
} else  {

    # Create a group and shaping constraints for two columns of the memory tiles
    create_group -name mem_left -shaping
    create_group -name mem_right -shaping
    for {set y 0} {$y < 4} {incr y} {
        add_to_group mem_left [get_cells i_picobello_top/gen_memtile_${y}__i_mem_tile]
        add_to_group mem_right [get_cells i_picobello_top/gen_memtile_[expr $y + 4]__i_mem_tile]
    }
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping mem_left]
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping mem_right]

    # Create a group for every column of the cluster tiles
    for {set x 0} {$x < 4} {incr x} {
        create_group -name cluster_${x} -shaping
        for {set y 0} {$y < 4} {incr y} {
            add_to_group cluster_${x} [get_cells i_picobello_top/gen_clusters_[expr $x * 4 + $y]__i_cluster_tile]
        }
        create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping cluster_${x}]
    }

    # Create a group for the rest
    create_group -name uncore -shaping
    add_to_group uncore [get_cells i_picobello_top/i_fhg_spu_tile]
    add_to_group uncore [get_cells i_picobello_top/i_cheshire_tile]
    create_shaping_constraint -type array_layout -array_layout north [get_groups -shaping uncore]

    # Create a group for the whole core
    create_group -name all_columns -shaping [get_groups -shaping {mem_left cluster* mem_right uncore}]
    create_shaping_constraint -type array_layout -array_layout east [get_groups -shaping all_columns]

    # Try to decrease the utilziation of the clusters
    create_shaping_constraint -type utilization -target_util 0.5 [get_blocks -hierarchical cluster_tile]
    create_shaping_constraint -type utilization -target_util 0.8 [get_blocks -hierarchical mem_tile]
}

# Restrict the orientations of the blocks
foreach_in_collection inst [get_cells [get_all_block_insts]] {
    create_shaping_constraint -type allowable_orientation -allowable_orientation R0 $inst
}

# We need to set this, otherwise the constraints are not applied
set_app_options -name plan.shaping.import_tcl_shaping_constraints -value true

# Makes sure that the block edges are aligned to the grid. Otherwise, Fusion
# tends to do funny business and creates non-rectangular blocks.
set_app_options -name plan.shaping.snap_to_boundary_grid -value true

# Shape the blocks
shape_blocks -channels true
