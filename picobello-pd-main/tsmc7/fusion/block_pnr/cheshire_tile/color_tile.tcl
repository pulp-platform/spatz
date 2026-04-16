# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

remove_colors [get_flat_cells *]

set_colors -color_name green [get_flat_cells *router*]
set_colors -color_name purple [get_flat_cells *chimney*]

set_colors -color_name light_blue [get_flat_cells *nw_join*]
set_colors -color_name light_green [get_flat_cells *axi_xbar*]

set_colors -color_name red [get_flat_cells *cva6*]
set_colors -color_name orange [get_flat_cells *dma*]

set_colors -color_name grey [get_flat_cells *serial_link*]
set_colors -color_name blue [get_flat_cells *dbg*]
set_colors -color_name light_blue [get_flat_cells *atomic*]
set_colors -color_name blue [get_flat_cells *llc*]
