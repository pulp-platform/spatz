// Copyright 2025 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

`ifndef TC_SRAM_SP_DEFAULT_CLK
  `define TC_SRAM_SP_DEFAULT_CLK clk_i[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_CEB
  `define TC_SRAM_SP_DEFAULT_CEB req_i[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_WEB
  `define TC_SRAM_SP_DEFAULT_WEB we_i[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_ADDR
  `define TC_SRAM_SP_DEFAULT_ADDR addr_i[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_BWEB
  `define TC_SRAM_SP_DEFAULT_BWEB be[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_D
  `define TC_SRAM_SP_DEFAULT_D wdata_i[0]
`endif
`ifndef TC_SRAM_SP_DEFAULT_Q
  `define TC_SRAM_SP_DEFAULT_Q rdata_o[0]
`endif

`define TSMC7_L1CACHE_TEMPLATE(num_words, num_bits, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb, sram_d, sram_q) \
  tsn7_l1cache_``num_words``x``num_bits``m``num_mux i_cut (       \
     /* Clock */                                              \
    .CLK  (sram_clk),                                         \
     /* Chip-enable, active-low */                            \
    .CEB (~sram_ceb),                                         \
     /* Write-enable, active-low */                           \
    .WEB (~sram_web),                                         \
     /* L/R bank enable*/                                     \
    .LREN (sram_ctrl.lren),                                   \
     /* Address */                                            \
    .A    (sram_addr),                                        \
     /* bit enable, active-low */                             \
    .BWEB (~sram_bweb),                                       \
     /* Write data */                                         \
    .D (sram_d),                                              \
     /* Read data */                                          \
    .Q (sram_q),                                              \
     /* Early CEB, active-low,  */                            \
    .ECEB (sram_ctrl.eceb),                                   \
     /* Not supported mode */                                 \
    .TURBO(sram_ctrl.turbo),                                  \
     /* Timing adjustment setting, */                         \
     /* default values must be set according to */            \
     /* TSMC N7 L1 cache Compiler Databook Table Table 2.4 */ \
    .RTSEL (sram_ctrl.rtsel),                                 \
    .WTSEL (sram_ctrl.wtsel),                                 \
    .SAPW (sram_ctrl.sapw)                                    \
  );

`define TSMC7_L1CACHE(num_words, num_bits, num_mux, sram_ctrl, sram_clk = `TC_SRAM_SP_DEFAULT_CLK, sram_ceb = `TC_SRAM_SP_DEFAULT_CEB, sram_web = `TC_SRAM_SP_DEFAULT_WEB, sram_addr = `TC_SRAM_SP_DEFAULT_ADDR, sram_bweb = `TC_SRAM_SP_DEFAULT_BWEB, sram_d = `TC_SRAM_SP_DEFAULT_D, sram_q = `TC_SRAM_SP_DEFAULT_Q) \
  `TSMC7_L1CACHE_TEMPLATE(num_words, num_bits, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb, sram_d, sram_q)

`define TSMC7_L1CACHE_WORD_PADDING(num_words, num_bits_sram, num_bits_actual, num_mux, sram_ctrl, sram_clk = `TC_SRAM_SP_DEFAULT_CLK, sram_ceb = `TC_SRAM_SP_DEFAULT_CEB, sram_web = `TC_SRAM_SP_DEFAULT_WEB, sram_addr = `TC_SRAM_SP_DEFAULT_ADDR, sram_bweb = `TC_SRAM_SP_DEFAULT_BWEB, sram_d = `TC_SRAM_SP_DEFAULT_D, sram_q = `TC_SRAM_SP_DEFAULT_Q) \
  localparam int unsigned NumPaddingBits = num_bits_sram - num_bits_actual; \
  logic [num_bits_sram-1:0] sram_bweb_padded, sram_d_padded, sram_q_padded; \
  assign sram_bweb_padded = {{NumPaddingBits{1'b0}}, sram_bweb}; \
  assign sram_d_padded = {{NumPaddingBits{1'b0}}, sram_d}; \
  assign sram_q = sram_q_padded[num_bits_actual-1:0]; \
  `TSMC7_L1CACHE_TEMPLATE(num_words, num_bits_sram, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb_padded, sram_d_padded, sram_q_padded)


`define TSMC7_SPSBSRAM_TEMPLATE(num_words, num_bits, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb, sram_d, sram_q) \
  tsn7_spsbsram_``num_words``x``num_bits``m``num_mux i_cut (  \
    /* Shut-down mode, active-high */                     \
    .SD (sram_ctrl.sd),                                   \
    /* Deep-sleep mode, active-high */                    \
    .DSLP (sram_ctrl.dslp),                               \
    /* Clock */                                           \
    .CLK (sram_clk),                                      \
    /* Chip-enable, active-low */                         \
    .CEB (~sram_ceb),                                     \
    /* Write-enable, active-low */                        \
    .WEB (~sram_web),                                     \
    /* Address */                                         \
    .A (sram_addr),                                       \
    /* Bit-write enable, active-low */                    \
    .BWEB (~sram_bweb),                                   \
    /* Write data */                                      \
    .D (sram_d),                                          \
    /* Read data */                                       \
    .Q (sram_q),                                          \
    /* Propagation signal from shut-down mode */          \
    .PUDELAY_SD (),                                       \
    /* Propagation signal from deep-sleep mode */         \
    .PUDELAY_DSLP (),                                     \
    /* Faulty-IO address input */                         \
    /* TODO: Implement this if desired */                 \
    .FADIO ('0),                                          \
    /* Column repair enable, active-high */               \
    .REDENIO (sram_ctrl.redenio),                         \
    /* Scan-chain enable, active-high */                  \
    .SE (sram_ctrl.se),                                   \
    /* DFT enable, active-high */                         \
    .DFTBYP (sram_ctrl.dftbyp),                           \
    /* Scan-in control */                                 \
    .SIC (sram_ctrl.sic),                                 \
    /* Scan-in data/bit write enable */                   \
    .SID (sram_ctrl.sid),                                 \
    /* Scan-out control */                                \
    /* TODO: Implement if desired */                      \
    .SOC (),                                              \
    /* Scan-out data/bit write enable */                  \
    /* TODO: Implement if desired */                      \
    .SOD (),                                              \
    /* Diode bypass mode enable     */                    \
    .DSLPLV (sram_ctrl.dslplv),                           \
    /* Timing adjustment settings */                      \
    .RTSEL (sram_ctrl.rtsel),                             \
    /* Timing adjustment settings */                      \
    .WTSEL (sram_ctrl.wtsel)                              \
  );

`define TSMC7_SPSBSRAM(num_words, num_bits, num_mux, sram_ctrl, sram_clk = `TC_SRAM_SP_DEFAULT_CLK, sram_ceb = `TC_SRAM_SP_DEFAULT_CEB, sram_web = `TC_SRAM_SP_DEFAULT_WEB, sram_addr = `TC_SRAM_SP_DEFAULT_ADDR, sram_bweb = `TC_SRAM_SP_DEFAULT_BWEB, sram_d = `TC_SRAM_SP_DEFAULT_D, sram_q = `TC_SRAM_SP_DEFAULT_Q) \
  `TSMC7_SPSBSRAM_TEMPLATE(num_words, num_bits, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb, sram_d, sram_q)

`define TSMC7_SPSBSRAM_WORD_PADDING(num_words, num_bits_sram, num_bits_actual, num_mux, sram_ctrl, sram_clk = `TC_SRAM_SP_DEFAULT_CLK, sram_ceb = `TC_SRAM_SP_DEFAULT_CEB, sram_web = `TC_SRAM_SP_DEFAULT_WEB, sram_addr = `TC_SRAM_SP_DEFAULT_ADDR, sram_bweb = `TC_SRAM_SP_DEFAULT_BWEB, sram_d = `TC_SRAM_SP_DEFAULT_D, sram_q = `TC_SRAM_SP_DEFAULT_Q) \
  localparam int unsigned NumPaddingBits = num_bits_sram - num_bits_actual; \
  logic [num_bits_sram-1:0] sram_bweb_padded, sram_d_padded, sram_q_padded; \
  assign sram_bweb_padded = {{NumPaddingBits{1'b0}}, sram_bweb}; \
  assign sram_d_padded = {{NumPaddingBits{1'b0}}, sram_d}; \
  assign sram_q = sram_q_padded[num_bits_actual-1:0]; \
  `TSMC7_SPSBSRAM_TEMPLATE(num_words, num_bits_sram, num_mux, sram_ctrl, sram_clk, sram_ceb, sram_web, sram_addr, sram_bweb_padded, sram_d_padded, sram_q_padded)
