add wave -group "ACC_STALL" /tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_snitch/acc_qready_i \
/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_snitch/acc_qvalid_o \
/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_snitch/acc_stall;
add wave -group "MASTER_CC" {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_issue_prsp_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_qreq_valid_o} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_qreq_o} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_valid_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_ready_o}
add wave -group "SLAVE_CC" {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_issue_prsp_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_qreq_valid_o} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_qreq_o} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_valid_i} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_merge_interface/acc_intf_prsp_ready_o}
add wave -group "From_Snitch (Slave)" {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/acc_snitch_demux_qvalid} \
{sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/acc_snitch_demux};
add wave -group "At_Spatz (Slave)" {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/acc_issue_req};
add wave -group "In_Spatz (Slave)" {sim:/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[1]/genblk1/genblk1/i_spatz_cc/i_spatz/issue_req_i};


