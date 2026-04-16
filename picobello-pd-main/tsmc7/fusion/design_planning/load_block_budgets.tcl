# Copyright (c) 2025 ETH Zurich.
# Tim Fischer <fischeti@iis.ee.ethz.ch>

open_block $block_libfilename:$block_refname
reopen_block -edit
# This is necessary to protect against unexpected closing of the block prior to saving
save_block

if {[file exists $block_budget_dir/$block_refname_no_label/top.tcl]} {
   puts "Block: $block_refname_no_label - Loading budgets"
   source -echo $block_budget_dir/$block_refname_no_label/top.tcl
} else {
   puts "RM-error: No budgets loaded for block: $block_refname_no_label"
}

save_lib
close_lib
