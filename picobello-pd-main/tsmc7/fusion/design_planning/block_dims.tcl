# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

set grid_step_size [lcm [list \
    [get_attribute [get_layers M5] pitch] \
    [get_attribute [get_site_defs unit] height] \
    [get_attribute [get_site_defs unit] width]]]

# The dimensions should be multiples of the grid step size
set width(cluster_tile) 679.44
set height(cluster_tile) 921.12
set width(mem_tile) 697.68
set height(mem_tile) 921.12
set width(fhg_spu_tile) 456
set height(fhg_spu_tile) 1190.16
set width(cheshire_tile) 456
set height(cheshire_tile) 921.12
