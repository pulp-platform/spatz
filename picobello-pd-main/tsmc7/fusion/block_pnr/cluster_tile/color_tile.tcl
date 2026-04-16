# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

remove_colors [get_flat_cells *]

set_colors -color_name green [get_flat_cells *router*]
set_colors -color_name purple [get_flat_cells *chimney*]

set_colors -color_name blue [get_flat_cells *interconnect*]
set_colors -color_name blue [get_flat_cells *tcdm*]

set_colors -color_name orange [get_flat_cells {*hwpe* *redmule*}]

set_colors -color_name grey [get_flat_cells *peripheral*]
set_colors -color_name white [get_flat_cells *cache*]

set_colors -color_name red [get_flat_cells *gen_core*]
set_colors -color_name light_orange [get_flat_cells *dma_inst64*]
set_colors -color_name light_green [get_flat_cells *axi*] -keep
