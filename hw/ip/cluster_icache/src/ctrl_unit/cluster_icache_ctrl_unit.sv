// Copyright 2024 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Michael Rogenmoser <michaero@iis.ee.ethz.ch>

module cluster_icache_ctrl_unit import snitch_icache_pkg::*; #(
  parameter int unsigned NR_FETCH_PORTS = -1,
  parameter type         reg_req_t      = logic,
  parameter type         reg_rsp_t      = logic
) (
  input  logic                                   clk_i,
  input  logic                                   rst_ni,

  input  reg_req_t                               reg_req_i,
  output reg_rsp_t                               reg_rsp_o,

  output logic                                   enable_prefetching_o,
  output logic              [NR_FETCH_PORTS-1:0] flush_valid_o,
  input  logic              [NR_FETCH_PORTS-1:0] flush_ready_i,

  input  icache_l0_events_t [NR_FETCH_PORTS-1:0] l0_events_i,
  input  icache_l1_events_t                      l1_events_i
);

  import cluster_icache_ctrl_reg_pkg::*;

  initial begin
    assert(NumL0Events == $bits(icache_l0_events_t));
    assert(NumL1Events == $bits(icache_l1_events_t));
    assert(NumAvailableCounters >= NumL1Events + NR_FETCH_PORTS*NumL0Events);
    assert(NR_FETCH_PORTS <= NumCores);
  end

  cluster_icache_ctrl_reg2hw_t reg2hw;
  cluster_icache_ctrl_hw2reg_t hw2reg;

  cluster_icache_ctrl_reg_top #(
    .reg_req_t(reg_req_t),
    .reg_rsp_t(reg_rsp_t)
  ) i_regs (
    .clk_i,
    .rst_ni,

    .reg_req_i,
    .reg_rsp_o,

    .reg2hw (reg2hw),
    .hw2reg (hw2reg),
    .devmode_i (1'b0)
  );

  cluster_icache_ctrl_hw2reg_counters_mreg_t [NumAvailableCounters-1:0] counters_reg;

  always_comb begin : gen_counters_reg
    // set up defaults - increment but not active
    for (int unsigned i = 0; i < NumAvailableCounters; i++) begin
      counters_reg[i].d = reg2hw.counters[i].q + 1;
      counters_reg[i].de = '0;
    end

    // Activate increment counters
    counters_reg[0].de = reg2hw.enable_counters.q & l1_events_i.l1_miss;
    counters_reg[1].de = reg2hw.enable_counters.q & l1_events_i.l1_hit;
    counters_reg[2].de = reg2hw.enable_counters.q & l1_events_i.l1_stall;
    counters_reg[3].de = reg2hw.enable_counters.q & l1_events_i.l1_handler_stall;
    counters_reg[5].de = reg2hw.enable_counters.q & l1_events_i.l1_tag_parity_error;
    counters_reg[6].de = reg2hw.enable_counters.q & l1_events_i.l1_data_parity_error;

    for (int unsigned i = 0; i < NR_FETCH_PORTS; i++) begin
      counters_reg[NumL1Events + i*NumL0Events + 0].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_miss;
      counters_reg[NumL1Events + i*NumL0Events + 1].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_hit;
      counters_reg[NumL1Events + i*NumL0Events + 2].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_prefetch;
      counters_reg[NumL1Events + i*NumL0Events + 3].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_double_hit;
      counters_reg[NumL1Events + i*NumL0Events + 4].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_stall;
      counters_reg[NumL1Events + i*NumL0Events + 5].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_tag_parity_error;
      counters_reg[NumL1Events + i*NumL0Events + 6].de = reg2hw.enable_counters.q &
                                                         l0_events_i[i].l0_data_parity_error;
    end

    // Clear on global clear signal
    if (reg2hw.clear_counters.q) begin
      for (int unsigned i = 0; i < NumAvailableCounters; i++) begin
        counters_reg[i].d = '0;
        counters_reg[i].de = '1;
      end
    end
  end

  assign enable_prefetching_o = reg2hw.enable_prefetch.q;
  assign flush_valid_o = ({NR_FETCH_PORTS{reg2hw.flush.q}} & {NR_FETCH_PORTS{reg2hw.flush.qe}}) |
                         (reg2hw.sel_flush_icache.q[NR_FETCH_PORTS-1:0] &
                          {NR_FETCH_PORTS{reg2hw.sel_flush_icache.qe}});

  assign hw2reg.flush.d            = ~|flush_ready_i;
  assign hw2reg.flush_l1_only.d    = '0;
  assign hw2reg.sel_flush_icache.d = ~flush_ready_i;
  assign hw2reg.clear_counters.d   = '0;
  assign hw2reg.counters           = counters_reg;

endmodule
