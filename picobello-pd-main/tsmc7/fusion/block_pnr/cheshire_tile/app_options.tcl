# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Avoid multiple connections to macro pins for constant signals
set_app_options -name opt.tie_cell.max_fanout -value 1
