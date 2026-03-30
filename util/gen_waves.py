#!/usr/bin/env python3

def get_number(value, reg):
    """Compute encoded number from bank index and register group."""
    MSB = value >> 1
    LSB = value & 0x1
    output = (MSB << 2) | (reg << 1) | LSB
    return output

def generate_mrf_waves(n_lanes, n_regs, n_banks, output_file="mrf_waves.do"):
    """Generate virtual install and waveform commands for MRF signals."""

    create_virtual    = "quietly virtual function -install "
    instance_name     = "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_regfile"
    open_list_signals = " -env /tb_bin { &{"

    command_virtual = ""

    # --- Build command_virtual ---
    for ii in range(n_lanes):
        for jj in range(n_banks):
            for kk in range(int(n_lanes * n_regs / n_banks)):
                command_virtual += create_virtual + '{' + instance_name + '}' + open_list_signals
                for zz in range(n_lanes):
                    signal = f"/mem_q[{ii}][{jj}][{kk}][{n_lanes - 1 - zz}]"
                    command_virtual += instance_name + signal + ", "
                name = f"m{get_number(jj, (kk - (kk % n_lanes)) // n_lanes)}_{kk % n_lanes}_{ii}"
                if command_virtual.endswith(", "):
                    command_virtual = command_virtual[:-2]
                command_virtual += " }} " 
                command_virtual += f"{name}\n"

    # --- Build command_wave ---
    add_wave_MRF = "add wave -noupdate -group MRF "
    command_wave = ""
    command_wave += add_wave_MRF + "{" + instance_name + "/clk_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/rst_ni}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/testmode_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/raddr_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/rrowaddr_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/rdata_o}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/waddr_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/wrowaddr_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/wdata_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/wbe_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/we_i}\n"
    command_wave += (
        add_wave_MRF
        + "-subitemconfig {{"
        + instance_name
        + "/mem_q[3]}  {"
        + instance_name
        + "/mem_q[3][3]} } {"
        + instance_name
        + "/mem_q}\n"
    )

    for ii in range(n_regs):
        for jj in range(n_lanes):
            for kk in range(n_lanes):
                command_wave += add_wave_MRF
                command_wave += f"-group m{ii} {{{instance_name}/m{ii}_{jj}_{kk}}}\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_virtual)
        f.write(command_wave)

    print(f"✅ File '{output_file}' generated successfully.")
def generate_quad_mrf(n_banks, output_file="quad_mrf_sc.do"):
  command_wave = ""
  instance_name = "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_mrf/"
  add_wave      = "add wave -noupdate -group {Quad MRF Controller} "
  grp_issue     = "-group {Issue Request} {" 
  grp_ports_fu  = "-group {Ports FU} {" 
  grp_ports_rf  = "-group {Ports RF} {" 
  grp_int       = "-group Scoreboard {" 

  command_wave += add_wave + "{" + instance_name + "clk_i}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_valid_i}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_ready_o}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_data_i}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_valid}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_ready}\n"
  command_wave += add_wave + grp_issue + instance_name + "sb_req_q}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "addr_i}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "rowaddr_i}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "rdata_o}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "valid_o}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "last_i}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "is_zero_i}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "ready_i}\n"
  command_wave += add_wave + grp_ports_fu + instance_name + "wdata_i}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "req}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "addr}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "we}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "be}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "rdata}\n"
  command_wave += add_wave + grp_ports_rf + instance_name + "wdata}\n"
  command_wave += add_wave + grp_int + instance_name + "eu_wr}\n"
  command_wave += add_wave + grp_int + instance_name + "eu_port_id}\n"
  command_wave += add_wave + grp_int + instance_name + "eu_valid}\n"
  command_wave += add_wave + grp_int + instance_name + "eu_ready}\n"

  gen_sb   = "gen_superbanks["
  gen_sign = "]/gen_sb_signals["
  gen_sc   = "]/i_scoreboard"
  
    
  for j in range(n_banks):
    for i in range(4):
        command_wave += add_wave + "-group {Scoreboard State} {" + instance_name + gen_sb + f"{i}" + gen_sign + f"{j}" + gen_sc + "/sb_current_q}\n"
  
   # --- Write to file ---
  with open(output_file, "w") as f:
      f.write(command_wave)
  print(f"✅ File '{output_file}' generated successfully.")
def generate_quad_lsu(port):
  output_file   = f"quad_lsu_{port}.do"
  command_wave  = ""
  instance_name = f"/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/gen_port[{port}]/i_lsu/"
  add_wave      = "add wave -noupdate -group {Quad LSU} "
  grp_mem       = "-group {Memory Ports}" 
  grp_rd        = "-group {Read Port}" 
  grp_wr        = "-group {Write Port}" 
  grp_addr      = "-group Address" 
  grp_st_fifo   = "-group {Store FIFO}" 
  grp_ld_fifo   = "-group {Load FIFO}" 
  grp_cnt       = "-group Counters" 
  grp_finish    = "-group Finish" 
  grp_int       = "-group Internal" 

  command_wave += add_wave + "{" + instance_name + "clk_i}\n"
  command_wave += add_wave + "{" + instance_name + "rst_ni}\n"
  command_wave += add_wave + "-color Magenta {" + instance_name + "state_q}\n"
  command_wave += add_wave + "{" + instance_name + "instr_i}\n"
  command_wave += add_wave + "{" + instance_name + "instr_valid_i}\n"
  command_wave += add_wave + "{" + instance_name + "instr_ready_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_req_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_addr_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_we_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_be_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_wdata_o}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_gnt_i}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_rvalid_i}\n"
  command_wave += add_wave + grp_mem + " {" + instance_name + "data_rdata_i}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "waddr_o}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "wrowaddr_o}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "wdata_o}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "we_o}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "wlast_o}\n"
  command_wave += add_wave + grp_wr + " {" + instance_name + "wready_i}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "raddr_o}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rrowaddr_o}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rdata_i}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rdata_valid_i}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rdata_ready_o}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rlast_o}\n"
  command_wave += add_wave + grp_rd + " {" + instance_name + "rzero_o}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "base_addr}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "base_addr_d}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "base_addr_q}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "stride_d}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "stride_q}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "stride}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "addr_incr}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "mem_addr}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "mem_row_shift}\n"
  command_wave += add_wave + grp_addr + " {" + instance_name + "mem_col_shift}\n"
  command_wave += add_wave + grp_ld_fifo + " {" + instance_name + "load_fifo_*}\n"
  command_wave += add_wave + grp_st_fifo + " {" + instance_name + "store_fifo_*}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_done}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_done}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_col_cnt_d}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_col_cnt_q}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_row_cnt_d}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_row_cnt_q}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_cnt_d}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_cnt_q}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_row_cnt_d}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_row_cnt_q}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_col_cnt_d}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_cnt_upd}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_col_cnt_tc}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "rf_row_cnt_tc}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_cnt_upd}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_col_cnt_tc}\n"
  command_wave += add_wave + grp_cnt + " {" + instance_name + "mem_col_cnt_q}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_valid_o}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_ready_i}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_id_o}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_stall}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_d}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_q}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_id_d}\n"
  command_wave += add_wave + grp_finish + " {" + instance_name + "commit_id_q}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_waddr}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_wrowaddr}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_wdata}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_we}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_wlast}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_wready}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_raddr}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_rrowaddr}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_rdata}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_rdata_valid}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_rdata_ready}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "sp_rlast}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "data_rvalid}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "data_rready}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "data_wvalid}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mask_rvalid_q}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mask_rvalid_d}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "state_d}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "back_id_q}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "back_id_d}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "data_mask}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "write_q}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "write_d}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "rf_write}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mem_write}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mreg_d}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mreg_q}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mreg}\n"
  command_wave += add_wave + grp_int + " {" + instance_name + "mem_en}\n"

   # --- Write to file ---
  with open(output_file, "w") as f:
      f.write(command_wave)
  print(f"✅ File '{output_file}' generated successfully.")
def generate_quad_signals(output_file="quad_instr.do"):
    command_wave = ""
    instance_name  = "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/"
    add_wave       = "add wave -noupdate "
    grp_ctrl_issue = "-group {Quad Controller} -group Issue "
    grp_ctrl_sb    = "-group {Quad Controller} -group Scoreboard "
    grp_ctrl_disp  = "-group {Quad Controller} -group Dispatch "
    grp_compress   = "-group XIF -group Compressed "
    grp_issue      = "-group XIF -group Issue "
    grp_commit     = "-group XIF -group Commit "
    grp_memory     = "-group XIF -group Memory "
    grp_xif_res    = "-group XIF -group Result "

    command_wave += add_wave + "{" + instance_name + "clk_i}\n"
    command_wave += add_wave + "-color Magenta {"  + instance_name + "x_issue_valid_i}\n"
    command_wave += add_wave + "-color Magenta {"  + instance_name + "x_issue_ready_o}\n"
    command_wave += add_wave + "-color Magenta {"  + instance_name + "x_issue_req_i}\n"


    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/clk_i}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/instr_valid_i}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/instr_i}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/instr_ready_o}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/dec_valid}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/dec_instr}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/instr}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/issue_ready}\n"
    command_wave += add_wave + grp_ctrl_issue + "{" + instance_name + "i_controller/issue_valid}\n"
    command_wave += add_wave + grp_ctrl_sb + "{" + instance_name + "i_controller/ctrl_valid}\n"
    command_wave += add_wave + grp_ctrl_sb + "{" + instance_name + "i_controller/ctrl_ready}\n"
    command_wave += add_wave + grp_ctrl_sb + "{" + instance_name + "i_controller/sb*}\n"
    command_wave += add_wave + grp_ctrl_disp + "{" + instance_name + "i_controller/eu_valid}\n"
    command_wave += add_wave + grp_ctrl_disp + "{" + instance_name + "i_controller/eu_ready}\n"
    command_wave += add_wave + grp_ctrl_disp + "{" + instance_name + "i_controller/acc_zero_q}\n"
    command_wave += add_wave + grp_ctrl_disp + "{" + instance_name + "i_controller/mrf_zero_q}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group SA {" + instance_name + "i_controller/sa_*}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group SA {" + instance_name + "i_controller/is_mac_q}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group SA {" + instance_name + "i_controller/mac_start}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group SA {" + instance_name + "i_controller/mac_done}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group SA {" + instance_name + "i_controller/mac_last_id_q}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group LSU {" + instance_name + "i_controller/lsu*}\n"
    command_wave += add_wave + grp_ctrl_disp + "-group Zero {" + instance_name + "i_controller/*zero*}\n"

    command_wave += add_wave + grp_compress + "{" + instance_name + "x_compressed*}\n"
    command_wave += add_wave + grp_issue + "{" + instance_name + "x_issue*}\n"
    command_wave += add_wave + grp_issue + "{" + instance_name + "issue_ready}\n"
    command_wave += add_wave + grp_issue + "{" + instance_name + "issue_valid}\n"
    command_wave += add_wave + grp_issue + "{" + instance_name + "instr_valid}\n"
    command_wave += add_wave + grp_issue + "{" + instance_name + "instr}\n"
    command_wave += add_wave + grp_commit + "{" + instance_name + "x_commit*}\n"
    command_wave += add_wave + grp_commit + "{" + instance_name + "commit_id_q}\n"
    command_wave += add_wave + grp_commit + "{" + instance_name + "uncommited_instr_q}\n"
    command_wave += add_wave + grp_commit + "{" + instance_name + "instr_commit}\n"
    command_wave += add_wave + grp_memory + "{" + instance_name + "x_mem*}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "x_result*}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "commit_id}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "commit_valid}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "commit_ready}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "lsu_commit*}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "sa_commit*}\n"
    command_wave += add_wave + grp_xif_res + "{" + instance_name + "zero_commit*}\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_wave)
    print(f"✅ File '{output_file}' generated successfully.")
def generate_mrf_banks(n_lanes, n_regs, n_banks, output_file="mrf_waves.do"):
    """Generate virtual install and waveform commands for MRF signals."""
    
    create_virtual    = "quietly virtual function -install "
    instance_name     = "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_mrf"
    ff_name   = "gen_ff_bank"
    command_virtual = ""

    # --- Build command_virtual ---
    for lane in range(n_lanes):
        for bank in range(n_banks):
            for row in range(n_lanes):
                for ii in range(2):
                    command_virtual += create_virtual + '{' + instance_name 
                    command_virtual += f"/gen_superbanks[{lane}]/gen_rf_signals[{bank}]/i_quadrilatero_mrf_bank/"
                    command_virtual += ff_name + '}'
                    if ii==1 : 
                        command_virtual += " -env " + instance_name 
                        command_virtual += f"/gen_superbanks[{lane}]/gen_rf_signals[{bank}]/i_quadrilatero_mrf_bank/"
                        command_virtual += ff_name 
                        command_virtual += " { (concat_ascending)&{"
                        name = f"m{bank}_{row}_{lane}"
                    else :
                        command_virtual += " -env /tb_bin { &{"
                        name = f"m{bank}_{row}_{lane}__0"
        # bank_name = 
                    for byte in range(4):
                        signal = f"/mem_q[{row}][{4-1-byte}]"
                        command_virtual += instance_name
                        command_virtual += f"/gen_superbanks[{lane}]/gen_rf_signals[{bank}]/i_quadrilatero_mrf_bank/"
                        command_virtual += ff_name
                        command_virtual += signal + ", "
                    if command_virtual.endswith(", "):
                        command_virtual = command_virtual[:-2]
                    
                    command_virtual += " }} " 
                    command_virtual += f"{name}\n"

    # --- Build command_wave ---
    add_wave_MRF = "add wave -noupdate -group MRF "
    command_wave = ""
    command_wave += add_wave_MRF + "{" + instance_name + "/clk_i}\n"
    command_wave += add_wave_MRF + "{" + instance_name + "/rst_ni}\n"
    # # command_wave += add_wave_MRF + "{" + instance_name + "/testmode_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/req_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/we_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/addr_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/wdata_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/be_i}\n"
    # command_wave += add_wave_MRF + "{" + instance_name + "/rdata_o}\n"

    for ii in range(n_regs):
        for jj in range(n_lanes):
            for kk in range(n_lanes):
                command_wave += add_wave_MRF
                command_wave += f"-group m{ii} {instance_name}"
                command_wave += f"/gen_superbanks[{kk}]/gen_rf_signals[{ii}]/i_quadrilatero_mrf_bank/"
                command_wave += ff_name
                command_wave += f"/m{ii}_{jj}_{kk}\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_virtual)
        f.write(command_wave)

    print(f"✅ File '{output_file}' generated successfully.")

# Example usage:
if __name__ == "__main__":
    # generate_quad_mrf(n_banks=8)
    generate_mrf_banks(n_lanes=4, n_regs=16, n_banks=16)
    # generate_quad_lsu(port=0)
    # generate_quad_lsu(port=1)
    # generate_quad_signals()
