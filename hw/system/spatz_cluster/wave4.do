onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/clk_i}
add wave -noupdate /tb_bin/i_dut/cluster_probe
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_valid_i}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/spatz_req_ready_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vfu_rsp_valid_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/vfu_rsp_ready_i}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/op_i}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/vectorial_op_i}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/in_valid_i}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/in_ready_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/result_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/out_valid_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/out_ready_i}
add wave -noupdate -label nl_phase {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/tag_o.nl_phase}
add wave -noupdate -label nl_op_sel {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/tag_o.nl_op_sel}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/busy_o}
add wave -noupdate {/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_cc/i_spatz/i_vfu/gen_fpu/gen_fpnew[0]/i_fpu/opgrp_busy}
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 1} {9503000 ps} 1} {{Cursor 2} {9492000 ps} 1}
quietly wave cursor active 2
configure wave -namecolwidth 182
configure wave -valuecolwidth 100
configure wave -justifyvalue left
configure wave -signalnamewidth 1
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits ns
update
WaveRestoreZoom {9491346 ps} {9504334 ps}
