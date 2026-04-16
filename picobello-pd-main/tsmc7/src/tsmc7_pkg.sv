// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

package tsmc7_pkg;

  typedef struct packed {
    // Input enable
    logic ie;
    // Schmitt-trigger enable
    logic st;
    // Pull-up
    logic pu;
    // Pull-down
    logic pd;
    // Drive strength
    logic [3:0] ds;
    // Output enable
    logic oe;
  } io_pad_ctrl_t;

  localparam io_pad_ctrl_t DefaultInputIoPadCtrl = '{
    ie : 1'b1,
    st : 1'b0,
    default : '0
  };

  localparam io_pad_ctrl_t DefaultOutputIoPadCtrl = '{
    pu : 1'b0,
    pd : 1'b0,
    ds : 4'b1000, // TODO(fischeti): This was arbitrarily chosen
    oe : 1'b1,
    default: '0
  };

  typedef struct packed {
    // L/R bank enable
    logic [1:0] lren;
    // Early CEB
    logic eceb;
    // Turbo mode
    logic turbo;
    // Read timing selection
    logic [1:0] rtsel;
    // Write timing selection
    logic [1:0] wtsel;
    // Timing adjustment setting for debugging purposes
    logic sapw;
  } l1cache_ctrl_t;

  localparam l1cache_ctrl_t DefaultL1CacheCtrl = '{
    // Left and right bank enable; only available in non-folded option
    // 11: Enables normal R/W operation
    // 01: Disables R/W in upper array; enables normal R/W in lower array
    // 10: Disables R/W in lower array; enables normal R/W in upper array
    // 00: Illegal State
    lren : 2'b11,
    // Early chip enable, avtive low; only available in non-folded option
    // INFO: Could be enabled if request is known one cycle before
    eceb : 1'b0,
    // Turbo mode not supported, according to Databook (page 17)
    turbo : 1'b0,
    // Timing Adjustment settings for debugging purpose
    // The timing data is characterized with the default setting only
    // Default values are defined in TSMC N7 L1 cache Compiler Databook Table 2.4, page 17
    rtsel : 2'b01,
    // Timing Adjustment settings for debugging purpose
    // The timing data is characterized with the default setting only
    // Default values are defined in TSMC N7 L1 cache Compiler Databook Table 2.4, page 17
    wtsel : 2'b01,
    // Timing Adjustment settings for debugging purpose
    // The timing data is characterized with the default setting only
    // Default values are defined in TSMC N7 L1 cache Compiler Databook Table 2.4, page 17
    sapw : 1'b0
  };

  typedef struct packed {
    // Shot-down mode, active-high
    logic sd;
    // Deep-sleep mode, active-high
    logic dslp;
    // Column repair enable, active-high
    logic redenio;
    // Scan-chain enable, active-high
    logic se;
    // DFT enable, active-high
    logic dftbyp;
    // Scan-in control
    logic sic;
    // Scan-in data/bit write enable
    logic [1:0] sid;
    // Diode bypass enable mode
    logic dslplv;
    // Read timing selection
    logic [1:0] rtsel;
    // Write timing selection
    logic [1:0] wtsel;
  } spsbram_ctrl_t;

  localparam spsbram_ctrl_t DefaultSpSbSramCtrl = '{
    // Shot-down mode, active-high
    sd : 1'b0,
    // Deep-sleep mode, active-high
    dslp : 1'b0,
    // Column repair enable, active-high
    redenio : 1'b0,
    // Scan-chain enable, active-high
    se : 1'b0,
    // DFT enable, active-high
    dftbyp : 1'b0,
    // Scan-in control
    sic : 1'b0,
    // Scan-in data/bit write enable
    sid : 2'b00,
    // Diode bypass enable mode
    // See SRAM Compiler Databook, Section 2.1.5, page 33
    dslplv : 1'b0,
    // Timing Adjustment settings for debugging purpose
    // The timing data is characterized with the default setting only
    // Default values are defined in TSMC N7 SRAM Compiler Databook Table 2.19, page 32
    rtsel : 2'b10,
    // Timing Adjustment settings for debugging purpose
    // The timing data is characterized with the default setting only
    // Default values are defined in TSMC N7 SRAM Compiler Databook Table 2.19, page 32
    wtsel : 2'b01
  };

endpackage
