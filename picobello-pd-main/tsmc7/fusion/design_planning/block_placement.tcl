# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

source $FCDIR/scripts/fp_utils.tcl

set channel_size [expr 2 * $grid_step_size]
set block_offset [expr 2 * $channel_size]

# Source the block dimensions
source $DPDIR/block_dims.tcl

foreach block $block_refs {
  set_attribute [get_blocks -hierarchical $block] -name boundary -value [list {0 0} [list $width($block) $height($block)]]
}

set x_offset [expr $fp_x0 + $block_offset]
set y_offset [expr $fp_y0 + $block_offset]

for {set x 0} {$x < 7} {incr x} {
  for {set y 0} {$y < 4} {incr y} {

    # Left memory tile column
    if {$x == 0} {
      set cell [get_block_inst mem_tile $y]
      set cell_height [get_attribute $cell height]
      set cell_width [get_attribute $cell width]
      set_attribute $cell -name origin -value [list $x_offset [expr $y_offset + $y * ($cell_height + $channel_size)]]
    } elseif {$x >= 1 && $x <= 4} {
      set cell [get_block_inst cluster_tile [expr ($x - 1) * 4 + $y]]
      set cell_height [get_attribute $cell height]
      set cell_width [get_attribute $cell width]
      set_attribute $cell -name origin -value [list $x_offset [expr $y_offset + $y * ($cell_height + $channel_size)]]
    } elseif {$x == 5} {
      set cell [get_block_inst mem_tile [expr $y + 4]]
      set cell_height [get_attribute $cell height]
      set cell_width [get_attribute $cell width]
      set_attribute $cell -name origin -value [list $x_offset [expr $y_offset + $y * ($cell_height + $channel_size)]]
    } else {
      if {$y == 0} {
        set cell [get_block_inst fhg_spu_tile]
        set cell_height [get_attribute $cell height]
        set cell_width [get_attribute $cell width]
        set_attribute $cell -name origin -value [list $x_offset [expr $y_offset + $y * ($cell_height + $channel_size)]]
      } elseif {$y == 3} {
        set cell [get_block_inst cheshire_tile]
        set cell_height [get_attribute $cell height]
        set cell_width [get_attribute $cell width]
        set_attribute $cell -name origin -value [list $x_offset [expr $y_offset + $y * ($cell_height + $channel_size)]]
      }
    }
  }
  set x_offset [expr $x_offset + $channel_size + $cell_width]
}

set_attribute [get_all_block_insts] -name physical_status -value fixed

# Create keepout margins around the blocks
create_block_keepout [get_cells [get_all_block_insts]]
