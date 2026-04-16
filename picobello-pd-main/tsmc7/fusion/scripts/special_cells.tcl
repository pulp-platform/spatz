# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>
#
# Set the purpose of the standard cells.

source $FCDIR/scripts/fp_utils.tcl
remove_boundary_cells
remove_tap_cells

####################
## Boundary Cells ##
####################

remove_boundary_cell_rules -all

set_boundary_cell_rules \
    -left_boundary_cell               {BOUNDARYLEFTBWP240*} \
    -right_boundary_cell              {BOUNDARYRIGHTBWP240*} \
    -bottom_boundary_cells            {BOUNDARYPROW*} \
    -top_boundary_cells               {BOUNDARYNROW*} \
    -top_right_outside_corner_cell    {BOUNDARYNCORNERBWP240*} \
    -top_left_outside_corner_cell     {BOUNDARYNCORNERBWP240*} \
    -bottom_right_outside_corner_cell {BOUNDARYPCORNERBWP240*} \
    -bottom_left_outside_corner_cell  {BOUNDARYPCORNERBWP240*} \
    -bottom_left_inside_corner_cells  {BOUNDARYPINCORNERBWP240*} \
    -bottom_right_inside_corner_cell  {BOUNDARYPINCORNERBWP240*} \
    -top_left_inside_corner_cell      {BOUNDARYNINCORNERBWP240*} \
    -top_right_inside_corner_cell     {BOUNDARYNINCORNERBWP240*} \
    -mirror_left_outside_corner_cell \
    -mirror_left_inside_corner_cell

report_boundary_cell_rules

compile_boundary_cells

check_boundary_cells

###############
## Tap Cells ##
###############

create_tap_cells \
    -lib_cell  [get_lib_cells */TAPCELLBWP240H8P57PDSVT] \
    -pattern stagger -distance 200 -offset 50 -skip_fixed_cells

# Connect all the newly inserted cells
connect_pg_net -automatic
