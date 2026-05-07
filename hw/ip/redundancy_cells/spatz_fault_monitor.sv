// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

module spatz_fault_monitor #(
  parameter int unsigned NumVrfUnits     = 1,
  parameter int unsigned NumTcdmBanks    = 1,
  parameter int unsigned CounterWidth    = 32,
  parameter bit          SaturatingCount = 1'b1
) (
  input  logic clk_i,
  input  logic rst_ni,

  // Tie to 1'b0 if no explicit clear is needed.
  input  logic clear_i,

  // Fault inputs.
  // Expected to be one-cycle pulses.
  input  logic [NumVrfUnits-1:0]  vrf_correctable_fault_i,
  input  logic [NumVrfUnits-1:0]  vrf_uncorrectable_fault_i,

  input  logic [NumTcdmBanks-1:0] tcdm_rd_correctable_fault_i,
  input  logic [NumTcdmBanks-1:0] tcdm_rd_uncorrectable_fault_i,
  input  logic [NumTcdmBanks-1:0] tcdm_scrub_correctable_fault_i,
  input  logic [NumTcdmBanks-1:0] tcdm_scrub_uncorrectable_fault_i,

  // Per-source counters.
  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  vrf_correctable_count_o,
  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  vrf_uncorrectable_count_o,

  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_rd_correctable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_rd_uncorrectable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_scrub_correctable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_scrub_uncorrectable_count_o
);


  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      vrf_correctable_count_o    <= '0;
      vrf_uncorrectable_count_o  <= '0;
      tcdm_rd_correctable_count_o   <= '0;
      tcdm_rd_uncorrectable_count_o <= '0;
      tcdm_scrub_correctable_count_o   <= '0;
      tcdm_scrub_uncorrectable_count_o <= '0;

    end else if (clear_i) begin
      vrf_correctable_count_o    <= '0;
      vrf_uncorrectable_count_o  <= '0;
      tcdm_rd_correctable_count_o   <= '0;
      tcdm_rd_uncorrectable_count_o <= '0;
      tcdm_scrub_correctable_count_o   <= '0;
      tcdm_scrub_uncorrectable_count_o <= '0;

    end else begin
      for (int unsigned i = 0; i < NumVrfUnits; i++) begin
        if (vrf_correctable_fault_i[i]) begin
          if (!SaturatingCount || !(&vrf_correctable_count_o[i])) begin
            vrf_correctable_count_o[i] <= vrf_correctable_count_o[i] + 1'b1;
          end
        end

        if (vrf_uncorrectable_fault_i[i]) begin
          if (!SaturatingCount || !(&vrf_uncorrectable_count_o[i])) begin
            vrf_uncorrectable_count_o[i] <= vrf_uncorrectable_count_o[i] + 1'b1;
          end
        end
      end

      for (int unsigned i = 0; i < NumTcdmBanks; i++) begin
        if (tcdm_rd_correctable_fault_i[i]) begin
          if (!SaturatingCount || !(&tcdm_rd_correctable_count_o[i])) begin
            tcdm_rd_correctable_count_o[i] <= tcdm_rd_correctable_count_o[i] + 1'b1;
          end
        end

        if (tcdm_rd_uncorrectable_fault_i[i]) begin
          if (!SaturatingCount || !(&tcdm_rd_uncorrectable_count_o[i])) begin
            tcdm_rd_uncorrectable_count_o[i] <= tcdm_rd_uncorrectable_count_o[i] + 1'b1;
          end
        end

        if (tcdm_scrub_correctable_fault_i[i]) begin
          if (!SaturatingCount || !(&tcdm_scrub_correctable_count_o[i])) begin
            tcdm_scrub_correctable_count_o[i] <= tcdm_scrub_correctable_count_o[i] + 1'b1;
          end
        end

        if (tcdm_scrub_uncorrectable_fault_i[i]) begin
          if (!SaturatingCount || !(&tcdm_scrub_uncorrectable_count_o[i])) begin
            tcdm_scrub_uncorrectable_count_o[i] <= tcdm_scrub_uncorrectable_count_o[i] + 1'b1;
          end
        end
      end
    end
  end

endmodule