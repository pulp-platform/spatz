#!/usr/bin/env python3

"""
This script generates ModelSim/QuestaSim .do files for MRF, Quad LSU, and Quad signals.
It includes functions to compute encoded numbers and generate virtual signals and waveforms.
"""


def get_number(value, reg):
    """
    Compute encoded number from bank index and register group.

    Args:
        value (int): The bank index.
        reg (int): The register group.

    Returns:
        int: The computed encoded number.
    """
    msb = value >> 1
    lsb = value & 0x1
    output = (msb << 2) | (reg << 1) | lsb
    return output


def generate_mrf_waves(n_lanes, n_regs, n_banks, output_file="mrf_waves.do"):
    """
    Generate virtual install and waveform commands for MRF signals.

    Args:
        n_lanes (int): Number of lanes.
        n_regs (int): Number of registers.
        n_banks (int): Number of banks.
        output_file (str): Name of the output .do file.
    """
    create_virtual = "quietly virtual function -install "
    instance_name = (
        "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/"
        "i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_regfile"
    )
    open_list_signals = " -env /tb_bin { &{"

    command_virtual = ""

    # --- Build command_virtual ---
    for ii in range(n_lanes):
        for jj in range(n_banks):
            for kk in range(int(n_lanes * n_regs / n_banks)):
                command_virtual += (
                    f"{create_virtual}{{{instance_name}}}{open_list_signals}"
                )
                for zz in range(n_lanes):
                    signal = f"/mem_q[{ii}][{jj}][{kk}][{n_lanes - 1 - zz}]"
                    command_virtual += f"{instance_name}{signal}, "

                reg_group = (kk - (kk % n_lanes)) // n_lanes
                name = f"m{get_number(jj, reg_group)}_{kk % n_lanes}_{ii}"

                if command_virtual.endswith(", "):
                    command_virtual = command_virtual[:-2]

                command_virtual += " }} "
                command_virtual += f"{name}\n"

    # --- Build command_wave ---
    add_wave_mrf = "add wave -noupdate -group MRF "
    command_wave = ""
    signals = [
        "clk_i", "rst_ni", "testmode_i", "raddr_i", "rrowaddr_i",
        "rdata_o", "waddr_i", "wrowaddr_i", "wdata_i", "wbe_i", "we_i"
    ]

    for sig in signals:
        command_wave += f"{add_wave_mrf}{{{instance_name}/{sig}}}\n"

    command_wave += (
        f"{add_wave_mrf}-subitemconfig {{"
        f"{{{instance_name}/mem_q[3]}} "
        f"{{{instance_name}/mem_q[3][3]}} }} "
        f"{{{instance_name}/mem_q}}\n"
    )

    for ii in range(n_regs):
        for jj in range(n_lanes):
            for kk in range(n_lanes):
                command_wave += (
                    f"{add_wave_mrf}-group m{ii} {{{instance_name}/m{ii}_{jj}_{kk}}}\n"
                )

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_virtual)
        f.write(command_wave)

    print(f"✅ File '{output_file}' generated successfully.")


def generate_quad_mrf(n_banks, output_file="quad_mrf_sc.do"):
    """
    Generate waveform commands for the Quad MRF Controller.

    Args:
        n_banks (int): Number of banks.
        output_file (str): Name of the output .do file.
    """
    instance_name = (
        "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/"
        "i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_mrf/"
    )
    add_wave = "add wave -noupdate -group {Quad MRF Controller} "
    grp_issue = "-group {Issue Request} {"
    grp_ports_fu = "-group {Ports FU} {"
    grp_ports_rf = "-group {Ports RF} {"
    grp_int = "-group Scoreboard {"

    commands = [
        f"{add_wave}{{{instance_name}clk_i}}",
        f"{add_wave}{grp_issue}{instance_name}sb_valid_i}}",
        f"{add_wave}{grp_issue}{instance_name}sb_ready_o}}",
        f"{add_wave}{grp_issue}{instance_name}sb_data_i}}",
        f"{add_wave}{grp_issue}{instance_name}sb_valid}}",
        f"{add_wave}{grp_issue}{instance_name}sb_ready}}",
        f"{add_wave}{grp_issue}{instance_name}sb_req_q}}",
        f"{add_wave}{grp_ports_fu}{instance_name}addr_i}}",
        f"{add_wave}{grp_ports_fu}{instance_name}rowaddr_i}}",
        f"{add_wave}{grp_ports_fu}{instance_name}rdata_o}}",
        f"{add_wave}{grp_ports_fu}{instance_name}valid_o}}",
        f"{add_wave}{grp_ports_fu}{instance_name}last_i}}",
        f"{add_wave}{grp_ports_fu}{instance_name}is_zero_i}}",
        f"{add_wave}{grp_ports_fu}{instance_name}ready_i}}",
        f"{add_wave}{grp_ports_fu}{instance_name}wdata_i}}",
        f"{add_wave}{grp_ports_rf}{instance_name}req}}",
        f"{add_wave}{grp_ports_rf}{instance_name}addr}}",
        f"{add_wave}{grp_ports_rf}{instance_name}we}}",
        f"{add_wave}{grp_ports_rf}{instance_name}be}}",
        f"{add_wave}{grp_ports_rf}{instance_name}rdata}}",
        f"{add_wave}{grp_ports_rf}{instance_name}wdata}}",
        f"{add_wave}{grp_int}{instance_name}eu_wr}}",
        f"{add_wave}{grp_int}{instance_name}eu_port_id}}",
        f"{add_wave}{grp_int}{instance_name}eu_valid}}",
        f"{add_wave}{grp_int}{instance_name}eu_ready}}"
    ]

    command_wave = "\n".join(commands) + "\n"

    gen_sb = "gen_superbanks["
    gen_sign = "]/gen_sb_signals["
    gen_sc = "]/i_scoreboard"

    for j in range(n_banks):
        for i in range(4):
            command_wave += (
                f"{add_wave}-group {{Scoreboard State}} "
                f"{{{instance_name}{gen_sb}{i}{gen_sign}{j}{gen_sc}/sb_current_q}}\n"
            )

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_wave)
    print(f"✅ File '{output_file}' generated successfully.")


def generate_quad_lsu(port):
    """
    Generate waveform commands for a specific Quad LSU port.

    Args:
        port (int): The port index.
    """
    output_file = f"quad_lsu_{port}.do"
    instance_name = (
        f"/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/"
        f"i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/gen_port[{port}]/i_lsu/"
    )
    add_wave = "add wave -noupdate -group {Quad LSU} "
    grp_mem = "-group {Memory Ports}"
    grp_rd = "-group {Read Port}"
    grp_wr = "-group {Write Port}"
    grp_addr = "-group Address"
    grp_st_fifo = "-group {Store FIFO}"
    grp_ld_fifo = "-group {Load FIFO}"
    grp_cnt = "-group Counters"
    grp_finish = "-group Finish"
    grp_int = "-group Internal"

    lines = [
        f"{add_wave}{{{instance_name}clk_i}}",
        f"{add_wave}{{{instance_name}rst_ni}}",
        f"{add_wave}-color Magenta {{{instance_name}state_q}}",
        f"{add_wave}{{{instance_name}instr_i}}",
        f"{add_wave}{{{instance_name}instr_valid_i}}",
        f"{add_wave}{{{instance_name}instr_ready_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_req_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_addr_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_we_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_be_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_wdata_o}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_gnt_i}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_rvalid_i}}",
        f"{add_wave}{grp_mem} {{{instance_name}data_rdata_i}}",
        f"{add_wave}{grp_wr} {{{instance_name}waddr_o}}",
        f"{add_wave}{grp_wr} {{{instance_name}wrowaddr_o}}",
        f"{add_wave}{grp_wr} {{{instance_name}wdata_o}}",
        f"{add_wave}{grp_wr} {{{instance_name}we_o}}",
        f"{add_wave}{grp_wr} {{{instance_name}wlast_o}}",
        f"{add_wave}{grp_wr} {{{instance_name}wready_i}}",
        f"{add_wave}{grp_rd} {{{instance_name}raddr_o}}",
        f"{add_wave}{grp_rd} {{{instance_name}rrowaddr_o}}",
        f"{add_wave}{grp_rd} {{{instance_name}rdata_i}}",
        f"{add_wave}{grp_rd} {{{instance_name}rdata_valid_i}}",
        f"{add_wave}{grp_rd} {{{instance_name}rdata_ready_o}}",
        f"{add_wave}{grp_rd} {{{instance_name}rlast_o}}",
        f"{add_wave}{grp_rd} {{{instance_name}rzero_o}}",
        f"{add_wave}{grp_addr} {{{instance_name}base_addr}}",
        f"{add_wave}{grp_addr} {{{instance_name}base_addr_d}}",
        f"{add_wave}{grp_addr} {{{instance_name}base_addr_q}}",
        f"{add_wave}{grp_addr} {{{instance_name}stride_d}}",
        f"{add_wave}{grp_addr} {{{instance_name}stride_q}}",
        f"{add_wave}{grp_addr} {{{instance_name}stride}}",
        f"{add_wave}{grp_addr} {{{instance_name}addr_incr}}",
        f"{add_wave}{grp_addr} {{{instance_name}mem_addr}}",
        f"{add_wave}{grp_addr} {{{instance_name}mem_row_shift}}",
        f"{add_wave}{grp_addr} {{{instance_name}mem_col_shift}}",
        f"{add_wave}{grp_ld_fifo} {{{instance_name}load_fifo_*}}",
        f"{add_wave}{grp_st_fifo} {{{instance_name}store_fifo_*}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_done}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_done}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_col_cnt_d}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_col_cnt_q}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_row_cnt_d}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_row_cnt_q}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_cnt_d}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_cnt_q}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_row_cnt_d}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_row_cnt_q}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_col_cnt_d}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_cnt_upd}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_col_cnt_tc}}",
        f"{add_wave}{grp_cnt} {{{instance_name}rf_row_cnt_tc}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_cnt_upd}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_col_cnt_tc}}",
        f"{add_wave}{grp_cnt} {{{instance_name}mem_col_cnt_q}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_valid_o}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_ready_i}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_id_o}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_stall}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_d}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_q}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_id_d}}",
        f"{add_wave}{grp_finish} {{{instance_name}commit_id_q}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_waddr}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_wrowaddr}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_wdata}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_we}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_wlast}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_wready}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_raddr}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_rrowaddr}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_rdata}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_rdata_valid}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_rdata_ready}}",
        f"{add_wave}{grp_int} {{{instance_name}sp_rlast}}",
        f"{add_wave}{grp_int} {{{instance_name}data_rvalid}}",
        f"{add_wave}{grp_int} {{{instance_name}data_rready}}",
        f"{add_wave}{grp_int} {{{instance_name}data_wvalid}}",
        f"{add_wave}{grp_int} {{{instance_name}mask_rvalid_q}}",
        f"{add_wave}{grp_int} {{{instance_name}mask_rvalid_d}}",
        f"{add_wave}{grp_int} {{{instance_name}state_d}}",
        f"{add_wave}{grp_int} {{{instance_name}back_id_q}}",
        f"{add_wave}{grp_int} {{{instance_name}back_id_d}}",
        f"{add_wave}{grp_int} {{{instance_name}data_mask}}",
        f"{add_wave}{grp_int} {{{instance_name}write_q}}",
        f"{add_wave}{grp_int} {{{instance_name}write_d}}",
        f"{add_wave}{grp_int} {{{instance_name}rf_write}}",
        f"{add_wave}{grp_int} {{{instance_name}mem_write}}",
        f"{add_wave}{grp_int} {{{instance_name}mreg_d}}",
        f"{add_wave}{grp_int} {{{instance_name}mreg_q}}",
        f"{add_wave}{grp_int} {{{instance_name}mreg}}",
        f"{add_wave}{grp_int} {{{instance_name}mem_en}}"
    ]

    command_wave = "\n".join(lines) + "\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_wave)
    print(f"✅ File '{output_file}' generated successfully.")


def generate_quad_signals(output_file="quad_instr.do"):
    """
    Generate waveform commands for Quad instruction signals.

    Args:
        output_file (str): Name of the output .do file.
    """
    instance_name = (
        "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/"
        "i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/"
    )
    add_wave = "add wave -noupdate "
    grp_ctrl_issue = "-group {Quad Controller} -group Issue "
    grp_ctrl_sb = "-group {Quad Controller} -group Scoreboard "
    grp_ctrl_disp = "-group {Quad Controller} -group Dispatch "
    grp_compress = "-group XIF -group Compressed "
    grp_issue = "-group XIF -group Issue "
    grp_commit = "-group XIF -group Commit "
    grp_memory = "-group XIF -group Memory "
    grp_xif_res = "-group XIF -group Result "

    lines = [
        f"{add_wave}{{{instance_name}clk_i}}",
        f"{add_wave}-color Magenta {{{instance_name}x_issue_valid_i}}",
        f"{add_wave}-color Magenta {{{instance_name}x_issue_ready_o}}",
        f"{add_wave}-color Magenta {{{instance_name}x_issue_req_i}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/clk_i}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/instr_valid_i}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/instr_i}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/instr_ready_o}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/dec_valid}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/dec_instr}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/instr}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/issue_ready}}",
        f"{add_wave}{grp_ctrl_issue}{{{instance_name}i_controller/issue_valid}}",
        f"{add_wave}{grp_ctrl_sb}{{{instance_name}i_controller/ctrl_valid}}",
        f"{add_wave}{grp_ctrl_sb}{{{instance_name}i_controller/ctrl_ready}}",
        f"{add_wave}{grp_ctrl_sb}{{{instance_name}i_controller/sb*}}",
        f"{add_wave}{grp_ctrl_disp}{{{instance_name}i_controller/eu_valid}}",
        f"{add_wave}{grp_ctrl_disp}{{{instance_name}i_controller/eu_ready}}",
        f"{add_wave}{grp_ctrl_disp}{{{instance_name}i_controller/acc_zero_q}}",
        f"{add_wave}{grp_ctrl_disp}{{{instance_name}i_controller/mrf_zero_q}}",
        f"{add_wave}{grp_ctrl_disp}-group SA {{{instance_name}i_controller/sa_*}}",
        f"{add_wave}{grp_ctrl_disp}-group SA {{{instance_name}i_controller/is_mac_q}}",
        f"{add_wave}{grp_ctrl_disp}-group SA {{{instance_name}i_controller/mac_start}}",
        f"{add_wave}{grp_ctrl_disp}-group SA {{{instance_name}i_controller/mac_done}}",
        f"{add_wave}{grp_ctrl_disp}-group SA {{{instance_name}i_controller/mac_last_id_q}}",
        f"{add_wave}{grp_ctrl_disp}-group LSU {{{instance_name}i_controller/lsu*}}",
        f"{add_wave}{grp_ctrl_disp}-group Zero {{{instance_name}i_controller/*zero*}}",
        f"{add_wave}{grp_compress}{{{instance_name}x_compressed*}}",
        f"{add_wave}{grp_issue}{{{instance_name}x_issue*}}",
        f"{add_wave}{grp_issue}{{{instance_name}issue_ready}}",
        f"{add_wave}{grp_issue}{{{instance_name}issue_valid}}",
        f"{add_wave}{grp_issue}{{{instance_name}instr_valid}}",
        f"{add_wave}{grp_issue}{{{instance_name}instr}}",
        f"{add_wave}{grp_commit}{{{instance_name}x_commit*}}",
        f"{add_wave}{grp_commit}{{{instance_name}commit_id_q}}",
        f"{add_wave}{grp_commit}{{{instance_name}uncommited_instr_q}}",
        f"{add_wave}{grp_commit}{{{instance_name}instr_commit}}",
        f"{add_wave}{grp_memory}{{{instance_name}x_mem*}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}x_result*}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}commit_id}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}commit_valid}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}commit_ready}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}lsu_commit*}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}sa_commit*}}",
        f"{add_wave}{grp_xif_res}{{{instance_name}zero_commit*}}"
    ]

    command_wave = "\n".join(lines) + "\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_wave)
    print(f"✅ File '{output_file}' generated successfully.")


def generate_mrf_banks(n_lanes, n_regs, n_banks, output_file="mrf_waves.do"):
    """
    Generate virtual install and waveform commands for MRF bank signals.

    Args:
        n_lanes (int): Number of lanes.
        n_regs (int): Number of registers.
        n_banks (int): Number of banks.
        output_file (str): Name of the output .do file.
    """
    create_virtual = "quietly virtual function -install "
    instance_name = (
        "/tb_bin/i_dut/i_cluster_wrapper/i_cluster/gen_core[0]/"
        "i_spatz_quadrilatero_cc/gen_quadrilatero/i_quadrilatero/i_mrf"
    )
    ff_name = "gen_ff_bank"
    command_virtual = ""

    # --- Build command_virtual ---
    for lane in range(n_lanes):
        for bank in range(n_banks):
            for row in range(n_lanes):
                for ii in range(2):
                    bank_path = (
                        f"{instance_name}/gen_superbanks[{lane}]/"
                        f"gen_rf_signals[{bank}]/i_quadrilatero_mrf_bank/{ff_name}"
                    )
                    command_virtual += f"{create_virtual}{{{bank_path}}}"

                    if ii == 1:
                        command_virtual += f" -env {bank_path} {{ (concat_ascending)&{{"
                        name = f"m{bank}_{row}_{lane}"
                    else:
                        command_virtual += " -env /tb_bin { &{"
                        name = f"m{bank}_{row}_{lane}__0"

                    for byte in range(4):
                        signal = f"/mem_q[{row}][{4 - 1 - byte}]"
                        command_virtual += f"{bank_path}{signal}, "

                    if command_virtual.endswith(", "):
                        command_virtual = command_virtual[:-2]

                    command_virtual += " }} "
                    command_virtual += f"{name}\n"

    # --- Build command_wave ---
    add_wave_mrf = "add wave -noupdate -group MRF "
    command_wave = f"{add_wave_mrf}{{{instance_name}/clk_i}}\n"
    command_wave += f"{add_wave_mrf}{{{instance_name}/rst_ni}}\n"

    for ii in range(n_regs):
        for jj in range(n_lanes):
            for kk in range(n_lanes):
                bank_ff_path = (
                    f"{instance_name}/gen_superbanks[{kk}]/"
                    f"gen_rf_signals[{ii}]/i_quadrilatero_mrf_bank/{ff_name}"
                )
                command_wave += f"{add_wave_mrf}-group m{ii} {bank_ff_path}/m{ii}_{jj}_{kk}\n"

    # --- Write to file ---
    with open(output_file, "w") as f:
        f.write(command_virtual)
        f.write(command_wave)

    print(f"✅ File '{output_file}' generated successfully.")


if __name__ == "__main__":
    # Example usage:
    # generate_quad_mrf(n_banks=8)
    generate_mrf_banks(n_lanes=4, n_regs=16, n_banks=16)
    # generate_quad_lsu(port=0)
    # generate_quad_lsu(port=1)
    # generate_quad_signals()
