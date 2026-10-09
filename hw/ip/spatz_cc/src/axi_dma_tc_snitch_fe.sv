// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Thomas Benz <tbenz@ethz.ch>

// Implements the tightly-coupled frontend. This module can directly be connected
// to an accelerator bus in the snitch system

`include "common_cells/registers.svh"

module axi_dma_tc_snitch_fe #(
    parameter int  unsigned AddrWidth          = 0,
    parameter int  unsigned DataWidth          = 0,
    parameter int  unsigned DMADataWidth       = 0,
    parameter int  unsigned IdWidth            = 0,
    parameter int  unsigned UserWidth          = 0,
    parameter int  unsigned DMAAxiReqFifoDepth = 3,
    parameter int  unsigned DMAReqFifoDepth    = 3,
    /// Width of one index-stream read from L1/TCDM (indexed/gather mode)
    parameter int  unsigned IndexTcdmDataWidth = 64,
    parameter type          axi_req_t          = logic,
    parameter type          axi_ar_chan_t      = logic,
    parameter type          axi_aw_chan_t      = logic,
    parameter type          axi_res_t          = logic,
    parameter type          acc_resp_t         = logic,
    parameter type          dma_events_t       = logic,
    /// TCDM request/response types for the index-stream read port (same as the
    /// cluster's tcdm_req_t/tcdm_rsp_t so it wires straight into the interconnect)
    parameter type          tcdm_req_t         = logic,
    parameter type          tcdm_rsp_t         = logic,
    /// Derived parameter *Do not override*
    parameter type          addr_t             = logic [AddrWidth-1:0],
    parameter type          data_t             = logic [DataWidth-1:0]
  ) (
    input  logic            clk_i,
    input  logic            rst_ni,
    // AXI4 bus
    output axi_req_t        axi_dma_req_o,
    input  axi_res_t        axi_dma_res_i,
    // debug output
    output logic            dma_busy_o,
    // accelerator interface
    input  logic     [31:0] acc_qaddr_i,
    input  logic     [ 5:0] acc_qid_i,
    input  logic     [31:0] acc_qdata_op_i,
    input  data_t           acc_qdata_arga_i,
    input  data_t           acc_qdata_argb_i,
    input  addr_t           acc_qdata_argc_i,
    input  logic            acc_qvalid_i,
    output logic            acc_qready_o,

    output data_t        acc_pdata_o,
    output logic  [ 5:0] acc_pid_o,
    output logic         acc_perror_o,
    output logic         acc_pvalid_o,
    input  logic         acc_pready_i,

    // hart id of the frankensnitch
    input logic [31:0] hart_id_i,

    // performance output
    output axi_dma_pkg::dma_perf_t dma_perf_o,
    output dma_events_t            dma_events_o,

    // Index-stream read port to L1/TCDM (indexed/gather mode, read-only)
    output tcdm_req_t dma_idx_req_o,
    input  tcdm_rsp_t dma_idx_rsp_i
  );

  typedef logic [IdWidth-1:0] id_t;

  typedef struct packed {
    id_t id;
    addr_t src, dst, num_bytes;
    axi_pkg::cache_t cache_src, cache_dst;
    axi_pkg::burst_t burst_src, burst_dst;
    logic decouple_rw;
    logic deburst;
    logic serialize;
  } burst_req_t;

  typedef struct packed {
    id_t id;
    addr_t src, dst, num_bytes;
    axi_pkg::cache_t cache_src, cache_dst;
    addr_t stride_src, stride_dst, num_repetitions;
    axi_pkg::burst_t burst_src, burst_dst;
    logic decouple_rw;
    logic deburst;
    logic is_twod;
    // indexed (gather) extension:
    //   is_gather=1 -> src_addr[i] = src + idx[i]*stride_src, dst_addr[i] = dst + i*stride_dst,
    //   num_repetitions = index count (G), idx stream read from idx_addr in L1.
    addr_t      idx_addr;
    logic [1:0] idx_width;   // 00/01/10/11 -> 8/16/32/64-bit index elements
    logic       is_gather;
  } twod_req_t;

  //--------------------------------------
  // Backend Instanciation
  //--------------------------------------
  logic       backend_idle;
  burst_req_t burst_req;
  logic       burst_req_valid;
  logic       burst_req_ready;
  logic       oned_trans_complete;

  // typedefs
  typedef logic [AddrWidth-1:0] int_addr_t;
  typedef logic [AddrWidth-1:0] int_tf_len_t;
  typedef logic [IdWidth-1:0] int_id_t;

  // iDMA request / response types
  `include "idma/typedef.svh"
  `IDMA_TYPEDEF_FULL_REQ_T(idma_req_t, int_id_t, int_addr_t, int_tf_len_t)
  `IDMA_TYPEDEF_FULL_RSP_T(idma_rsp_t, int_addr_t)

  // local signals
  idma_req_t idma_req;
  logic      idma_rsp_valid;
  idma_pkg::idma_busy_t idma_busy;

  // busy if at least one of the sub-units is busy
  assign backend_idle = ~|idma_busy;

  // assemble the new request from the old
  always_comb begin : proc_idma_req
    idma_req = '0;

    idma_req.length   = burst_req.num_bytes;
    idma_req.src_addr = burst_req.src;
    idma_req.dst_addr = burst_req.dst;

    idma_req.opt.axi_id             = burst_req.id;
    // DMA only supports incremental burst
    idma_req.opt.src.burst          = axi_pkg::BURST_INCR; // burst_req.burst_src;
    idma_req.opt.src.cache          = burst_req.cache_src;
    // AXI4 does not support locked transactions, use atomics
    idma_req.opt.src.lock           = '0;
    // unpriviledged, secure, data access
    idma_req.opt.src.prot           = '0;
    // not participating in qos
    idma_req.opt.src.qos            = '0;
    // only one region
    idma_req.opt.src.region         = '0;
    // DMA only supports incremental burst
    idma_req.opt.dst.burst          = axi_pkg::BURST_INCR; // burst_req.burst_dst;
    idma_req.opt.dst.cache          = burst_req.cache_dst;
    // AXI4 does not support locked transactions, use atomics
    idma_req.opt.dst.lock           = '0;
    // unpriviledged, secure, data access
    idma_req.opt.dst.prot           = '0;
    // not participating in qos
    idma_req.opt.dst.qos            = '0;
    // only one region in system
    idma_req.opt.dst.region         = '0;
    // ensure coupled AW to avoid deadlocks
    idma_req.opt.beo.decouple_aw    = '0;
    idma_req.opt.beo.decouple_rw    = burst_req.decouple_rw;
    // this compatibility layer only supports completely debursting
    idma_req.opt.beo.src_max_llen   = '0;
    // this compatibility layer only supports completely debursting
    idma_req.opt.beo.dst_max_llen   = '0;
    idma_req.opt.beo.src_reduce_len = burst_req.deburst;
    idma_req.opt.beo.dst_reduce_len = burst_req.deburst;
  end

  // transfer is completed if response is valid (there is no error handling)
  assign oned_trans_complete = idma_rsp_valid;

 typedef struct packed {
    axi_ar_chan_t ar_chan;
  } axi_read_meta_channel_t;

  typedef struct packed {
    axi_read_meta_channel_t axi;
  } read_meta_channel_t;

  typedef struct packed {
    axi_aw_chan_t aw_chan;
  } axi_write_meta_channel_t;

  typedef struct packed {
    axi_write_meta_channel_t axi;
  } write_meta_channel_t;

  // Internal AXI channels
  axi_req_t axi_read_req, axi_write_req;
  axi_res_t axi_read_rsp, axi_write_rsp;

  idma_backend_rw_axi #(
    .CombinedShifter      ( 1'b0                        ),
    .PrintFifoInfo        ( 0                           ),
    .DataWidth            ( DMADataWidth                ),
    .AddrWidth            ( AddrWidth                   ),
    .UserWidth            ( UserWidth                   ),
    .AxiIdWidth           ( IdWidth                     ),
    .NumAxInFlight        ( DMAAxiReqFifoDepth          ),
    .BufferDepth          ( 3                           ),
    .TFLenWidth           ( AddrWidth                   ),
    .RAWCouplingAvail     ( 1                           ),
    .MaskInvalidData      ( 1                           ),
    .HardwareLegalizer    ( 1                           ),
    .RejectZeroTransfers  ( 1                           ),
    .MemSysDepth          ( 0                           ),
    .ErrorCap             ( idma_pkg::NO_ERROR_HANDLING ),
    .idma_req_t           ( idma_req_t                  ),
    .idma_rsp_t           ( idma_rsp_t                  ),
    .idma_eh_req_t        ( idma_pkg::idma_eh_req_t     ),
    .idma_busy_t          ( idma_pkg::idma_busy_t       ),
    .axi_req_t            ( axi_req_t                   ),
    .axi_rsp_t            ( axi_res_t                   ),
    .write_meta_channel_t ( write_meta_channel_t        ),
    .read_meta_channel_t  ( read_meta_channel_t         )
  ) i_idma_backend (
    .clk_i           ( clk_i               ),
    .rst_ni          ( rst_ni              ),
    .testmode_i      ( 1'b0                ),
    .idma_req_i      ( idma_req            ),
    .req_valid_i     ( burst_req_valid     ),
    .req_ready_o     ( burst_req_ready     ),
    .idma_rsp_o      ( /* NOT CONNECTED */ ),
    .rsp_valid_o     ( idma_rsp_valid      ), // valid_o signals a completed transfer
    .rsp_ready_i     ( 1'b1                ), // always ready for complete transfers
    .idma_eh_req_i   ( '0                  ), // No error handling hardware is present
    .eh_req_valid_i  ( 1'b1                ),
    .eh_req_ready_o  ( /* NOT CONNECTED */ ),
    .axi_read_req_o  ( axi_read_req        ),
    .axi_read_rsp_i  ( axi_read_rsp        ),
    .axi_write_req_o ( axi_write_req       ),
    .axi_write_rsp_i ( axi_write_rsp       ),
    .busy_o          ( idma_busy           )
  );

  axi_rw_join #(
   .axi_req_t   ( axi_req_t ),
   .axi_resp_t  ( axi_res_t )
  ) i_axi_rw_join (
   .clk_i,
   .rst_ni,
   .slv_read_req_i    ( axi_read_req  ),
   .slv_read_resp_o   ( axi_read_rsp  ),
   .slv_write_req_i   ( axi_write_req ),
   .slv_write_resp_o  ( axi_write_rsp ),
   .mst_req_o         ( axi_dma_req_o ),
   .mst_resp_i        ( axi_dma_res_i )
  );

  //--------------------------------------
  // 2D Extension
  //--------------------------------------
  twod_req_t twod_req_d, twod_req_q;
  logic      twod_req_valid;
  logic      twod_req_ready;
  logic      twod_req_last;

  //--------------------------------------
  // Launch routing: is_gather -> gather engine, else -> 2D engine
  //--------------------------------------
  // NOTE (stage 1): SW must not have 1D/2D and gather transfers in flight at the
  // same time (wait_all between mode switches). Only one engine is ever active,
  // so the burst-stream mux below never sees both sources valid at once.
  logic is_gather_launch;
  logic twod_ext_valid, gather_ext_valid;
  logic twod_ext_ready, gather_ext_ready;

  assign is_gather_launch = twod_req_d.is_gather;
  assign twod_ext_valid   = twod_req_valid & ~is_gather_launch;
  assign gather_ext_valid = twod_req_valid &  is_gather_launch;
  assign twod_req_ready   = is_gather_launch ? gather_ext_ready : twod_ext_ready;

  // Per-engine burst streams, merged into the single backend request.
  burst_req_t twod_burst_req,   gather_burst_req;
  logic       twod_burst_valid, gather_burst_valid;
  logic       twod_burst_ready, gather_burst_ready;
  logic       twod_burst_last,  gather_burst_last;

  // gather takes priority if both were ever valid (must not happen — see NOTE)
  assign burst_req_valid    = twod_burst_valid | gather_burst_valid;
  assign burst_req          = gather_burst_valid ? gather_burst_req  : twod_burst_req;
  assign twod_req_last      = gather_burst_valid ? gather_burst_last : twod_burst_last;
  assign gather_burst_ready = burst_req_ready &  gather_burst_valid;
  assign twod_burst_ready   = burst_req_ready & ~gather_burst_valid;

  axi_dma_twod_ext #(
    .ADDR_WIDTH     ( AddrWidth       ),
    .REQ_FIFO_DEPTH ( DMAReqFifoDepth ),
    .burst_req_t    ( burst_req_t     ),
    .twod_req_t     ( twod_req_t      )
  ) i_axi_dma_twod_ext (
    .clk_i             ( clk_i            ),
    .rst_ni            ( rst_ni           ),
    .twod_req_i        ( twod_req_d       ),
    .twod_req_valid_i  ( twod_ext_valid   ),
    .twod_req_ready_o  ( twod_ext_ready   ),
    .burst_req_o       ( twod_burst_req   ),
    .burst_req_valid_o ( twod_burst_valid ),
    .burst_req_ready_i ( twod_burst_ready ),
    .twod_req_last_o   ( twod_burst_last  )
  );

  //--------------------------------------
  // Indexed (gather) Extension
  //--------------------------------------
  // The engine uses a protocol-neutral req/gnt/rvalid micro-interface; adapt it
  // here to the cluster's TCDM request/response type (read-only) so it wires
  // straight into the TCDM interconnect.
  logic                          idx_req, idx_gnt, idx_rvalid;
  addr_t                         idx_addr;
  logic [IndexTcdmDataWidth-1:0] idx_rdata;

  always_comb begin : proc_idx_tcdm_req
    dma_idx_req_o         = '0;
    dma_idx_req_o.q.addr  = idx_addr[$bits(dma_idx_req_o.q.addr)-1:0]; // TCDM-relative (truncated)
    dma_idx_req_o.q.write = 1'b0;
    dma_idx_req_o.q.amo   = reqrsp_pkg::AMONone;
    dma_idx_req_o.q.data  = '0;
    dma_idx_req_o.q.strb  = '0;
    dma_idx_req_o.q.user  = '0;
    dma_idx_req_o.q_valid = idx_req;
  end
  assign idx_gnt    = dma_idx_rsp_i.q_ready;   // grant = q_ready
  assign idx_rvalid = dma_idx_rsp_i.p_valid;
  assign idx_rdata  = dma_idx_rsp_i.p.data;

  axi_dma_gather_ext #(
    .ADDR_WIDTH     ( AddrWidth          ),
    .REQ_FIFO_DEPTH ( DMAReqFifoDepth    ),
    .TcdmDataWidth  ( IndexTcdmDataWidth ),
    .burst_req_t    ( burst_req_t        ),
    .gather_req_t   ( twod_req_t         )
  ) i_axi_dma_gather_ext (
    .clk_i              ( clk_i              ),
    .rst_ni             ( rst_ni             ),
    .burst_req_o        ( gather_burst_req   ),
    .burst_req_valid_o  ( gather_burst_valid ),
    .burst_req_ready_i  ( gather_burst_ready ),
    .gather_req_i       ( twod_req_d         ),
    .gather_req_valid_i ( gather_ext_valid   ),
    .gather_req_ready_o ( gather_ext_ready   ),
    .gather_req_last_o  ( gather_burst_last  ),
    .idx_req_o          ( idx_req            ),
    .idx_addr_o         ( idx_addr           ),
    .idx_gnt_i          ( idx_gnt            ),
    .idx_rvalid_i       ( idx_rvalid         ),
    .idx_rdata_i        ( idx_rdata          )
  );

  //--------------------------------------
  // Buffer twod last
  //--------------------------------------
  localparam int unsigned TwodBufferDepth = 2 * (DMAReqFifoDepth +
    DMAAxiReqFifoDepth) + 3 + 1;
  logic twod_req_last_realigned;
  fifo_v3 # (
    .DATA_WIDTH ( 1               ),
    .DEPTH      ( TwodBufferDepth )
  ) i_fifo_v3_last_twod_buffer (
    .clk_i      ( clk_i                             ),
    .rst_ni     ( rst_ni                            ),
    .flush_i    ( 1'b0                              ),
    .testmode_i ( 1'b0                              ),
    .full_o     (                                   ),
    .empty_o    (                                   ),
    .usage_o    (                                   ),
    .data_i     ( twod_req_last                     ),
    .push_i     ( burst_req_valid & burst_req_ready ),
    .data_o     ( twod_req_last_realigned           ),
    .pop_i      ( oned_trans_complete               )
  );

  //--------------------------------------
  // ID gen
  //--------------------------------------
  logic [31:0] next_id;
  logic [31:0] completed_id;

  `FFL(next_id, next_id + 'h1, twod_req_valid & twod_req_ready, 0)
  `FFL(completed_id, completed_id + 'h1, oned_trans_complete & twod_req_last_realigned, 0)

  // dma is busy when it is not idle
  assign dma_busy_o = next_id != completed_id;

  //--------------------------------------
  // Performance counters
  //--------------------------------------
  axi_dma_perf_counters #(
    .TRANSFER_ID_WIDTH ( 32           ),
    .DATA_WIDTH        ( DMADataWidth ),
    .axi_req_t         ( axi_req_t    ),
    .axi_res_t         ( axi_res_t    ),
    .dma_events_t      ( dma_events_t )
  ) i_axi_dma_perf_counters (
    .clk_i          ( clk_i         ),
    .rst_ni         ( rst_ni        ),
    .axi_dma_req_i  ( axi_dma_req_o ),
    .axi_dma_res_i  ( axi_dma_res_i ),
    .next_id_i      ( next_id       ),
    .completed_id_i ( completed_id  ),
    .dma_busy_i     ( dma_busy_o    ),
    .dma_perf_o     ( dma_perf_o    ),
    .dma_events_o   ( dma_events_o  )
  );

  //--------------------------------------
  // Spill register for response channel
  //--------------------------------------
  acc_resp_t acc_pdata_spill, acc_pdata;
  logic      acc_pvalid_spill;
  logic      acc_pready_spill;

  // the response path needs to be decoupled
  spill_register #(
    .T ( acc_resp_t )
  ) i_spill_register_dma_resp (
    .clk_i   ( clk_i            ),
    .rst_ni  ( rst_ni           ),
    .valid_i ( acc_pvalid_spill ),
    .ready_o ( acc_pready_spill ),
    .data_i  ( acc_pdata_spill  ),
    .valid_o ( acc_pvalid_o     ),
    .ready_i ( acc_pready_i     ),
    .data_o  ( acc_pdata        )
  );

  assign acc_pdata_o  = acc_pdata.data;
  assign acc_pid_o    = acc_pdata.id;
  assign acc_perror_o = acc_pdata.error;

  //--------------------------------------
  // Instruction decode
  //--------------------------------------
  logic            is_dma_op;
  logic [12*8-1:0] dma_op_name;

  always_comb begin : proc_fe_inst_decode
    twod_req_d            = twod_req_q;
    twod_req_d.burst_src  = axi_pkg::BURST_INCR;
    twod_req_d.burst_dst  = axi_pkg::BURST_INCR;
    twod_req_d.cache_src  = axi_pkg::CACHE_MODIFIABLE;
    twod_req_d.cache_dst  = axi_pkg::CACHE_MODIFIABLE;
    twod_req_valid        = 1'b0;
    acc_qready_o          = 1'b0;
    acc_pdata_spill       = '0;
    acc_pdata_spill.error = 1'b1;
    acc_pvalid_spill      = 1'b0;

    // debug signal
    is_dma_op   = 1'b0;
    dma_op_name = "Invalid";

    // decode
    if (acc_qvalid_i == 1'b1) begin
      unique casez (acc_qdata_op_i)

        // manipulate the source register
        riscv_instr::DMSRC : begin
          twod_req_d.src[31: 0] = acc_qdata_arga_i[31:0];
          acc_qready_o          = 1'b1;
          is_dma_op             = 1'b1;
          dma_op_name           = "DMSRC";
        end

        // manipulate the destination register
        riscv_instr::DMDST : begin
          twod_req_d.dst[31: 0] = acc_qdata_arga_i[31:0];
          acc_qready_o          = 1'b1;
          is_dma_op             = 1'b1;
          dma_op_name           = "DMDST";
        end

        // start the DMA
        riscv_instr::DMCPYI,
        riscv_instr::DMCPY : begin
          automatic logic [4:0] cfg;

          // Parse the transfer parameters from the register or immediate.
          // cfg[0]=decouple_rw, cfg[1]=is_twod, cfg[2]=is_gather,
          // cfg[3]=direction (0=gather/1=scatter, reserved), cfg[4]=reserved.
          cfg = '0;
          unique casez (acc_qdata_op_i)
            riscv_instr::DMCPYI : cfg = acc_qdata_op_i[24:20];
            riscv_instr::DMCPY  : cfg = acc_qdata_argb_i;
            default:;
          endcase
          dma_op_name = "DMCPY";
          is_dma_op   = 1'b1;

          twod_req_d.num_bytes   = acc_qdata_arga_i;
          twod_req_d.decouple_rw = cfg[0];
          twod_req_d.is_twod     = cfg[1];
          twod_req_d.is_gather   = cfg[2];

          // Perform the following sequence:
          // 1. wait for acc response channel to be ready (pready)
          // 2. request twod transfer (valid)
          // 3. wait for twod transfer to be accepted (ready)
          // 4. send acc response (pvalid)
          // 5. acknowledge acc request (qready)
          if (acc_pready_spill) begin
            twod_req_valid = 1'b1;
            if (twod_req_ready) begin
              acc_pdata_spill.id    = acc_qid_i;
              acc_pdata_spill.data  = next_id;
              acc_pdata_spill.error = 1'b0;
              acc_pvalid_spill      = 1'b1;
              acc_qready_o          = twod_req_ready;
            end
          end
        end

        // status of the DMA
        riscv_instr::DMSTATI,
        riscv_instr::DMSTAT: begin
          automatic logic [1:0] status;

          // Parse the status index from the register or immediate.
          status = '0;
          unique casez (acc_qdata_op_i)
            riscv_instr::DMSTATI: status = acc_qdata_op_i[24:20];
            riscv_instr::DMSTAT : status = acc_qdata_argb_i;
            default:;
          endcase
          dma_op_name = "DMSTAT";
          is_dma_op   = 1'b1;

          // Compose the response.
          acc_pdata_spill.id    = acc_qid_i;
          acc_pdata_spill.error = 1'b0;
          case (status)
            2'b00 : acc_pdata_spill.data = completed_id;
            2'b01 : acc_pdata_spill.data = next_id;
            2'b10 : acc_pdata_spill.data = {{{8'd63}{1'b0}}, dma_busy_o};
            2'b11 : acc_pdata_spill.data = {{{8'd63}{1'b0}}, !twod_req_ready};
            default:;
          endcase

          // Wait for acc response channel to become ready, then ack the
          // request.
          if (acc_pready_spill) begin
            acc_pvalid_spill = 1'b1;
            acc_qready_o     = 1'b1;
          end
        end

        // manipulate the strides
        riscv_instr::DMSTR : begin
          twod_req_d.stride_src = acc_qdata_arga_i;
          twod_req_d.stride_dst = acc_qdata_argb_i;
          acc_qready_o          = 1'b1;
          is_dma_op             = 1'b1;
          dma_op_name           = "DMSTR";
        end

        // manipulate the strides
        riscv_instr::DMREP : begin
          twod_req_d.num_repetitions = acc_qdata_arga_i;
          acc_qready_o               = 1'b1;
          is_dma_op                  = 1'b1;
          dma_op_name                = "DMREP";
        end

        // set the index-stream base + element width (indexed/gather mode)
        riscv_instr::DMIDX : begin
          twod_req_d.idx_addr   = acc_qdata_arga_i;         // rs1: index stream base in L1
          twod_req_d.idx_width  = acc_qdata_op_i[21:20];    // imm[1:0]: element width
          acc_qready_o          = 1'b1;
          is_dma_op             = 1'b1;
          dma_op_name           = "DMIDX";
        end

        default:;
      endcase
    end
  end

  //--------------------------------------
  // State
  //--------------------------------------
  `FF(twod_req_q, twod_req_d, '0)

endmodule
