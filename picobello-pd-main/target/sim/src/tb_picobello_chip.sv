// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

module tb_picobello_chip;
  `ifndef MEM_TILE_NET_0
    `ifndef PICOBELLO_CHIP_NET
      `define L2_SRAM_PATH fix.dut.i_picobello_top.gen_memtile[i].i_mem_tile.gen_sram_banks[j].\
                           gen_sram_macros[k].i_mem.gen_1024x128m4.i_cut.u_ram_core.memory
    `endif
  `endif
 

  `include "tb_picobello_tasks.svh"

  // Instantiate the fixture
  fixture_picobello_chip fix();

  string       preload_elf;
  string       boot_hex;
  logic [1:0]  boot_mode;
  logic [1:0]  preload_mode;
  bit [31:0]   exit_code;
  bit          snitch_preload;
  string       snitch_elf;
  logic [63:0] snitch_entry;
  int          snitch_fn;
  bit          use_fll;

  initial begin
    // Fetch plusargs or use safe (fail-fast) defaults
    if (!$value$plusargs("BOOTMODE=%d",   boot_mode))     boot_mode     = 0;
    if (!$value$plusargs("PRELMODE=%d",   preload_mode))  preload_mode  = 1;
    if (!$value$plusargs("CHS_BINARY=%s", preload_elf))   preload_elf   = "";
    if (!$value$plusargs("IMAGE=%s",      boot_hex))      boot_hex      = "";
    if (!$value$plusargs("USE_FLL=%s",    use_fll))       use_fll       = 0;

    if ($value$plusargs("SN_BINARY=%s", snitch_elf)) begin
      snitch_fn = $fopen(".rtlbinary", "w");
      $fwrite(snitch_fn, snitch_elf);
      snitch_preload = 1;
    end else begin
      snitch_preload = 0;
    end

    // Set boot mode and preload boot image if there is one
    fix.vip.set_boot_mode(boot_mode);
    fix.vip.i2c_eeprom_preload(boot_hex);
    fix.vip.spih_norflash_preload(boot_hex);

    if (use_fll) begin
      // Apply the FLL reference clock in the beginning
      fix.vip_fll.apply_ref_clk();
    end

    // Wait for reset
    fix.vip.wait_for_reset();

    if (use_fll) begin
      // Wait for the FLL to lock
      fix.vip_fll.lock_wait();
      // Disable the clock bypass
      fix.vip_fll.disable_bypass();
      // Wait for a couple of cycles
      repeat (10) @(posedge fix.vip_fll.ref_clk);
    end

    // Preload in idle mode or wait for completion in autonomous boot
    if (boot_mode == 0) begin
      // Idle boot: preload with the specified mode
      case (preload_mode)
        0: begin      // JTAG
          jtag_enable_tiles();      // Write control registers
          if (snitch_preload) jtag_32b_elf_preload(snitch_elf, snitch_entry);
          fix.vip.jtag_elf_run(preload_elf);
          fix.vip.jtag_wait_for_eoc(exit_code);
        end 1: begin  // Serial Link
          slink_enable_tiles();      // Write control registers
          if (snitch_preload) slink_32b_elf_preload(snitch_elf, snitch_entry);
          fix.vip.slink_elf_run(preload_elf);
          fix.vip.slink_wait_for_eoc(exit_code);
        end 2: begin  // UART
          jtag_enable_tiles();      // Write control registers
          if (snitch_preload) $fatal(1, "Unsupported snitch binary preload mode %d (UART)!", preload_mode);
          fix.vip.uart_debug_elf_run_and_wait(preload_elf, exit_code);
        end 3: begin // Fast Mode
        `ifdef L2_SRAM_PATH 
          jtag_enable_tiles();      // Write control registers
          if (snitch_preload) fastmode_elf_preload(snitch_elf, snitch_entry);
          // TODO(fischeti): Implement fast mode for Cheshire binary
          fix.vip.jtag_elf_run(preload_elf);
          fix.vip.jtag_wait_for_eoc(exit_code);
          if (snitch_preload) fastmode_read();
        `else
          // Throw a warning if fastmode is enabled with MEM_TILE_NET_0 defined
          $warning(1, "Preload mode %d (Fast mode) cannot be enabled when the mem_tile or picobello_chip netlist is instantiated! Falling back to Serial Link mode", preload_mode);
          slink_enable_tiles();      // Write control registers
          if (snitch_preload) slink_32b_elf_preload(snitch_elf, snitch_entry);
          fix.vip.slink_elf_run(preload_elf);
          fix.vip.slink_wait_for_eoc(exit_code);
        `endif
        end default: begin
          $fatal(1, "Unsupported preload mode %d (reserved)!", boot_mode);
        end
      endcase
    end else if (boot_mode == 1) begin
      $fatal(1, "Unsupported boot mode %d (SD Card)!", boot_mode);
    end else begin
      // Autonomous boot: Only poll return code
      fix.vip.jtag_init();
      fix.vip.jtag_wait_for_eoc(exit_code);
    end

    // Wait for the UART to finish reading the current byte
    wait (fix.vip.uart_reading_byte == 0);

    $finish;
  end

endmodule
