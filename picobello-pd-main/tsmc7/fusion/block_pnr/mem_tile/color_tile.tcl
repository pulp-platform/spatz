# Copyright (c) 2025 ETH Zurich.
# Riccardo Fiorani Gallotta <riccardo.fiorani3@unibo.it>

remove_colors [get_flat_cells *]

set_colors -color_name green [get_flat_cells *router*]
set_colors -color_name purple [get_flat_cells *chimney*]

set_colors -color_name orange [get_flat_cells *obi*]
set_colors -color_name blue [get_flat_cells *axi*]
