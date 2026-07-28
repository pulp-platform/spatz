// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

module spatz_mempool_cc
  import snitch_pkg::meta_id_t;
#(
  parameter logic [31:0] BootAddr   = 32'h0000_1000,
  parameter logic [31:0] MTVEC      = BootAddr,
  parameter bit          RVE        = 0,  // Reduced-register extension
  parameter bit          RVM        = 1,  // Enable IntegerMmultiplication & Division Extension
  parameter bit          RVV        = 0,  // Enable Vector Extension
  parameter bit          XFVEC      = 0,
  parameter bit          XFDOTP     = 0,
  parameter bit          XFAUX      = 0,
  /// Enable F Extension.
  parameter bit          RVF        = 0,
  /// Enable D Extension.
  parameter bit          RVD        = 0,
  parameter bit          XF16       = 0,
  parameter bit          XF16ALT    = 0,
  parameter bit          XF8        = 0,
  parameter bit          XF8ALT     = 0,
  /// Enable div/sqrt unit (buggy - use with caution)
  parameter bit          XDivSqrt   = 0,
  parameter bit RegisterOffloadReq  = 1,
  parameter bit RegisterOffloadResp = 1,
  parameter bit RegisterTCDMReq     = 0,
  parameter bit RegisterTCDMResp    = 0,

  parameter int unsigned        TCDMPorts              = 1,
  parameter int unsigned        NumMemPortsPerSpatz    = 1,
  // Usable remote resp ports for the burst-receive spread (threaded down to the VLSU).
  parameter int unsigned        NumRespPorts           = 1
) (
  input  logic                                        clk_i,
  input  logic                                        rst_i,
  input  logic [31:0]                                 hart_id_i,
  // Instruction Port
  output logic [31:0]                                 inst_addr_o,
  input  logic [31:0]                                 inst_data_i,
  output logic                                        inst_valid_o,
  input  logic                                        inst_ready_i,
  // TCDM Ports
  output logic      [TCDMPorts-1:0][31:0]   data_qaddr_o,
  output logic      [TCDMPorts-1:0]         data_qwrite_o,
  output logic      [TCDMPorts-1:0][3:0]    data_qamo_o,
  output logic      [TCDMPorts-1:0][31:0]   data_qdata_o,
  output logic      [TCDMPorts-1:0][3:0]    data_qstrb_o,
  output meta_id_t  [TCDMPorts-1:0]         data_qid_o,
  output logic      [TCDMPorts-1:0][snitch_pkg::BurstLenWidth-1:0] data_qburst_len_o,
  output logic      [TCDMPorts-1:0]         data_qvalid_o,
  input  logic      [TCDMPorts-1:0]         data_qready_i,
  input  logic      [TCDMPorts-1:0][31:0]   data_pdata_i,
  input  logic      [TCDMPorts-1:0]         data_pwrite_i,
  input  logic      [TCDMPorts-1:0]         data_perror_i,
  input  meta_id_t  [TCDMPorts-1:0]         data_pid_i,
  input  logic      [TCDMPorts-1:0]         data_pvalid_i,
  output logic      [TCDMPorts-1:0]         data_pready_o,

  input  logic                              wake_up_sync_i,
  // Core event strobes
  output snitch_pkg::core_events_t          core_events_o
);

  // --------
  // Typedefs
  // --------
  import spatz_pkg::*;

  // TODO Diyou: dreq_t and drsp_t are not consistent in spatz, mempool and here

  typedef struct packed {
    logic accept;
    logic writeback;
    logic loadstore;
    logic exception;
    logic isfloat;
  } acc_issue_rsp_t;

  typedef logic [31:0] addr_t;
  typedef logic [31:0] data_t;
  typedef logic [3:0]  strb_t;

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation = spatz_pkg::MemPoolFPUImpl;


  // ----------------
  // Wire Definitions
  // ----------------

  // Data port signals
  snitch_pkg::dreq_t  data_req_d, data_req_q, snitch_req, fp_lsu_req;
  snitch_pkg::dresp_t data_resp_d, data_resp_q, snitch_resp, fp_lsu_rsp;

  logic data_req_d_valid, data_req_d_ready, data_resp_d_valid, data_resp_d_ready;
  logic data_req_q_valid, data_req_q_ready, data_resp_q_valid, data_resp_q_ready;
  logic snitch_req_valid, snitch_req_ready, snitch_resp_valid, snitch_resp_ready;

  // Accelerator signals
  // TODO Diyou: do we need to change name to acc_issue_req_t to keep the same convention as in spatz?
  snitch_pkg::acc_req_t  acc_req_d,  acc_req_q;
  snitch_pkg::acc_resp_t acc_resp_d, acc_resp_q;

  logic acc_req_d_valid, acc_req_d_ready, acc_resp_d_valid, acc_resp_d_ready;
  logic acc_req_q_valid, acc_req_q_ready, acc_resp_q_valid, acc_resp_q_ready;


  // Spatz Memory consistency signals
  logic [1:0] spatz_mem_finished;
  logic [1:0] spatz_mem_str_finished;
  logic [1:0] spatz_mem_req_sent;      // request-side: all of a mem op's requests issued
  logic       acc_mem_vec_accepted;    // vector mem op accepted -> vector-only fence increment

  // Spatz floating point signals
  fpnew_pkg::roundmode_e fpu_rnd_mode;
  fpnew_pkg::fmt_mode_t fpu_fmt_mode;
  fpnew_pkg::status_t fpu_status;
  acc_issue_rsp_t acc_req_rsp;

  // Spatz floating point mem signals
  // reqrsp_req_t fp_lsu_mem_req;
  // reqrsp_rsp_t fp_lsu_mem_rsp;

  // Spatz TCDM mem ports
  spatz_mem_req_t [NumMemPortsPerSpatz-1:0] spatz_mem_req;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_req_valid;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_req_ready;
  spatz_mem_rsp_t [NumMemPortsPerSpatz-1:0] spatz_mem_rsp;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_rsp_valid;

  spatz_mem_req_t  fp_lsu_mem_req;
  logic            fp_lsu_mem_req_ready;
  logic            fp_lsu_mem_req_valid;
  spatz_mem_rsp_t  fp_lsu_mem_rsp;
  logic            fp_lsu_mem_rsp_ready;
  logic            fp_lsu_mem_rsp_valid;

  // Snitch Integer Core
  snitch #(
    .BootAddr   ( BootAddr  ),
    .MTVEC      ( MTVEC     ),
    .RVE        ( RVE       ),
    .RVM        ( RVM       ),
    .RVV        ( RVV       ),
    .XFVEC      ( XFVEC     ),
    .XFDOTP     ( XFDOTP    ),
    .XFAUX      ( XFAUX     ),
    .RVF        ( RVF       ),
    .RVD        ( RVD       ),
    .XF16       ( XF16      ),
    .XF16ALT    ( XF16ALT   ),
    .XF8        ( XF8       ),
    .XF8ALT     ( XF8ALT    ),
    .XDivSqrt   ( XDivSqrt  ),
    .acc_issue_rsp_t  ( acc_issue_rsp_t )
  ) i_snitch (
    .clk_i                  ( clk_i                  ),
    .rst_i                  ( rst_i                  ),
    .hart_id_i              ( hart_id_i              ),
    /// Instruction
    .inst_addr_o            ( inst_addr_o            ),
    .inst_data_i            ( inst_data_i            ),
    .inst_valid_o           ( inst_valid_o           ),
    .inst_ready_i           ( inst_ready_i           ),
    /// Spatz
    .acc_qaddr_o            ( acc_req_d.addr         ),
    .acc_qid_o              ( acc_req_d.id           ),
    .acc_qdata_op_o         ( acc_req_d.data_op      ),
    .acc_qdata_arga_o       ( acc_req_d.data_arga    ),
    .acc_qdata_argb_o       ( acc_req_d.data_argb    ),
    .acc_qdata_argc_o       ( acc_req_d.data_argc    ),
    .acc_qvalid_o           ( acc_req_d_valid        ),
    .acc_qready_i           ( acc_req_d_ready        ),
    .acc_pdata_i            ( acc_resp_q.data        ),
    .acc_pid_i              ( acc_resp_q.id          ),
    .acc_pwrite_i           ( acc_resp_q.write       ),
    .acc_perror_i           ( acc_resp_q.error       ),
    .acc_pvalid_i           ( acc_resp_q_valid       ),
    .acc_pready_o           ( acc_resp_q_ready       ),
    .acc_qdata_rsp_i        ( acc_req_rsp            ),
    .acc_mem_finished_i     ( spatz_mem_finished     ),
    .acc_mem_str_finished_i ( spatz_mem_str_finished ),
    .acc_mem_req_sent_i     ( spatz_mem_req_sent     ),
    .acc_mem_vec_accepted_i ( acc_mem_vec_accepted   ),
    /// TCDM
    .data_qaddr_o           ( snitch_req.addr        ),
    .data_qwrite_o          ( snitch_req.write       ),
    .data_qamo_o            ( snitch_req.amo         ),
    .data_qdata_o           ( snitch_req.data        ),
    .data_qstrb_o           ( snitch_req.strb        ),
    .data_qid_o             ( snitch_req.id          ),
    .data_qvalid_o          ( snitch_req_valid       ),
    .data_qready_i          ( snitch_req_ready       ),
    .data_pdata_i           ( snitch_resp.data       ),
    .data_perror_i          ( snitch_resp.error      ),
    .data_pid_i             ( snitch_resp.id         ),
    .data_pvalid_i          ( snitch_resp_valid      ),
    .data_pready_o          ( snitch_resp_ready      ),
    .wake_up_sync_i         ( wake_up_sync_i         ),
    .fpu_fmt_mode_o         ( fpu_fmt_mode           ),
    .fpu_rnd_mode_o         ( fpu_rnd_mode           ),
    .fpu_status_i           ( fpu_status             ),
    .core_events_o          ( core_events_o          )
  );

  assign acc_req_q = acc_req_d;
  assign acc_req_q_valid = acc_req_d_valid;
  assign acc_req_d_ready = acc_req_q_ready;

  // Burst loads consume one ROB ID per returned word. Keep at least one full
  // MaxBurstWords window to avoid ID aliasing during a single burst.
  localparam int unsigned SpatzNumOutstandingLoads =
      (snitch_pkg::NumIntOutstandingLoads < snitch_pkg::MaxBurstWords) ?
      snitch_pkg::MaxBurstWords : snitch_pkg::NumIntOutstandingLoads;

  // Cut off-loading response path
  spill_register #(
    .T      ( snitch_pkg::acc_resp_t ),
    .Bypass ( !RegisterOffloadResp   )
  ) i_spill_register_acc_resp (
    .clk_i                       ,
    .rst_ni  ( ~rst_i           ),
    .valid_i ( acc_resp_d_valid ),
    .ready_o ( acc_resp_d_ready ),
    .data_i  ( acc_resp_d       ),
    .valid_o ( acc_resp_q_valid ),
    .ready_i ( acc_resp_q_ready ),
    .data_o  ( acc_resp_q       )
  );

  spatz #(
    .NrMemPorts         ( NumMemPortsPerSpatz     ),
    .NumRespPorts       ( NumRespPorts            ),
    .NumOutstandingLoads( SpatzNumOutstandingLoads ),
    .FPUImplementation  ( FPUImplementation       ),
    .RegisterRsp        ( 1'b1                    ),
    .spatz_mem_req_t    ( spatz_mem_req_t         ),
    .spatz_mem_rsp_t    ( spatz_mem_rsp_t         ),
    .dreq_t             ( spatz_mem_req_t         ),
    .drsp_t             ( spatz_mem_rsp_t         ),
    .spatz_issue_req_t  ( snitch_pkg::acc_req_t   ),
    .spatz_issue_rsp_t  ( acc_issue_rsp_t         ),
    .spatz_rsp_t        ( snitch_pkg::acc_resp_t  )
  ) i_spatz (
    .clk_i                   ( clk_i                 ),
    .rst_ni                  ( ~rst_i                ),
    .testmode_i              ( 1'b0                  ),
    .hart_id_i               ( hart_id_i             ),
    .issue_valid_i           ( acc_req_q_valid       ),
    .issue_ready_o           ( acc_req_q_ready       ),
    .issue_req_i             ( acc_req_q             ),
    .issue_rsp_o             ( acc_req_rsp           ),
    .rsp_valid_o             ( acc_resp_d_valid      ),
    .rsp_ready_i             ( acc_resp_d_ready      ),
    .rsp_o                   ( acc_resp_d            ),
    .spatz_mem_req_o         ( spatz_mem_req         ),
    .spatz_mem_req_valid_o   ( spatz_mem_req_valid   ),
    .spatz_mem_req_ready_i   ( spatz_mem_req_ready   ),
    .spatz_mem_rsp_i         ( spatz_mem_rsp         ),
    .spatz_mem_rsp_valid_i   ( spatz_mem_rsp_valid   ),// ***notice no ready signal here***
    .spatz_mem_finished_o    ( spatz_mem_finished    ),
    .spatz_mem_str_finished_o( spatz_mem_str_finished),
    .spatz_mem_req_sent_o    ( spatz_mem_req_sent    ),
    .acc_mem_vec_accepted_o  ( acc_mem_vec_accepted  ),
    .fp_lsu_mem_req_o        ( fp_lsu_mem_req        ),
    .fp_lsu_mem_req_valid_o  ( fp_lsu_mem_req_valid  ),
    .fp_lsu_mem_req_ready_i  ( fp_lsu_mem_req_ready  ),
    .fp_lsu_mem_rsp_i        ( fp_lsu_mem_rsp        ),
    .fp_lsu_mem_rsp_valid_i  ( fp_lsu_mem_rsp_valid  ),
    .fp_lsu_mem_rsp_ready_o  ( fp_lsu_mem_rsp_ready  ),
    .fpu_rnd_mode_i          ( fpu_rnd_mode          ),
    .fpu_fmt_mode_i          ( fpu_fmt_mode          ),
    .fpu_status_o            ( fpu_status            )
  );

  // TODO: Perhaps put it into a module
  // Assign TCDM data interface
  for (genvar i = 0; i < NumMemPortsPerSpatz; i++) begin : gen_tcdm_assignment
    assign data_qaddr_o[i+1]       = spatz_mem_req[i].addr;
    assign data_qwrite_o[i+1]      = spatz_mem_req[i].write;
    assign data_qamo_o[i+1]        = '0;
    assign data_qdata_o[i+1]       = spatz_mem_req[i].data;
    assign data_qstrb_o[i+1]       = spatz_mem_req[i].strb;
    assign data_qid_o[i+1]         = spatz_mem_req[i].id;
    assign data_qburst_len_o[i+1]  = spatz_mem_req[i].burst_len;
    assign data_qvalid_o[i+1]      = spatz_mem_req_valid[i];
    assign spatz_mem_req_ready[i]  = data_qready_i[i+1];
    assign spatz_mem_rsp[i].data   = data_pdata_i[i+1];
    assign spatz_mem_rsp[i].write  = data_pwrite_i[i+1];
    assign spatz_mem_rsp[i].id     = data_pid_i[i+1];
    assign spatz_mem_rsp[i].err    = data_perror_i[i+1];
    assign spatz_mem_rsp_valid[i]  = data_pvalid_i[i+1];
    // *** no ready signal for spatz here, tie to 1 ***
    assign data_pready_o[i+1]      = '1;
  end

  // ROB64 width-pairing tripwires: the VLSU ROB id, the wire meta_id and the response
  // struct id must agree, or ids silently truncate at :285/:291. These are elaboration-time
  // checks (constant), so they fire at compile, not at runtime.
  if ($bits(spatz_mem_req[0].id) < snitch_pkg::MetaIdWidth)
    $error("[spatz_mempool_cc] spatz_mem_req_t.id (%0d) narrower than meta_id_t (%0d) -- request truncation.",
           $bits(spatz_mem_req[0].id), snitch_pkg::MetaIdWidth);
  if ($bits(spatz_mem_rsp[0].id) != snitch_pkg::MetaIdWidth)
    $error("[spatz_mempool_cc] spatz_mem_rsp_t.id (%0d) != MetaIdWidth (%0d) -- response aliasing.",
           $bits(spatz_mem_rsp[0].id), snitch_pkg::MetaIdWidth);

  assign fp_lsu_req = '{
    addr   : fp_lsu_mem_req.addr,
    id     : fp_lsu_mem_req.id,
    write  : fp_lsu_mem_req.write,
    data   : fp_lsu_mem_req.data,
    strb   : fp_lsu_mem_req.strb,
    burst_len: snitch_pkg::BurstLenWidth'(1),
    default: '0
  };

  assign fp_lsu_mem_rsp = '{
    id  : fp_lsu_rsp.id,
    data: fp_lsu_rsp.data,
    err : fp_lsu_rsp.error,
    write: fp_lsu_rsp.write
  };

  if (RVF || RVD) begin: gen_id_remapper
    // Merge Snitch and FP Subsequencer memory interfaces
    tcdm_id_remapper #(
      .NumIn(2)
    ) i_id_remapper (
      .clk_i       (clk_i                                     ),
      .rst_ni      (~rst_i                                    ),
      .req_i       ({fp_lsu_req, snitch_req}                  ),
      .req_valid_i ({fp_lsu_mem_req_valid, snitch_req_valid}  ),
      .req_ready_o ({fp_lsu_mem_req_ready, snitch_req_ready}  ),
      .resp_o      ({fp_lsu_rsp, snitch_resp}                 ),
      .resp_valid_o({fp_lsu_mem_rsp_valid, snitch_resp_valid} ),
      .resp_ready_i({fp_lsu_mem_rsp_ready, snitch_resp_ready} ),
      .req_o       (data_req_d                                ),
      .req_valid_o (data_req_d_valid                          ),
      .req_ready_i (data_req_d_ready                          ),
      .resp_i      (data_resp_q                               ),
      .resp_valid_i(data_resp_q_valid                         ),
      .resp_ready_o(data_resp_q_ready                         )
    );
  end: gen_id_remapper else begin: gen_id_remapper_bypass
    // Bypass the remapper
    assign data_req_d       = snitch_req;
    assign data_req_d_valid = snitch_req_valid;
    assign snitch_req_ready = data_req_d_ready;

    assign snitch_resp       = data_resp_q;
    assign snitch_resp_valid = data_resp_q_valid;
    assign data_resp_q_ready = snitch_resp_ready;

    assign fp_lsu_rsp           = '0;
    assign fp_lsu_mem_rsp_valid = 1'b0;
    assign fp_lsu_mem_req_ready = 1'b0;
  end: gen_id_remapper_bypass


  // Cut TCDM data request path
  spill_register #(
    .T      ( snitch_pkg::dreq_t ),
    .Bypass ( !RegisterTCDMReq   )
  ) i_spill_register_tcdm_req (
    .clk_i                       ,
    .rst_ni  ( ~rst_i           ),
    .valid_i ( data_req_d_valid ),
    .ready_o ( data_req_d_ready ),
    .data_i  ( data_req_d       ),
    .valid_o ( data_req_q_valid ),
    .ready_i ( data_req_q_ready ),
    .data_o  ( data_req_q       )
  );

  // Cut TCDM data response path
  spill_register #(
    .T      ( snitch_pkg::dresp_t ),
    .Bypass ( !RegisterTCDMResp   )
  ) i_spill_register_tcdm_resp (
    .clk_i                       ,
    .rst_ni  ( ~rst_i           ),
    .valid_i ( data_resp_d_valid ),
    .ready_o ( data_resp_d_ready ),
    .data_i  ( data_resp_d       ),
    .valid_o ( data_resp_q_valid ),
    .ready_i ( data_resp_q_ready ),
    .data_o  ( data_resp_q       )
  );

  // Assign TCDM data interface
  assign data_qaddr_o[0]      = data_req_q.addr;
  assign data_qwrite_o[0]     = data_req_q.write;
  assign data_qamo_o[0]       = data_req_q.amo;
  assign data_qdata_o[0]      = data_req_q.data;
  assign data_qstrb_o[0]      = data_req_q.strb;
  assign data_qid_o[0]        = data_req_q.id;
  assign data_qburst_len_o[0] = snitch_pkg::BurstLenWidth'(1);
  assign data_qvalid_o[0]     = data_req_q_valid;
  assign data_req_q_ready     = data_qready_i[0];
  assign data_resp_d.data     = data_pdata_i[0];
  assign data_resp_d.id       = data_pid_i[0];
  assign data_resp_d.write    = '0; // Don't care here
  assign data_resp_d.error    = data_perror_i[0];
  assign data_resp_d_valid    = data_pvalid_i[0];
  assign data_pready_o[0]     = data_resp_d_ready;

  // --------------------------
  // Tracer
  // --------------------------
  // pragma translate_off
  int f;
  string fn;
  logic [63:0] cycle;
  int unsigned stall, stall_ins, stall_raw, stall_lsu, stall_acc;

  // verilog_lint: waive-start always-ff-non-blocking
  always_ff @(posedge rst_i) begin
    if(rst_i) begin
      // Format in hex because vcs and vsim treat decimal differently
      // Format with 8 digits because Verilator does not support anything else
      $sformat(fn, "trace_hart_0x%08x.dasm", hart_id_i);
      f = $fopen(fn, "w");
      $display("[Tracer] Logging Hart %d to %s", hart_id_i, fn);
    end
  end
  // verilog_lint: waive-stop always-ff-non-blocking

  typedef enum logic [1:0] {SrcSnitch =  0, SrcFpu = 1, SrcFpuSeq = 2} trace_src_e;
  localparam int SnitchTrace = `ifdef SNITCH_TRACE `SNITCH_TRACE `else 0 `endif;

  // verilog_lint: waive-start always-ff-non-blocking
  always_ff @(posedge clk_i or posedge rst_i) begin
      automatic string trace_entry;
      automatic string extras_str;

      if (!rst_i) begin
        cycle <= cycle + 1;
        // Trace snitch iff:
        // Tracing enabled by CSR register
        // we are not stalled <==> we have issued and processed an instruction (including offloads)
        // OR we are retiring (issuing a writeback from) a load or accelerator instruction
        if ((i_snitch.csr_trace_q || SnitchTrace) && (!i_snitch.stall || i_snitch.retire_load || i_snitch.retire_acc)) begin
          // Manual loop unrolling for Verilator
          // Data type keys for arrays are currently not supported in Verilator
          extras_str = "{";
          // State
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "source",      SrcSnitch);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "stall",       i_snitch.stall);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_tot",   stall);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_ins",   stall_ins);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_raw",   stall_raw);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_lsu",   stall_lsu);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_acc",   stall_acc);
          // Decoding
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rs1",         i_snitch.rs1);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rs2",         i_snitch.rs2);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rd",          i_snitch.rd);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "is_load",     i_snitch.is_load);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "is_store",    i_snitch.is_store);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "is_branch",   i_snitch.is_branch);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "pc_d",        i_snitch.pc_d);
          // Operands
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opa",         i_snitch.opa);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opb",         i_snitch.opb);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "opa_select",  i_snitch.opa_select);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "opb_select",  i_snitch.opb_select);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "opc_select",  i_snitch.opc_select);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "write_rd",    i_snitch.write_rd);
          extras_str = $sformatf("%s'%s': 0x%3x, ", extras_str, "csr_addr",    i_snitch.inst_data_i[31:20]);
          // Pipeline writeback
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "writeback",   i_snitch.alu_writeback);
          // Load/Store
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "gpr_rdata_1", i_snitch.gpr_rdata[1]);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "ls_size",     i_snitch.ls_size);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "ld_result_32",i_snitch.ld_result[31:0]);
          extras_str = $sformatf("%s'%s': 0x%2x, ", extras_str, "lsu_rd",      i_snitch.lsu_rd);
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "retire_load", i_snitch.retire_load);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "alu_result",  i_snitch.alu_result);
          // Atomics
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "ls_amo",      i_snitch.ls_amo);
          // Accumulator
          extras_str = $sformatf("%s'%s': 0x%1x, ", extras_str, "retire_acc",  i_snitch.retire_acc);
          extras_str = $sformatf("%s'%s': 0x%2x, ", extras_str, "acc_pid",     i_snitch.acc_pid_i);
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "acc_pdata_32",i_snitch.acc_pdata_i[31:0]);
          extras_str = $sformatf("%s}", extras_str);

          $timeformat(-9, 0, "", 10);
          $sformat(trace_entry, "%t %8d 0x%h DASM(%h) #; %s\n",
              $time, cycle, i_snitch.pc_q, i_snitch.inst_data_i, extras_str);
          $fwrite(f, trace_entry);
        end

        // Reset all stalls when we execute an instruction
        if (!i_snitch.stall) begin
            stall <= 0;
            stall_ins <= 0;
            stall_raw <= 0;
            stall_lsu <= 0;
            stall_acc <= 0;
        end else begin
          // We are currently stalled, let's count the stall causes
          if (i_snitch.stall) begin
            stall <= stall + 1;
          end
          if ((!i_snitch.inst_ready_i) && (i_snitch.inst_valid_o)) begin
            stall_ins <= stall_ins + 1;
          end
          if ((!i_snitch.operands_ready) || (!i_snitch.dst_ready)) begin
            stall_raw <= stall_raw + 1;
          end
          if (i_snitch.lsu_stall) begin
            stall_lsu <= stall_lsu + 1;
          end
          if (i_snitch.acc_stall) begin
            stall_acc <= stall_acc + 1;
          end
        end
      end else begin
        cycle <= '0;
        stall <= 0;
        stall_ins <= 0;
        stall_raw <= 0;
        stall_lsu <= 0;
        stall_acc <= 0;
      end
    end

  final begin
    $fclose(f);
  end
  // verilog_lint: waive-stop always-ff-non-blocking
  // pragma translate_on

  ///////////////////////////////
  //  Spatz vector-core tracer  //
  ///////////////////////////////
  // pragma translate_off
`ifndef VERILATOR
  // Per-Spatz-core trace, mirroring the Snitch trace_hart_*.dasm workflow (post-processed by
  // `make spatz-trace` -> hardware/scripts/gen_spatz_trace.py). Two streams per core:
  //   trace_spatz_insn_hart_0x*.log : one ISSUE + one RETIRE line per vector instruction (what
  //     ran, when, and a lifetime summary: active/stall cycles + IPU/FPU/mem-beat cycle counts).
  //   trace_spatz_cyc_hart_0x*.log  : one line per ACTIVE cycle -- per-lane IPU busy/result mask,
  //     per-FPU result mask, VLSU FSM state + memory beats, and the issue-stall reason.
  // Gated exactly like the Snitch tracer: the per-core csr_trace_q region OR a SPATZ_TRACE define.
  // Blocking assignments are intentional (a simulation-only scoreboard with program semantics).
  localparam int          SpatzTraceOn = `ifdef SPATZ_TRACE `SPATZ_TRACE `else 0 `endif;
  localparam int unsigned SpNId        = spatz_pkg::NrParallelInstructions;

  int          sp_insn_f, sp_cyc_f, sp_fpl_f;
  string       sp_insn_fn, sp_cyc_fn, sp_fpl_fn;
  logic [63:0] sp_cycle;
  logic [SpNId-1:0] sp_run_prev;
  // Per-in-flight-id (spatz_id_t) instruction snapshot + lifetime accumulators.
  string       sp_op    [SpNId];
  string       sp_unit  [SpNId];
  logic        sp_isvfu [SpNId];
  logic        sp_islsu [SpNId];
  int unsigned sp_act   [SpNId];
  int unsigned sp_stall [SpNId];
  int unsigned sp_ipuc  [SpNId];
  int unsigned sp_fpuc  [SpNId];
  int unsigned sp_memc  [SpNId];

  // ---- PC / raw-instruction correlation (so a trace line can be matched to the .dasm) ----------
  // The Spatz side has no PC of its own: an offloaded instruction arrives over the acc interface
  // carrying only data_op. We snapshot (pc, insn) at the OFFLOAD HANDSHAKE, where i_snitch.pc_q is
  // still the offending instruction's PC, and carry it forward:
  //   * non-local (vector) ops  -> a small in-order FIFO, popped when the controller assigns an id,
  //                                then held per-id so ISSUE and RETIRE can both print it.
  //   * local ops (flw/fsw/fmv) -> held per FP DEST REGISTER, so an out-of-order FP-LSU writeback
  //                                can still name the instruction that produced it.
  // CAVEAT: the vector FIFO assumes every accepted non-local op eventually reaches spatz_req_valid_o
  // exactly once and in order. An ILLEGAL vector instruction is popped by the controller without
  // asserting spatz_req_valid_o and would desync the PC column (the cycle/op fields stay correct).
  // Each line also prints the raw insn, so a mismatch is detectable.
  localparam int unsigned SpPcFifoDepth = 8;
  logic [31:0] sp_pcf_pc   [SpPcFifoDepth];
  logic [31:0] sp_pcf_insn [SpPcFifoDepth];
  int unsigned sp_pcf_wr, sp_pcf_rd;
  logic [31:0] sp_pc      [SpNId];   // PC of the vector insn occupying each in-flight id
  logic [31:0] sp_insn    [SpNId];
  logic [31:0] sp_fpr_pc  [32];      // PC of the insn that will write each FP register
  logic [31:0] sp_fpr_insn[32];

  // verilog_lint: waive-start always-ff-non-blocking
  always_ff @(posedge rst_i) begin
    if (rst_i) begin
      $sformat(sp_insn_fn, "trace_spatz_insn_hart_0x%08x.log", hart_id_i);
      $sformat(sp_cyc_fn,  "trace_spatz_cyc_hart_0x%08x.log",  hart_id_i);
      $sformat(sp_fpl_fn,  "trace_spatz_fplsu_hart_0x%08x.log", hart_id_i);
      sp_insn_f = $fopen(sp_insn_fn, "w");
      sp_cyc_f  = $fopen(sp_cyc_fn,  "w");
      sp_fpl_f  = $fopen(sp_fpl_fn,  "w");
      $fwrite(sp_insn_f,
        "# ISSUE cyc id unit op pc insn vd vs1 vs2 vl / RETIRE cyc id unit op pc insn active stall ipu_cyc fpu_cyc mem_beats\n");
      $fwrite(sp_cyc_f,
        "# cyc state reason(ALL active) run ipu_busy ipu_vld fpu_vld vlsu mem_req mem_rsp vfu_ins vfu_opr vfu_stl sb_deps  |  vfu_ins=1&vfu_opr=0 => VFU WAITING FOR DATA, not computing\n");
      $fwrite(sp_fpl_f,
        "# FP sequencer. ISSUE cyc pc insn kind(load|store|move|vector) fd / RETIRE cyc port(fpu|lsu) fd pc insn / REQ cyc addr tag(=fd) L|S / RSP cyc tag / STALL cyc reason(ALL active) pc\n");
      $display("[SpatzTracer] Logging Hart %0d to %s + %s + %s",
               hart_id_i, sp_insn_fn, sp_cyc_fn, sp_fpl_fn);
    end
  end

  always_ff @(posedge clk_i or posedge rst_i) begin
    automatic logic                        en;
    automatic logic [SpNId-1:0]            run_now;
    automatic logic [SpNId-1:0]            retired;
    automatic logic [spatz_pkg::N_IPU-1:0] ipu_busy_m;
    automatic logic [spatz_pkg::N_IPU-1:0] ipu_vld_m;
    automatic logic [spatz_pkg::N_FPU-1:0] fpu_vld_m;
    automatic logic                        mem_req_beat;
    automatic logic                        mem_rsp_beat;
    automatic logic                        any_ipu;
    automatic logic                        any_fpu;
    automatic logic                        cstall;
    automatic string                       reason;
    automatic string                       fpl_reason;
    automatic int unsigned                 iid;
    if (rst_i) begin
      sp_cycle    = '0;
      sp_run_prev = '0;
      for (int k = 0; k < SpNId; k++) begin
        sp_act[k] = 0; sp_stall[k] = 0; sp_ipuc[k] = 0; sp_fpuc[k] = 0; sp_memc[k] = 0;
        sp_isvfu[k] = 1'b0; sp_islsu[k] = 1'b0; sp_op[k] = "-"; sp_unit[k] = "-";
        sp_pc[k] = '0; sp_insn[k] = '0;
      end
      sp_pcf_wr = 0; sp_pcf_rd = 0;
      for (int k = 0; k < 32; k++) begin sp_fpr_pc[k] = '0; sp_fpr_insn[k] = '0; end
    end else begin
      en      = i_snitch.csr_trace_q || (SpatzTraceOn != 0);
      run_now = i_spatz.i_controller.running_insn_q;
      cstall  = i_spatz.i_controller.stall;
      // Functional-unit activity this cycle (all module-level signals, XMR-accessible).
      ipu_busy_m = i_spatz.i_vfu.int_ipu_busy;
      for (int l = 0; l < spatz_pkg::N_IPU; l++)
        ipu_vld_m[l] = |i_spatz.i_vfu.int_ipu_result_valid[l*spatz_pkg::ELENB +: spatz_pkg::ELENB];
      for (int l = 0; l < spatz_pkg::N_FPU; l++)
        fpu_vld_m[l] = |i_spatz.i_vfu.fpu_result_valid[l*spatz_pkg::ELENB +: spatz_pkg::ELENB];
      mem_req_beat = |(i_spatz.i_vlsu.spatz_mem_req_valid_o & i_spatz.i_vlsu.spatz_mem_req_ready_i);
      mem_rsp_beat = |i_spatz.i_vlsu.spatz_mem_rsp_valid_i;
      any_ipu = |ipu_busy_m;
      any_fpu = |fpu_vld_m;
      // Issue-block reasons. NOTE these are TWO INDEPENDENT mechanisms and must not be conflated:
      //   stall           = (vfu|vlsu|vsldu)_stall & req_buffer_valid   (spatz_controller.sv:449)
      //   running_insn_full is a SEPARATE gate on req_buffer_pop        (:469) -- NOT part of stall.
      // An earlier priority encoder put idfull first and therefore HID which unit actually caused
      // the stall whenever the id window was also full (which is most of the time on this kernel).
      // Report every active reason instead, so `state=stall` always names the real unit.
      reason = "";
      if (i_spatz.i_controller.vfu_stall)   reason = {reason, "vfu,"};
      if (i_spatz.i_controller.vlsu_stall)  reason = {reason, "vlsu,"};
      if (i_spatz.i_controller.vsldu_stall) reason = {reason, "vsldu,"};
      if (i_spatz.i_controller.running_insn_full) reason = {reason, "idfull,"};
      if (reason == "") reason = "-,";

      // Accumulate per-id lifetime counters (always, so an instruction that spans the trace
      // enable boundary still gets a complete summary; only the emit is gated by `en`).
      // NOTE: ipu_cyc/fpu_cyc/mem_beats are core-wide FU-busy cycles observed WHILE this id is in
      // flight, not exclusive per-instruction work -- when two same-unit instructions overlap in
      // running_insn_q (e.g. one writing back while the next executes, common at LMUL>=2) both are
      // credited, so their sum can exceed the real FU-busy cycle count. The per-cycle stream is the
      // authoritative per-lane/per-FPU source; treat these as a per-instruction window aggregate.
      for (int k = 0; k < SpNId; k++) begin
        if (run_now[k]) begin
          sp_act[k] = sp_act[k] + 1;
          if (cstall)                      sp_stall[k] = sp_stall[k] + 1;
          if (sp_isvfu[k] && any_ipu)      sp_ipuc[k]  = sp_ipuc[k]  + 1;
          if (sp_isvfu[k] && any_fpu)      sp_fpuc[k]  = sp_fpuc[k]  + 1;
          if (sp_islsu[k] && mem_req_beat) sp_memc[k]  = sp_memc[k]  + 1;
        end
      end

      // RETIRE: running_insn_q[id] fell 1->0 (edge vs my delayed copy). Emitted BEFORE the ISSUE
      // block so that a same-cycle id REUSE (an id retiring and being re-issued the same cycle,
      // which happens routinely when running_insn_full releases one id) reads the retiring
      // instruction's snapshot, not the new one's -- otherwise the ISSUE reset below would zero it.
      retired = sp_run_prev & ~run_now;
      for (int k = 0; k < SpNId; k++) begin
        if (retired[k] && en)
          $fwrite(sp_insn_f,
                  "RETIRE %0d id=%0d %s %s pc=0x%08x insn=0x%08x active=%0d stall=%0d ipu_cyc=%0d fpu_cyc=%0d mem_beats=%0d\n",
                  sp_cycle, k, sp_unit[k], sp_op[k], sp_pc[k], sp_insn[k], sp_act[k], sp_stall[k],
                  sp_ipuc[k], sp_fpuc[k], sp_memc[k]);
      end
      sp_run_prev = run_now;

      // ISSUE: a vector instruction is dispatched to a unit this cycle.
      if (i_spatz.i_controller.spatz_req_valid_o) begin
        iid           = i_spatz.i_controller.spatz_req_o.id;
        sp_op[iid]    = i_spatz.i_controller.spatz_req_o.op.name();
        sp_unit[iid]  = i_spatz.i_controller.spatz_req_o.ex_unit.name();
        sp_isvfu[iid] = (sp_unit[iid] == "VFU");
        sp_islsu[iid] = (sp_unit[iid] == "LSU");
        sp_act[iid]   = 0; sp_stall[iid] = 0; sp_ipuc[iid] = 0; sp_fpuc[iid] = 0; sp_memc[iid] = 0;
        // Take the PC snapshotted at this instruction's offload handshake (in-order FIFO).
        if (sp_pcf_rd != sp_pcf_wr) begin
          sp_pc[iid]   = sp_pcf_pc  [sp_pcf_rd % SpPcFifoDepth];
          sp_insn[iid] = sp_pcf_insn[sp_pcf_rd % SpPcFifoDepth];
          sp_pcf_rd    = sp_pcf_rd + 1;
        end else begin
          sp_pc[iid] = '0; sp_insn[iid] = '0;   // FIFO underflow (should not happen)
        end
        if (en)
          $fwrite(sp_insn_f, "ISSUE %0d id=%0d %s %s pc=0x%08x insn=0x%08x vd=%0d vs1=%0d vs2=%0d vl=%0d\n",
                  sp_cycle, iid, sp_unit[iid], sp_op[iid], sp_pc[iid], sp_insn[iid],
                  i_spatz.i_controller.spatz_req_o.vd, i_spatz.i_controller.spatz_req_o.vs1,
                  i_spatz.i_controller.spatz_req_o.vs2, i_spatz.i_controller.spatz_req_o.vl);
      end

      // Per-cycle activity stream (only cycles where the core has work or is stalled).
      // vfu_ins/vfu_opr/vfu_stl make the "is the VFU COMPUTING or WAITING?" question first-class:
      //   vfu_stall (in `reason`) is really "the VFU's 2-entry operation queue is full", which
      //   happens both while the VFU is genuinely executing AND while it holds an instruction whose
      //   operands are not yet in the VRF -- e.g. a vector LOAD is still writing the source
      //   register (spatz_vfu.sv:130-133 operands_ready <- vrf_rvalid_i, gated by the controller
      //   scoreboard). Without these fields the two are indistinguishable and memory-wait gets
      //   misattributed to compute.
      //     vfu_ins = the VFU holds an instruction   (spatz_vfu.sv:73  spatz_req_valid)
      //     vfu_opr = its operands are ready         (spatz_vfu.sv:133 operands_ready)
      //     vfu_stl = VFU-internal FSM stall         (spatz_vfu.sv:123 stall)
      //   => vfu_ins=1 & vfu_opr=0  ==  VFU occupied but WAITING FOR DATA (not computing).
      if (en && (|run_now || any_ipu || any_fpu || mem_req_beat || mem_rsp_beat ||
                 i_spatz.i_controller.spatz_req_valid_o || cstall))
        $fwrite(sp_cyc_f,
                "%0d %s reason=%s run=%b ipu_busy=%b ipu_vld=%b fpu_vld=%b vlsu=%s mem_req=%0d mem_rsp=%0d vfu_ins=%0d vfu_opr=%0d vfu_stl=%0d sb_deps=%b\n",
                sp_cycle, (cstall ? "stall" : "run"), reason, run_now,
                ipu_busy_m, ipu_vld_m, fpu_vld_m,
                i_spatz.i_vlsu.state_q.name(), mem_req_beat, mem_rsp_beat,
                i_spatz.i_vfu.spatz_req_valid, i_spatz.i_vfu.operands_ready,
                i_spatz.i_vfu.stall, i_spatz.i_controller.sb_port_has_deps_dbg);

      // Scalar FP-LSU (flw/fsw via the FPU sequencer i_fp_lsu -- a datapath SEPARATE from the
      // vector controller traced above; the vector streams never see scalar flw/fsw). REQ/RSP give
      // per-load request/return timing (match REQ->RSP by tag = fp dest reg fd) so shared A-word
      // loads' return ALIGNMENT across cores is directly observable; STALL gives why the sequencer
      // (and thus a scalar-FP-operand-dependent vector offload) is blocked -- 'operands' = the FPR
      // scoreboard waiting on a pending flw.
      // ---- FP-sequencer instruction stream (offload point => the PC is still available) ----------
      // ISSUE is logged at the acc handshake, i.e. the cycle Spatz ACCEPTS the instruction from
      // Snitch. kind=vector means it was forwarded to the vector controller (and its PC is pushed to
      // the FIFO so the vector stream can name it); load/store/move are executed here.
      if (acc_req_d_valid && acc_req_d_ready) begin
        automatic logic        loc = i_spatz.gen_fpu_sequencer.i_fpu_sequencer.is_local;
        automatic logic [4:0]  d   = i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fd;
        automatic string       knd =
            i_spatz.gen_fpu_sequencer.i_fpu_sequencer.is_load  ? "load"  :
            i_spatz.gen_fpu_sequencer.i_fpu_sequencer.is_store ? "store" :
            i_spatz.gen_fpu_sequencer.i_fpu_sequencer.is_move  ? "move"  : "vector";
        if (loc) begin
          // Local op: remember the PC per DEST REGISTER so an OOO writeback can be named.
          sp_fpr_pc[d]   = i_snitch.pc_q;
          sp_fpr_insn[d] = acc_req_d.data_op;
        end else begin
          // Vector op: hand the PC to the vector stream, in order.
          sp_pcf_pc  [sp_pcf_wr % SpPcFifoDepth] = i_snitch.pc_q;
          sp_pcf_insn[sp_pcf_wr % SpPcFifoDepth] = acc_req_d.data_op;
          sp_pcf_wr = sp_pcf_wr + 1;
        end
        if (en)
          $fwrite(sp_fpl_f, "ISSUE %0d pc=0x%08x insn=0x%08x kind=%s fd=%0d\n",
                  sp_cycle, i_snitch.pc_q, acc_req_d.data_op, knd, d);
      end

      if (en) begin
        // Retirements into the FP register file: port 0 = offloaded-FPU result, port 1 = FP-LSU.
        // Both name the instruction via the per-register PC captured at ISSUE.
        for (int unsigned w = 0; w < 2; w++) begin
          if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.retire[w]) begin
            automatic logic [4:0] rd = i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fpr_waddr[w];
            $fwrite(sp_fpl_f, "RETIRE %0d port=%s fd=%0d pc=0x%08x insn=0x%08x\n",
                    sp_cycle, (w == 0) ? "fpu" : "lsu", rd, sp_fpr_pc[rd], sp_fpr_insn[rd]);
          end
        end
        // Raw FP-LSU memory beats (match REQ->RSP by tag = the FP dest register).
        if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_qvalid &&
            i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_qready)
          $fwrite(sp_fpl_f, "REQ %0d addr=0x%0h tag=%0d %s\n", sp_cycle,
                  i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_qaddr,
                  i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_qtag,
                  i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_qwrite ? "S" : "L");
        if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_pvalid &&
            i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_pready)
          $fwrite(sp_fpl_f, "RSP %0d tag=%0d\n", sp_cycle,
                  i_spatz.gen_fpu_sequencer.i_fpu_sequencer.fp_lsu_ptag);
        // Stall: report EVERY active reason, not a priority-encoded winner (the reasons are ORed in
        // spatz_fpu_sequencer.sv:148, so more than one can hold at once and a priority encoder hides
        // the others). "offload" = !is_local && !issue_ready_i, i.e. blocked handing the instruction
        // to the VECTOR side -- the scalar sequencer itself is not out of resources.
        if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.stall) begin
          fpl_reason = "";
          if (!i_spatz.gen_fpu_sequencer.i_fpu_sequencer.operands_available)
            fpl_reason = {fpl_reason, "operands,"};
          if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.lsu_stall)  fpl_reason = {fpl_reason, "lsu,"};
          if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.vlsu_stall) fpl_reason = {fpl_reason, "vlsu,"};
          if (i_spatz.gen_fpu_sequencer.i_fpu_sequencer.move_stall) fpl_reason = {fpl_reason, "move,"};
          if (!i_spatz.gen_fpu_sequencer.i_fpu_sequencer.is_local &&
              !i_spatz.gen_fpu_sequencer.i_fpu_sequencer.issue_ready_i)
            fpl_reason = {fpl_reason, "offload,"};
          if (fpl_reason == "") fpl_reason = "-,";
          $fwrite(sp_fpl_f, "STALL %0d reason=%s pc=0x%08x\n",
                  sp_cycle, fpl_reason, i_snitch.pc_q);
        end
      end

      sp_cycle = sp_cycle + 1;
    end
  end
  // verilog_lint: waive-stop always-ff-non-blocking

  final begin
    $fclose(sp_insn_f);
    $fclose(sp_cyc_f);
    $fclose(sp_fpl_f);
  end
`endif
  // pragma translate_on

endmodule
