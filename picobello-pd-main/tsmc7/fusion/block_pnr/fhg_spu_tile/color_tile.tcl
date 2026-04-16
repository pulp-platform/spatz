# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

set_colors -color_name blue [get_flat_cells *router*]
set_colors -color_name blue [get_flat_cells *chimney*]

set_colors -color_name green [get_flat_cells *interconnect*]
set_colors -color_name green [get_flat_cells *tcdm*]

set_colors -color_name light_green [get_flat_cells *dma*]
set_colors -color_name light_green [get_flat_cells *gen_core_8*]

set_colors -color_name grey [get_flat_cells *peripheral*]
set_colors -color_name white [get_flat_cells *cache*]

set_colors -color_name red [get_flat_cells *gen_core_0*]
set_colors -color_name light_red [get_flat_cells *gen_core_1*]
set_colors -color_name orange [get_flat_cells *gen_core_2*]
set_colors -color_name light_orange [get_flat_cells *gen_core_3*]
set_colors -color_name purple [get_flat_cells *gen_core_4*]
set_colors -color_name pink [get_flat_cells *gen_core_5*]
set_colors -color_name brown [get_flat_cells *gen_core_6*]
set_colors -color_name yellow [get_flat_cells *gen_core_7*]