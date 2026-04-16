// Copyright (c) 2025 ETH Zurich.
// Tim Fischer <fischeti@iis.ee.ethz.ch>

module tc_sram
  import tsmc7_pkg::*;
#(
  parameter int unsigned NumWords     = 32'd1024, // Number of Words in data array
  parameter int unsigned DataWidth    = 32'd128,  // Data signal width
  parameter int unsigned ByteWidth    = 32'd8,    // Width of a data byte
  parameter int unsigned NumPorts     = 32'd2,    // Number of read and write ports
  parameter int unsigned Latency      = 32'd1,    // Latency when the read data is available
  parameter              SimInit      = "none",   // Simulation initialization
  parameter bit          PrintSimCfg  = 1'b0,     // Print configuration
  parameter              ImplKey      = "none",   // Reference to specific implementation
  parameter type         impl_in_t    = logic,    // Type for implementation inputs
  parameter type         impl_out_t   = logic,    // Type for implementation outputs
  parameter impl_out_t   ImplOutSim   = 'X,       // Implementation output in simulation
  // DEPENDENT PARAMETERS, DO NOT OVERWRITE!
  parameter int unsigned AddrWidth = (NumWords > 32'd1) ? $clog2(NumWords) : 32'd1,
  parameter int unsigned BeWidth   = (DataWidth + ByteWidth - 32'd1) / ByteWidth, // ceil_div
  parameter type         addr_t    = logic [AddrWidth-1:0],
  parameter type         data_t    = logic [DataWidth-1:0],
  parameter type         be_t      = logic [BeWidth-1:0]
) (
  input  logic  [NumPorts-1:0] clk_i,      // Clock
  input  logic  [NumPorts-1:0] rst_ni,     // Asynchronous reset active low
  // input ports
  input  logic  [NumPorts-1:0] req_i,      // request
  input  logic  [NumPorts-1:0] we_i,       // write enable
  input  addr_t [NumPorts-1:0] addr_i,     // request address
  input  data_t [NumPorts-1:0] wdata_i,    // write data
  input  be_t   [NumPorts-1:0] be_i,       // write byte enable
  // output ports
  output data_t [NumPorts-1:0] rdata_o     // read data
);

  // Generate bitwise write mask
  data_t [NumPorts-1:0] bit_wm;

  for (genvar p = 0; p < NumPorts; ++p) begin : gen_port_wms
    for (genvar i = 0; i < DataWidth; ++i) begin : gen_bit_wm
      // Ensure elaboration-time resolution of BE bit (integer dividers bad)
      localparam int unsigned BeIdx = i/ByteWidth;
      assign bit_wm[p][i] = be_i[p][BeIdx];
    end
  end

  // Generate clock gating for each port
  logic [NumPorts-1:0] sram_active_q;
  logic [NumPorts-1:0] sram_gated_clk;

  for (genvar p = 0; p < NumPorts; ++p) begin : gen_port_clk_gate

    tc_clk_gating i_ckg (
      .clk_i    (clk_i[p]                       ),
      .test_en_i(1'b0                           ),
      .en_i     (req_i[p]  || sram_active_q[p]  ),
      .clk_o    (sram_gated_clk[p]                    )
    );

    always_ff @(posedge clk_i or negedge rst_ni) begin
      if (~rst_ni) begin
        sram_active_q[p] <= 1'b0;
      end else begin
        sram_active_q[p] <= req_i[p];
      end
    end
  end

  `define TC_SRAM_SP_DEFAULT_CLK sram_gated_clk[0]
  `define TC_SRAM_SP_DEFAULT_BWEB bit_wm[0]
  `include "sram_sv_macros.svh"

  // Snitch Cluster - I$ tag
  if (NumWords == 128 && DataWidth == 38 && Latency == 1 && NumPorts == 1) begin : gen_128x38m4
    `TSMC7_SPSBSRAM(128, 38, 4, DefaultSpSbSramCtrl)
  end else
  // Snitch Cluster - I$ data & FhG SPU - I$ data
  if (NumWords == 128 && DataWidth == 256 && Latency == 1 && NumPorts == 1) begin : gen_128x256m1
    `TSMC7_SPSBSRAM(128, 256, 2, DefaultSpSbSramCtrl)
  end else
  // Snitch Cluster - TCDM & FhG SPU - TCDM
  if (NumWords == 512 && DataWidth == 64 && Latency == 1 && NumPorts == 1) begin : gen_512x64m4
    `TSMC7_L1CACHE(512, 64, 4, DefaultL1CacheCtrl)
  end else
    // CVA6 - I$ data
  if (NumWords == 256 && DataWidth == 128 && Latency == 1 && NumPorts == 1) begin : gen_256x128m4
    `TSMC7_SPSBSRAM(256, 128, 4, DefaultSpSbSramCtrl)
  end else
  // CVA6 - I$ tag
  if (NumWords == 256 && DataWidth == 45 && Latency == 1 && NumPorts == 1) begin : gen_256x45m4
    `TSMC7_SPSBSRAM(256, 45, 4, DefaultSpSbSramCtrl)
  end else
  // CVA6 - D$ tag
  if (NumWords == 256 && DataWidth == 44 && Latency == 1 && NumPorts == 1) begin : gen_256x45m4
    `TSMC7_SPSBSRAM_WORD_PADDING(256, 45, 44, 4, DefaultSpSbSramCtrl)
  end else
  // CVA6 - D$ meta
  if (NumWords == 256 && DataWidth == 136 && Latency == 1 && NumPorts == 1) begin : gen_256x136m4
    `TSMC7_SPSBSRAM(256, 136, 4, DefaultSpSbSramCtrl)
  end else
  // Cheshire - LLC I$ data
  if (NumWords == 2048 && DataWidth == 64 && Latency == 1 && NumPorts == 1) begin : gen_2048x64m8
    `TSMC7_SPSBSRAM(2048, 64, 8, DefaultSpSbSramCtrl)
  end else
  // Cheshire - LLC I$ tag
  if (NumWords == 256 && DataWidth == 36 && Latency == 1 && NumPorts == 1) begin : gen_256x36m4
    `TSMC7_SPSBSRAM(256, 36, 4, DefaultSpSbSramCtrl)
  end else
  // L2 Memtile - SPM
  if (NumWords == 1024 && DataWidth == 128 && Latency == 1 && NumPorts == 1) begin : gen_1024x128m4
    `TSMC7_SPSBSRAM(1024, 128, 4, DefaultSpSbSramCtrl)
  end else
  // FhG SPU - imem_sag
  if (NumWords == 256 && DataWidth == 64 && Latency == 1 && NumPorts == 1) begin : gen_256x64m4
    `TSMC7_SPSBSRAM(256, 64, 4, DefaultSpSbSramCtrl)
  end else
  // FhG SPU - imem_au
  if (NumWords == 256 && DataWidth == 32 && Latency == 1 && NumPorts == 1) begin : gen_256x32m4
    `TSMC7_SPSBSRAM(256, 32, 4, DefaultSpSbSramCtrl)
  end else
  // FhG SPU - I$ tag
  if (NumWords == 128 && DataWidth == 39 && Latency == 1 && NumPorts == 1) begin : gen_128x40m4
    `TSMC7_SPSBSRAM_WORD_PADDING(128, 40, 39, 4, DefaultSpSbSramCtrl)
  end else begin : gen_error

     // This cannot be elaborated since it is a recursive instantiation.
     // Fusion compiler throws an error, and also shows a traceback of the
     // actual model instantiation which fails.
     tc_sram #(
       .NumWords   (NumWords),
       .DataWidth  (DataWidth),
       .ByteWidth  (ByteWidth),
       .NumPorts   (NumPorts),
       .Latency    (Latency),
       .SimInit    (SimInit),
       .PrintSimCfg(PrintSimCfg),
       .ImplKey    (ImplKey),
       .AddrWidth  (AddrWidth),
       .BeWidth    (BeWidth),
       .addr_t     (addr_t),
       .data_t     (data_t),
       .be_t       (be_t)
     ) i_error (
       .*
     );
    // pragma translate_off
    $fatal(1, "NumWords: %d, DataWidth: %d, ByteWidth: %d, NumPorts: %d", NumWords, DataWidth, ByteWidth, NumPorts);
    // pragma translate_on
  end

endmodule

module tc_sram_impl #(
  parameter int unsigned NumWords     = 32'd1024, // Number of Words in data array
  parameter int unsigned DataWidth    = 32'd128,  // Data signal width
  parameter int unsigned ByteWidth    = 32'd8,    // Width of a data byte
  parameter int unsigned NumPorts     = 32'd2,    // Number of read and write ports
  parameter int unsigned Latency      = 32'd1,    // Latency when the read data is available
  parameter              SimInit      = "none",   // Simulation initialization
  parameter bit          PrintSimCfg  = 1'b0,     // Print configuration
  parameter              ImplKey      = "none",   // Reference to specific implementation
  parameter type         impl_in_t    = logic,    // Type for implementation inputs
  parameter type         impl_out_t   = logic,    // Type for implementation outputs
  parameter impl_out_t   ImplOutSim   = 'X,       // Implementation output in simulation
  // DEPENDENT PARAMETERS, DO NOT OVERWRITE!
  parameter int unsigned AddrWidth = (NumWords > 32'd1) ? $clog2(NumWords) : 32'd1,
  parameter int unsigned BeWidth   = (DataWidth + ByteWidth - 32'd1) / ByteWidth, // ceil_div
  parameter type         addr_t    = logic [AddrWidth-1:0],
  parameter type         data_t    = logic [DataWidth-1:0],
  parameter type         be_t      = logic [BeWidth-1:0]
) (
  input  logic                 clk_i,      // Clock
  input  logic                 rst_ni,     // Asynchronous reset active low
  // implementation-related IO
  input  impl_in_t             impl_i,
  output impl_out_t            impl_o,
  // input ports
  input  logic  [NumPorts-1:0] req_i,      // request
  input  logic  [NumPorts-1:0] we_i,       // write enable
  input  addr_t [NumPorts-1:0] addr_i,     // request address
  input  data_t [NumPorts-1:0] wdata_i,    // write data
  input  be_t   [NumPorts-1:0] be_i,       // write byte enable
  // output ports
  output data_t [NumPorts-1:0] rdata_o     // read data
);

  // We drive a static value for `impl_o` in behavioral simulation.
  assign impl_o = ImplOutSim;

  tc_sram #(
    .NumWords     ( NumWords    ),
    .DataWidth    ( DataWidth   ),
    .ByteWidth    ( ByteWidth   ),
    .NumPorts     ( NumPorts    ),
    .Latency      ( Latency     ),
    .SimInit      ( SimInit     ),
    .PrintSimCfg  ( PrintSimCfg ),
    .ImplKey      ( ImplKey     )
  ) i_tc_sram (
    .clk_i,
    .rst_ni,
    .req_i,
    .we_i,
    .addr_i,
    .wdata_i,
    .be_i,
    .rdata_o
  );

endmodule
