// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

module spatz_fault_monitor #(
  parameter int unsigned NumVrfUnits     = 1, // == NrCores; also used to size the
                                               // per-core FPU-dup/TMR/lockstep-core
                                               // fault sources below, since every
                                               // core has exactly one VRF, one VFU
                                               // (FPU-dup pair) and one lockstep
                                               // Snitch core.
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

  // Per-core FPU duplication (DMR) faults: a mismatch between the two
  // fpnew_top copies inside spatz_fpu_dmr. Always uncorrectable-class
  // (DMR alone cannot tell which copy is right).
  input  logic [NumVrfUnits-1:0]  fpu_dup_fault_i,

  // Per-core TMR mismatch (bitwise_TMR_voter_fail on a persistent
  // single-bit control flag or an FSM state register disagreeing across
  // its three replicas) -- self-corrected by the voter the same cycle
  // it's detected, so this is diagnostic/counter-only, same as e2e's
  // convention for TMR-voter mismatches (they never trigger the
  // uncorrectable-fault recovery interrupt -- see spatz_cluster_peripheral).
  input  logic [NumVrfUnits-1:0]  handshake_tmr_fault_i,

  // Per-core lockstep (triplicated) Snitch core replica mismatch. Also
  // self-corrected by majority vote; counter-only, same as above.
  input  logic [NumVrfUnits-1:0]  core_tmr_fault_i,

  // Per-source counters.
  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  vrf_correctable_count_o,
  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  vrf_uncorrectable_count_o,

  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_rd_correctable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_rd_uncorrectable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_scrub_correctable_count_o,
  output logic [NumTcdmBanks-1:0][CounterWidth-1:0] tcdm_scrub_uncorrectable_count_o,

  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  fpu_dup_fault_count_o,

  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  handshake_tmr_count_o,
  output logic [NumVrfUnits-1:0][CounterWidth-1:0]  core_tmr_count_o
);

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      vrf_correctable_count_o    <= '0;
      vrf_uncorrectable_count_o  <= '0;
      tcdm_rd_correctable_count_o   <= '0;
      tcdm_rd_uncorrectable_count_o <= '0;
      tcdm_scrub_correctable_count_o   <= '0;
      tcdm_scrub_uncorrectable_count_o <= '0;
      fpu_dup_fault_count_o      <= '0;
      handshake_tmr_count_o     <= '0;
      core_tmr_count_o          <= '0;

    end else if (clear_i) begin
      vrf_correctable_count_o    <= '0;
      vrf_uncorrectable_count_o  <= '0;
      tcdm_rd_correctable_count_o   <= '0;
      tcdm_rd_uncorrectable_count_o <= '0;
      tcdm_scrub_correctable_count_o   <= '0;
      tcdm_scrub_uncorrectable_count_o <= '0;
      fpu_dup_fault_count_o      <= '0;
      handshake_tmr_count_o     <= '0;
      core_tmr_count_o          <= '0;

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

        if (fpu_dup_fault_i[i]) begin
          if (!SaturatingCount || !(&fpu_dup_fault_count_o[i])) begin
            fpu_dup_fault_count_o[i] <= fpu_dup_fault_count_o[i] + 1'b1;
          end
        end

        if (handshake_tmr_fault_i[i]) begin
          if (!SaturatingCount || !(&handshake_tmr_count_o[i])) begin
            handshake_tmr_count_o[i] <= handshake_tmr_count_o[i] + 1'b1;
          end
        end

        if (core_tmr_fault_i[i]) begin
          if (!SaturatingCount || !(&core_tmr_count_o[i])) begin
            core_tmr_count_o[i] <= core_tmr_count_o[i] + 1'b1;
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
