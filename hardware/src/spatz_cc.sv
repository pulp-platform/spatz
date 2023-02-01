// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

`include "spatz/spatz.svh"

/* verilator lint_off DECLFILENAME */
module spatz_cc
  import fpnew_pkg::roundmode_e;
  import fpnew_pkg::status_t;
  import spatz_xif_pkg::*;
  #(
    parameter logic          [31:0] BootAddr            = 32'h0000_1000,
    parameter logic          [31:0] MTVEC               = BootAddr,
    parameter bit                   RVE                 = 0,             // Reduced-register extension
    parameter bit                   RVM                 = 1,             // Enable Integer Multiplication & Division Extension
    parameter bit                   RVF                 = 0,             // Enable SP Floating Point Extension
    parameter bit                   RVD                 = 0,             // Enable SP Floating Point Extension
    parameter bit                   RVV                 = 1,             // Enable Vector Extension
    parameter bit                   RegisterOffloadReq  = 1,
    parameter bit                   RegisterOffloadResp = 1,
    parameter bit                   RegisterTCDMReq     = 0,
    parameter bit                   RegisterTCDMResp    = 0,
    parameter type                  cc_req_t            = logic,
    parameter type                  cc_resp_t           = logic,
    parameter type                  data_t              = logic,
    parameter type                  strb_t              = logic,
    parameter type                  meta_id_t           = logic,
    parameter int   unsigned        NumDataPortsPerCore = 0,
    parameter int   unsigned        NumMemPortsPerSpatz = 0
  ) (
    input  logic                                 clk_i,
    input  logic                                 rst_i,
    input  logic [31:0]                          hart_id_i,
    // Instruction Port
    output logic [31:0]                          inst_addr_o,
    input  logic [31:0]                          inst_data_i,
    output logic                                 inst_valid_o,
    input  logic                                 inst_ready_i,
    // TCDM Ports
    output logic [NumDataPortsPerCore-1:0][31:0] data_qaddr_o,
    output logic [NumDataPortsPerCore-1:0]       data_qwrite_o,
    output logic [NumDataPortsPerCore-1:0][3:0]  data_qamo_o,
    output `STRUCT_VECT(data_t, [NumDataPortsPerCore-1:0]) data_qdata_o,
    output `STRUCT_VECT(strb_t, [NumDataPortsPerCore-1:0]) data_qstrb_o,
    output `STRUCT_VECT(meta_id_t, [NumDataPortsPerCore-1:0]) data_qid_o,
    output logic [NumDataPortsPerCore-1:0] data_qvalid_o,
    input  logic [NumDataPortsPerCore-1:0] data_qready_i,
    input `STRUCT_VECT(data_t, [NumDataPortsPerCore-1:0]) data_pdata_i,
    input logic [NumDataPortsPerCore-1:0] data_perror_i,
    input `STRUCT_VECT(meta_id_t, [NumDataPortsPerCore-1:0]) data_pid_i,
    input  logic [NumDataPortsPerCore-1:0] data_pvalid_i,
    output logic [NumDataPortsPerCore-1:0] data_pready_o,
    input  logic                           wake_up_sync_i,
    // Core event strobes
    output `STRUCT_PORT(snitch_pkg::core_events_t) core_events_o
  );

// Include FF
`include "common_cells/registers.svh"

  // Data port signals
  cc_req_t  data_req_d, data_req_q;
  cc_resp_t data_resp_d, data_resp_q;
  logic     data_req_d_valid, data_req_d_ready, data_resp_d_valid, data_resp_d_ready;
  logic     data_req_q_valid, data_req_q_ready, data_resp_q_valid, data_resp_q_ready;

  // Snitch signals
  cc_req_t  snitch_req;
  cc_resp_t snitch_resp;
  logic     snitch_req_valid, snitch_req_ready, snitch_resp_valid, snitch_resp_ready;

  logic                      x_issue_valid;
  logic                      x_issue_ready;
  spatz_x_issue_req_t        x_issue_req;
  spatz_x_issue_resp_t       x_issue_resp;
  logic                      x_result_valid_d, x_result_valid_q;
  logic                      x_result_ready_d, x_result_ready_q;
  spatz_x_result_t           x_result_d, x_result_q;
  logic                [1:0] spatz_mem_finished;
  logic                [1:0] spatz_mem_str_finished;
  roundmode_e                fpu_rnd_mode;
  status_t                   fpu_status;

  // Snitch Integer Core
  snitch #(
    .BootAddr       ( BootAddr             ),
    .MTVEC          ( MTVEC                ),
    .RVE            ( RVE                  ),
    .RVM            ( RVM                  ),
    .RVF            ( RVF                  ),
    .RVD            ( RVD                  ),
    .RVV            ( RVV                  ),
    .x_issue_req_t  ( spatz_x_issue_req_t  ),
    .x_issue_resp_t ( spatz_x_issue_resp_t ),
    .x_result_t     ( spatz_x_result_t     )
  ) i_snitch (
    .clk_i               ( clk_i                  ),
    .rst_i               ( rst_i                  ),
    .hart_id_i           ( hart_id_i              ),
    .inst_addr_o         ( inst_addr_o            ),
    .inst_data_i         ( inst_data_i            ),
    .inst_valid_o        ( inst_valid_o           ),
    .inst_ready_i        ( inst_ready_i           ),
    .x_issue_valid_o     ( x_issue_valid          ),
    .x_issue_ready_i     ( x_issue_ready          ),
    .x_issue_req_o       ( x_issue_req            ),
    .x_issue_resp_i      ( x_issue_resp           ),
    .x_result_valid_i    ( x_result_valid_q       ),
    .x_result_ready_o    ( x_result_ready_q       ),
    .x_result_i          ( x_result_q             ),
    .x_mem_finished_i    ( spatz_mem_finished     ),
    .x_mem_str_finished_i( spatz_mem_str_finished ),
    .data_qaddr_o        ( snitch_req.addr        ),
    .data_qwrite_o       ( snitch_req.write       ),
    .data_qamo_o         ( snitch_req.amo         ),
    .data_qdata_o        ( snitch_req.data        ),
    .data_qstrb_o        ( snitch_req.strb        ),
    .data_qid_o          ( snitch_req.id          ),
    .data_qvalid_o       ( snitch_req_valid       ),
    .data_qready_i       ( snitch_req_ready       ),
    .data_pdata_i        ( snitch_resp.data       ),
    .data_perror_i       ( snitch_resp.error      ),
    .data_pid_i          ( snitch_resp.id         ),
    .data_pvalid_i       ( snitch_resp_valid      ),
    .data_pready_o       ( snitch_resp_ready      ),
    .wake_up_sync_i      ( wake_up_sync_i         ),
    .core_events_o       ( core_events_o          ),
    .fpu_rnd_mode_o      ( fpu_rnd_mode           ),
    .fpu_status_i        ( fpu_status             )
  );

  // Cut off-loading response path
  spill_register #(
    .T      ( spatz_x_result_t     ),
    .Bypass ( !RegisterOffloadResp )
  ) i_spill_register_acc_resp (
    .clk_i   ( clk_i            ),
    .rst_ni  ( ~rst_i           ),
    .valid_i ( x_result_valid_d ),
    .ready_o ( x_result_ready_d ),
    .data_i  ( x_result_d       ),
    .valid_o ( x_result_valid_q ),
    .ready_i ( x_result_ready_q ),
    .data_o  ( x_result_q       )
  );

  // Snitch IPU accelerator
  import spatz_pkg::spatz_mem_req_t;
  import spatz_pkg::spatz_mem_resp_t;

  spatz_mem_req_t  [NumMemPortsPerSpatz-1:0] spatz_mem_req;
  logic            [NumMemPortsPerSpatz-1:0] spatz_mem_req_valid;
  logic            [NumMemPortsPerSpatz-1:0] spatz_mem_req_ready;
  spatz_mem_resp_t [NumMemPortsPerSpatz-1:0] spatz_mem_resp;
  logic            [NumMemPortsPerSpatz-1:0] spatz_mem_resp_valid;
  logic            [NumMemPortsPerSpatz-1:0] spatz_mem_resp_ready;

  spatz_mem_req_t  fp_lsu_mem_req;
  logic            fp_lsu_mem_req_valid;
  logic            fp_lsu_mem_req_ready;
  spatz_mem_resp_t fp_lsu_mem_resp;
  logic            fp_lsu_mem_resp_valid;
  logic            fp_lsu_mem_resp_ready;

  spatz #(
    .NrMemPorts    ( NumMemPortsPerSpatz  ),
    .x_issue_req_t ( spatz_x_issue_req_t  ),
    .x_issue_resp_t( spatz_x_issue_resp_t ),
    .x_result_t    ( spatz_x_result_t     )
  ) i_spatz (
    .clk_i                   ( clk_i                  ),
    .rst_ni                  ( ~rst_i                 ),
    .x_issue_valid_i         ( x_issue_valid          ),
    .x_issue_ready_o         ( x_issue_ready          ),
    .x_issue_req_i           ( x_issue_req            ),
    .x_issue_resp_o          ( x_issue_resp           ),
    .x_result_valid_o        ( x_result_valid_d       ),
    .x_result_ready_i        ( x_result_ready_d       ),
    .x_result_o              ( x_result_d             ),
    .spatz_mem_req_o         ( spatz_mem_req          ),
    .spatz_mem_req_valid_o   ( spatz_mem_req_valid    ),
    .spatz_mem_req_ready_i   ( spatz_mem_req_ready    ),
    .spatz_mem_resp_i        ( spatz_mem_resp         ),
    .spatz_mem_resp_valid_i  ( spatz_mem_resp_valid   ),
    .spatz_mem_resp_ready_o  ( spatz_mem_resp_ready   ),
    .spatz_mem_finished_o    ( spatz_mem_finished     ),
    .spatz_mem_str_finished_o( spatz_mem_str_finished ),
    .fp_lsu_mem_req_o        ( fp_lsu_mem_req         ),
    .fp_lsu_mem_req_valid_o  ( fp_lsu_mem_req_valid   ),
    .fp_lsu_mem_req_ready_i  ( fp_lsu_mem_req_ready   ),
    .fp_lsu_mem_resp_i       ( fp_lsu_mem_resp        ),
    .fp_lsu_mem_resp_valid_i ( fp_lsu_mem_resp_valid  ),
    .fp_lsu_mem_resp_ready_o ( fp_lsu_mem_resp_ready  ),
    .fpu_rnd_mode_i          ( fpu_rnd_mode           ),
    .fpu_status_o            ( fpu_status             )
  );

  // Assign TCDM data interface
  for (genvar i = 0; i < NumMemPortsPerSpatz; i++) begin
    assign data_qaddr_o[i+1]       = spatz_mem_req[i].addr;
    assign data_qwrite_o[i+1]      = spatz_mem_req[i].we;
    assign data_qamo_o[i+1]        = '0;
    assign data_qdata_o[i+1]       = spatz_mem_req[i].wdata;
    assign data_qstrb_o[i+1]       = spatz_mem_req[i].strb;
    assign data_qid_o[i+1]         = spatz_mem_req[i].id;
    assign data_qvalid_o[i+1]      = spatz_mem_req_valid[i];
    assign spatz_mem_req_ready[i]  = data_qready_i[i+1];
    assign spatz_mem_resp[i].rdata = data_pdata_i[i+1];
    assign spatz_mem_resp[i].id    = data_pid_i[i+1];
    assign spatz_mem_resp[i].err   = data_perror_i[i+1];
    assign spatz_mem_resp_valid[i] = data_pvalid_i[i+1];
    assign data_pready_o[i+1]      = spatz_mem_resp_ready[i];
  end

  // FP interface
  cc_req_t fp_lsu_req;
  assign fp_lsu_req = '{
      addr   : fp_lsu_mem_req.addr,
      write  : fp_lsu_mem_req.we,
      data   : fp_lsu_mem_req.wdata,
      strb   : fp_lsu_mem_req.strb,
      id     : fp_lsu_mem_req.id,
      default: '0
    };

  cc_resp_t fp_lsu_resp;
  assign fp_lsu_mem_resp = '{
      rdata: fp_lsu_resp.data,
      id   : fp_lsu_resp.id,
      err  : fp_lsu_resp.error
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
      .resp_o      ({fp_lsu_resp, snitch_resp}                ),
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
    .T      ( cc_req_t         ),
    .Bypass ( !RegisterTCDMReq )
  ) i_spill_register_tcdm_req (
    .clk_i   ( clk_i            ) ,
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
    .T      ( cc_resp_t         ),
    .Bypass ( !RegisterTCDMResp )
  ) i_spill_register_tcdm_resp (
    .clk_i   ( clk_i             ),
    .rst_ni  ( ~rst_i            ),
    .valid_i ( data_resp_d_valid ),
    .ready_o ( data_resp_d_ready ),
    .data_i  ( data_resp_d       ),
    .valid_o ( data_resp_q_valid ),
    .ready_i ( data_resp_q_ready ),
    .data_o  ( data_resp_q       )
  );

  // Assign TCDM data interface
  assign data_qaddr_o[0]   = data_req_q.addr;
  assign data_qwrite_o[0]  = data_req_q.write;
  assign data_qamo_o[0]    = data_req_q.amo;
  assign data_qdata_o[0]   = data_req_q.data;
  assign data_qstrb_o[0]   = data_req_q.strb;
  assign data_qid_o[0]     = data_req_q.id;
  assign data_qvalid_o[0]  = data_req_q_valid;
  assign data_req_q_ready  = data_qready_i[0];
  assign data_resp_d.data  = data_pdata_i[0];
  assign data_resp_d.id    = data_pid_i[0];
  assign data_resp_d.error = data_perror_i[0];
  assign data_resp_d_valid = data_pvalid_i[0];
  assign data_pready_o[0]  = data_resp_d_ready;

  // --------------------------
  // Tracer
  // --------------------------
  // pragma translate_off
  int                    f;
  string                 fn;
  logic           [63:0] cycle;
  int    unsigned        stall, stall_ins, stall_raw, stall_lsu, stall_acc;

  always_ff @(posedge rst_i) begin
    if(rst_i) begin
      $sformat(fn, "trace_hart_0x%04x.dasm", hart_id_i);
      f = $fopen(fn, "w");
      $display("[Tracer] Logging Hart %d to %s", hart_id_i, fn);
    end
  end

  typedef enum logic [1:0] {SrcSnitch = 0, SrcFpu = 1, SrcFpuSeq = 2} trace_src_e;
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
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "source", SrcSnitch);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall", i_snitch.stall);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_tot", stall);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_ins", stall_ins);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_raw", stall_raw);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_lsu", stall_lsu);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "stall_acc", stall_acc);
        // Decoding
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rs1", i_snitch.rs1);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rs2", i_snitch.rs2);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "rd", i_snitch.rd);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "is_load", i_snitch.is_load);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "is_store", i_snitch.is_store);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "is_branch", i_snitch.is_branch);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "pc_d", i_snitch.pc_d);
        // Operands
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opa", i_snitch.opa);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opb", i_snitch.opb);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opa_select", i_snitch.opa_select);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "opb_select", i_snitch.opb_select);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "write_rd", i_snitch.write_rd);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "csr_addr", i_snitch.inst_data_i[31:20]);
        // Pipeline writeback
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "writeback", i_snitch.alu_writeback);
        // Load/Store
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "gpr_rdata_1", i_snitch.gpr_rdata[1]);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "ls_size", i_snitch.ls_size);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "ld_result_32",i_snitch.ld_result[31:0]);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "lsu_rd", i_snitch.lsu_rd);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "retire_load", i_snitch.retire_load);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "alu_result", i_snitch.alu_result);
        // Atomics
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "ls_amo", i_snitch.ls_amo);
        // Accumulator
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "retire_acc", i_snitch.retire_acc);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "acc_pid", i_snitch.x_result_i.id);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "acc_pdata_32",i_snitch.x_result_i.data[31:0]);
        // FPU offload
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "fpu_offload", 1'b0);
        extras_str = $sformatf("%s'%s': 0x%8x, ", extras_str, "is_seq_insn", 1'b0);
        extras_str = $sformatf("%s}", extras_str);

        $sformat(trace_entry, "%t %8d 0x%h DASM(%h) #; %s\n",
          $time, cycle, i_snitch.pc_q, i_snitch.inst_data_i, extras_str);
        $fwrite(f, trace_entry);
      end

      // Reset all stalls when we execute an instruction
      if (!i_snitch.stall) begin
        stall     <= 0;
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
      cycle     <= '0;
      stall     <= 0;
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
/* verilator lint_on DECLFILENAME */

