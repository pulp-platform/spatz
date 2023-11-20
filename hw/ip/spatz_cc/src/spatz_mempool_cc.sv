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
  parameter bit RegisterOffloadReq  = 1,
  parameter bit RegisterOffloadResp = 1,
  parameter bit RegisterTCDMReq     = 0,
  parameter bit RegisterTCDMResp    = 0,

  // TODO: change to constant param from pkg or tile level?
  parameter int unsigned        TCDMPorts              = 1,
  parameter int unsigned        NumMemPortsPerSpatz    = 1,
  
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
  output logic      [TCDMPorts-1:0]         data_qvalid_o,
  input  logic      [TCDMPorts-1:0]         data_qready_i,
  input  logic      [TCDMPorts-1:0][31:0]   data_pdata_i,
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
  import spatz_pkg::*
  
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

  `REQRSP_TYPEDEF_ALL(reqrsp, addr_t, data_t, strb_t)

  `TCDM_TYPEDEF_ALL(tcdm, tcdm_addr_t, data_t, strb_t, tcdm_user_t)

  // TODO: NRVREG
  typedef struct packed {
    logic [$clog2(NRVREG):0] id;
    logic [31:0] addr;
    logic [1:0] mode;
    logic [1:0] size;
    logic write;
    logic [DataWidth/8-1:0] strb;
    logic [DataWidth-1:0] data;
    logic last;
    logic spec;
  } spatz_mem_req_t;

  typedef struct packed {
    logic [$clog2(NRVREG)-1:0] id;
    logic [DataWidth-1:0] data;
    logic err;
  } spatz_mem_rsp_t;


  // ----------------
  // Wire Definitions
  // ----------------

  // Data port signals
  snitch_pkg::dreq_t  data_req_d, data_req_q, snitch_req;
  snitch_pkg::dresp_t data_resp_d, data_resp_q, snitch_resp;

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

  // Spatz floating point signals
  fpnew_pkg::roundmode_e fpu_rnd_mode;
  fpnew_pkg::fmt_mode_t fpu_fmt_mode;
  fpnew_pkg::status_t fpu_status;
  acc_issue_rsp_t acc_req_rsp;

  // Spatz floating point mem signals
  reqrsp_req_t fp_lsu_mem_req;
  reqrsp_rsp_t fp_lsu_mem_rsp;

  // Spatz TCDM mem ports
  spatz_mem_req_t [NumMemPortsPerSpatz-1:0] spatz_mem_req;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_req_valid;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_req_ready;
  spatz_mem_rsp_t [NumMemPortsPerSpatz-1:0] spatz_mem_rsp;
  logic           [NumMemPortsPerSpatz-1:0] spatz_mem_rsp_valid;

  // Snitch Integer Core
  // TODO: where to put these eleboration parameters: spatz_pkg or top level?
  snitch_md #(
    .BootAddr   ( BootAddr  ),
    .MTVEC      ( MTVEC     ),
    .RVE        ( RVE       ),
    .RVM        ( RVM       ),
    .XFVEC      ( XFVEC     ),
    .XFDOTP     ( XFDOTP    ),
    .XFAUX      ( XFAUX     ),
    .RVF        ( RVF       ),
    .RVD        ( RVD       ),
    .XF16       ( XF16      ),
    .XF16ALT    ( XF16ALT   ),
    .XF8        ( XF8       ),
    .XF8ALT     ( XF8ALT    ),
    .acc_issue_rsp_t  ( acc_issue_rsp_t )
  ) i_snitch (
    .clk_i                  ( clk_i                  ), // checked
    .rst_i                  ( rst_i                  ), // checked
    .hart_id_i              ( hart_id_i              ), // checked
    .inst_addr_o            ( inst_addr_o            ), // checked
    .inst_data_i            ( inst_data_i            ), // checked
    .inst_valid_o           ( inst_valid_o           ), // checked
    .inst_ready_i           ( inst_ready_i           ), // checked
    .acc_qaddr_o            ( acc_req_d.addr         ), // checked
    .acc_qid_o              ( acc_req_d.id           ), // checked, 4:0 currently
    .acc_qdata_op_o         ( acc_req_d.data_op      ), // checked, 32 bits, HW
    .acc_qdata_arga_o       ( acc_req_d.data_arga    ), // checked, 32 bits, HW
    .acc_qdata_argb_o       ( acc_req_d.data_argb    ), // checked, 32 bits, HW
    .acc_qdata_argc_o       ( acc_req_d.data_argc    ), // checked, 32 bits, HW
    .acc_qvalid_o           ( acc_req_d_valid        ), // checked
    .acc_qready_i           ( acc_req_d_ready        ), // checked
    .acc_pdata_i            ( acc_resp_q.data        ), // checked, 32 bits, HW
    .acc_pid_i              ( acc_resp_q.id          ), // checked, 4:0 currently TODO: 6 bits?
    .acc_perror_i           ( acc_resp_q.error       ), // checked
    .acc_pvalid_i           ( acc_resp_q_valid       ), // checked
    .acc_pready_o           ( acc_resp_q_ready       ), // checked
    .acc_qdata_rsp_i        ( acc_req_rsp            ), // used by spatz decoder
    .acc_mem_finished_i     ( spatz_mem_finished     ), // used by spatz mem
    .acc_mem_str_finished_i ( spatz_mem_str_finished ), // used by spatz mem
    .data_qaddr_o           ( snitch_req.addr        ), // checked, 32 bits, HW
    .data_qwrite_o          ( snitch_req.write       ), // checked
    .data_qamo_o            ( snitch_req.amo         ), // checked
    .data_qdata_o           ( snitch_req.data        ), // checked, 32 bits, HW
    .data_qstrb_o           ( snitch_req.strb        ), // checked
    .data_qid_o             ( snitch_req.id          ), // MetaIdWidth
    .data_qvalid_o          ( snitch_req_valid       ), // checked
    .data_qready_i          ( snitch_req_ready       ), // checked
    .data_pdata_i           ( snitch_resp.data       ), // checked, 32 bits, HW 
    .data_perror_i          ( snitch_resp.error      ), // chekced
    .data_pid_i             ( snitch_resp.id         ), // MetaIdWidth
    .data_pvalid_i          ( snitch_resp_valid      ), // checked
    .data_pready_o          ( snitch_resp_ready      ), // checked
    .wake_up_sync_i         ( wake_up_sync_i         ), // checked
    .fpu_fmt_mode_o         ( fpu_fmt_mode           ), // checked
    .fpu_rnd_mode_o         ( fpu_rnd_mode           ), // used by spatz, FPU
    .fpu_status_i           ( fpu_status             ), // used by spatz, FPU
    .core_events_o          ( core_events_o          )  // checked
  );

  // Cut off-loading request path
  spill_register #(
    .T      ( snitch_pkg::acc_req_t ),
    .Bypass ( !RegisterOffloadReq   )
  ) i_spill_register_acc_req (
    .clk_i   ,
    .rst_ni  ( ~rst_i          ),
    .valid_i ( acc_req_d_valid ),
    .ready_o ( acc_req_d_ready ),
    .data_i  ( acc_req_d       ),
    .valid_o ( acc_req_q_valid ),
    .ready_i ( acc_req_q_ready ),
    .data_o  ( acc_req_q       )
  );

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

  // Snitch IPU accelerator
  // Replace with Spatz

  // snitch_ipu #(
  //   .IdWidth ( 5 )
  // ) i_snitch_ipu (
  //   .clk_i                                   ,
  //   .rst_i                                   ,
  //   .acc_qaddr_i      ( acc_req_q.addr      ),
  //   .acc_qid_i        ( acc_req_q.id        ),
  //   .acc_qdata_op_i   ( acc_req_q.data_op   ),
  //   .acc_qdata_arga_i ( acc_req_q.data_arga ),
  //   .acc_qdata_argb_i ( acc_req_q.data_argb ),
  //   .acc_qdata_argc_i ( acc_req_q.data_argc ),
  //   .acc_qvalid_i     ( acc_req_q_valid     ),
  //   .acc_qready_o     ( acc_req_q_ready     ),
  //   .acc_pdata_o      ( acc_resp_d.data     ),
  //   .acc_pid_o        ( acc_resp_d.id       ),
  //   .acc_perror_o     ( acc_resp_d.error    ),
  //   .acc_pvalid_o     ( acc_resp_d_valid    ),
  //   .acc_pready_i     ( acc_resp_d_ready    )
  // );

  

  spatz #(
    .NrMemPorts         ( NumMemPortsPerSpatz     ),
    .NumOutstandingLoads( NumSpatzOutstandingLoads),
    .FPUImplementation  ( FPUImplementation       ),
    .RegisterRsp        ( RegisterOffloadRsp      ),
    .dreq_t             ( reqrsp_req_t            ),  // checked
    .drsp_t             ( reqrsp_rsp_t            ),  // checked
    .spatz_mem_req_t    ( spatz_mem_req_t         ),  // checked
    .spatz_mem_rsp_t    ( spatz_mem_rsp_t         ),  // checked
    .spatz_issue_req_t  ( snitch_pkg::acc_req_t   ),  // checked, bad naming
    .spatz_issue_rsp_t  ( acc_issue_rsp_t         ),  // checked
    .spatz_rsp_t        ( snitch_pkg::acc_rsp_t   )   // checked, bad naming
  ) i_spatz (
    .clk_i                   ( clk_i                 ), // checked
    .rst_ni                  ( ~rst_i                ), // checked
    .testmode_i              ( testmode_i            ), // checked
    .hart_id_i               ( hart_id_i             ), // checked
    .issue_valid_i           ( acc_req_q_valid       ), // checked
    .issue_ready_o           ( acc_req_q_ready       ), // checked
    .issue_req_i             ( acc_req_q             ), // checked
    .issue_rsp_o             ( acc_req_rsp           ), // checked
    .rsp_valid_o             ( acc_resp_d_valid      ), // checked
    .rsp_ready_i             ( acc_resp_d_ready      ), // checked
    .rsp_o                   ( acc_resp_d            ), // checked, super bad naming
    .spatz_mem_req_o         ( spatz_mem_req         ), // checked
    .spatz_mem_req_valid_o   ( spatz_mem_req_valid   ), // checked
    .spatz_mem_req_ready_i   ( spatz_mem_req_ready   ), // checked
    .spatz_mem_rsp_i         ( spatz_mem_rsp         ), // checked
    .spatz_mem_rsp_valid_i   ( spatz_mem_rsp_valid   ), // checked, ***notice no ready signal here***
    .spatz_mem_finished_o    ( spatz_mem_finished    ), // checked
    .spatz_mem_str_finished_o( spatz_mem_str_finished), // checked
    .fp_lsu_mem_req_o        ( fp_lsu_mem_req        ), // checked
    .fp_lsu_mem_rsp_i        ( fp_lsu_mem_rsp        ), // checked
    .fpu_rnd_mode_i          ( fpu_rnd_mode          ), // checked
    .fpu_fmt_mode_i          ( fpu_fmt_mode          ), // checked
    .fpu_status_o            ( fpu_status            )  // checked
  );


  if (RVF || RVD) begin: gen_id_remapper
    // Merge Snitch and FP Subsequencer memory interfaces
    tcdm_id_remapper #(
      .NumIn(2)
    ) i_id_remapper (
      .clk_i       (clk_i                                     ),
      .rst_ni      (~rst_i                                    ),
      .req_i       ({fp_lsu_mem_req, snitch_req}              ),
      .req_valid_i ({fp_lsu_mem_req_valid, snitch_req_valid}  ),
      .req_ready_o ({fp_lsu_mem_req_ready, snitch_req_ready}  ),
      .resp_o      ({fp_lsu_mem_rsp, snitch_resp}             ),
      .resp_valid_o({fp_lsu_mem_resp_valid, snitch_resp_valid}),
      .resp_ready_i({fp_lsu_mem_resp_ready, snitch_resp_ready}),
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

    assign fp_lsu_resp           = '0;
    assign fp_lsu_mem_resp_valid = 1'b0;
    assign fp_lsu_mem_req_ready  = 1'b0;
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

  // TODO: Perhaps put it into a module
  // Assign TCDM data interface
  for (genvar i = 0; i < NumMemPortsPerSpatz; i++) begin
    assign data_qaddr_o[i+1]       = spatz_mem_req[i].addr;   // checked
    assign data_qwrite_o[i+1]      = spatz_mem_req[i].write;  // checked
    assign data_qamo_o[i+1]        = '0;                      // checked
    assign data_qdata_o[i+1]       = spatz_mem_req[i].data;   // checked
    assign data_qstrb_o[i+1]       = spatz_mem_req[i].strb;   // checked
    assign data_qid_o[i+1]         = spatz_mem_req[i].id;     // checked
    assign data_qvalid_o[i+1]      = spatz_mem_req_valid[i];  // checked
    assign spatz_mem_req_ready[i]  = data_qready_i[i+1];      // checked
    assign spatz_mem_rsp[i].data   = data_pdata_i[i+1];       // checked
    assign spatz_mem_rsp[i].id     = data_pid_i[i+1];         // checked
    assign spatz_mem_rsp[i].err    = data_perror_i[i+1];      // checked
    assign spatz_mem_rsp_valid[i]  = data_pvalid_i[i+1];      // checked
    assign data_pready_o[i+1]      = '1;                      // *** no ready signal for spatz here, tie to 1 ***
  end

  // Assign TCDM data interface
  assign data_qaddr_o[0]      = data_req_q.addr;
  assign data_qwrite_o[0]     = data_req_q.write;
  assign data_qamo_o[0]       = data_req_q.amo;
  assign data_qdata_o[0]      = data_req_q.data;
  assign data_qstrb_o[0]      = data_req_q.strb;
  assign data_qid_o[0]        = data_req_q.id;
  assign data_qvalid_o[0]     = data_req_q_valid;
  assign data_req_q_ready     = data_qready_i[0];
  assign data_resp_d.data     = data_pdata_i[0];
  assign data_resp_d.id       = data_pid_i[0];
  assign data_resp_d.write    = 'x; // Don't care here
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

  always_ff @(posedge rst_i) begin
    if(rst_i) begin
      // Format in hex because vcs and vsim treat decimal differently
      // Format with 8 digits because Verilator does not support anything else
      $sformat(fn, "trace_hart_0x%08x.dasm", hart_id_i);
      f = $fopen(fn, "w");
      $display("[Tracer] Logging Hart %d to %s", hart_id_i, fn);
    end
  end

  typedef enum logic [1:0] {SrcSnitch =  0, SrcFpu = 1, SrcFpuSeq = 2} trace_src_e;
  localparam int SnitchTrace = `ifdef SNITCH_TRACE `SNITCH_TRACE `else 0 `endif;

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
          extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "gpr_rdata_2", i_snitch.gpr_rdata[2]);
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
  // pragma translate_on

endmodule
