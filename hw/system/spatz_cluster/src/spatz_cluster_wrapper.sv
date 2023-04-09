// Copyright 2023 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>

/// Spatz many-core cluster with improved TCDM interconnect.
/// Spatz Cluster Top-Level.
module spatz_cluster_wrapper import spatz_pkg::*; import spatz_cluster_pkg::*; #(
    /// Address from which to fetch the first instructions.
    parameter logic          [31:0] BootAddr          = 32'h0,
    /// Zero memory address region size (in kB).
    parameter int   unsigned        ZeroMemorySize    = 64,
    /// Cluster peripheral address region size (in kB).
    parameter int   unsigned        ClusterPeriphSize = 64
  ) (
    /// System clock.
    input  logic                                   clk_i,
    /// Asynchronous active high reset. This signal is assumed to be _async_.
    input  logic                                   rst_ni,
    /// Per-core debug request signal. Asserting this signals puts the
    /// corresponding core into debug mode. This signal is assumed to be _async_.
    input  logic                [NumCores-1:0]     debug_req_i,
    /// Machine external interrupt pending. Usually those interrupts come from a
    /// platform-level interrupt controller. This signal is assumed to be _async_.
    input  logic                [NumCores-1:0]     meip_i,
    /// Machine timer interrupt pending. Usually those interrupts come from a
    /// core-local interrupt controller such as a timer/RTC. This signal is
    /// assumed to be _async_.
    input  logic                [NumCores-1:0]     mtip_i,
    /// Core software interrupt pending. Usually those interrupts come from
    /// another core to facilitate inter-processor-interrupts. This signal is
    /// assumed to be _async_.
    input  logic                [NumCores-1:0]     msip_i,
    /// First hartid of the cluster. Cores of a cluster are monotonically
    /// increasing without a gap, i.e., a cluster with 8 cores and a
    /// `hart_base_id_i` of 5 get the hartids 5 - 12.
    input  logic                [9:0]              hart_base_id_i,
    /// Base address of cluster. TCDM and cluster peripheral location are derived from
    /// it. This signal is pseudo-static.
    input  logic                [AxiAddrWidth-1:0] cluster_base_addr_i,
    /// Per-cluster probe on the cluster status. Can be written by the cores to indicate
    /// to the overall system that the cluster is executing something.
    output logic                                   cluster_probe_o,
    /// Per-cluster probe on the EOC status. Indicates the end of the execution.
    output logic                                   eoc_o,
    /// AXI Core cluster in-port.
    input  spatz_axi_in_req_t                      axi_in_req_i,
    output spatz_axi_in_resp_t                     axi_in_resp_o,
    /// AXI Core cluster out-port.
    output spatz_axi_out_req_t                     axi_out_req_o,
    input  spatz_axi_out_resp_t                    axi_out_resp_i
  );

  // FPU implementation
  localparam fpnew_pkg::fpu_implementation_t FPUImplementation = '{
    PipeRegs: '{
      '{1, // FP32
        2, // FP64
        0, // FP16
        0, // FP8
        0, // FP16alt
        0  // FP8alt
      },                   // FMA Block
      '{1, 1, 1, 1, 1, 1}, // DIVSQRT
      '{1, 1, 1, 1, 1, 1}, // NONCOMP
      '{2, 2, 2, 2, 2, 2}, // CONV
      '{2, 2, 2, 2, 2, 2}  // DOTP
    },
    UnitTypes: '{
      '{fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED},             // FMA
      '{fpnew_pkg::DISABLED, fpnew_pkg::DISABLED, fpnew_pkg::DISABLED, fpnew_pkg::DISABLED, fpnew_pkg::DISABLED, fpnew_pkg::DISABLED}, // DIVSQRT
      '{fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL, fpnew_pkg::PARALLEL}, // NONCOMP
      '{fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED},             // CONV
      '{fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED, fpnew_pkg::MERGED}              // DOTP
    },
    PipeConfig: fpnew_pkg::BEFORE
  };

  // PMA configuration
  localparam snitch_pma_pkg::snitch_pma_t SnitchPMACfg = '{
    NrCachedRegionRules: 1,
    CachedRegion       : '{default: '0},
    default            : '0
  };

  spatz_cluster #(
    .AxiAddrWidth            (AxiAddrWidth        ),
    .AxiDataWidth            (AxiDataWidth        ),
    .AxiIdWidthIn            (AxiIdInWidth        ),
    .AxiUserWidth            (AxiUserWidth        ),
    .BootAddr                (BootAddr            ),
    .ZeroMemorySize          (ZeroMemorySize      ),
    .ClusterPeriphSize       (ClusterPeriphSize   ),
    .NrCores                 (NumCores            ),
    .TCDMDepth               (TCDMDepth           ),
    .NrBanks                 (NumBanks            ),
    .ICacheLineWidth         (AxiDataWidth        ),
    .ICacheSets              (8                   ),
    .ICacheLineCount         (8                   ),
    .FPUImplementation       (FPUImplementation   ),
    .NumIntOutstandingLoads  (8                   ),
    .NumIntOutstandingMem    (8                   ),
    .NumSpatzOutstandingLoads(8                   ),
    .axi_in_req_t            (spatz_axi_in_req_t  ),
    .axi_in_resp_t           (spatz_axi_in_resp_t ),
    .axi_out_req_t           (spatz_axi_out_req_t ),
    .axi_out_resp_t          (spatz_axi_out_resp_t),
    .SnitchPMACfg            (SnitchPMACfg        )
  ) i_cluster (
    .clk_i              (clk_i              ),
    .rst_ni             (rst_ni             ),
    .debug_req_i        (debug_req_i        ),
    .meip_i             (meip_i             ),
    .mtip_i             (mtip_i             ),
    .msip_i             (msip_i             ),
    .hart_base_id_i     (hart_base_id_i     ),
    .cluster_base_addr_i(cluster_base_addr_i),
    .cluster_probe_o    (cluster_probe_o    ),
    .eoc_o              (eoc_o              ),
    .axi_in_req_i       (axi_in_req_i       ),
    .axi_in_resp_o      (axi_in_resp_o      ),
    .axi_out_req_o      (axi_out_req_o      ),
    .axi_out_resp_i     (axi_out_resp_i     )
  );

endmodule: spatz_cluster_wrapper
