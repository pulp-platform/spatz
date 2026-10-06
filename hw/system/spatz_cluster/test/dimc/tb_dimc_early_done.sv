// SPDX-License-Identifier: Apache-2.0
// Directed DIMC queue turnover, numerical results, and VRF backpressure check.
module tb_dimc_early_done;
  import spatz_pkg::*;
  import rvv_pkg::*;
  localparam int Count = 64;
  logic clk = 0;
  always #5 clk = ~clk;
  logic rst_n = 0;
  spatz_req_t req;
  logic req_valid = 0, req_ready, rsp_valid;
  vfu_rsp_t rsp;
  vrf_addr_t waddr;
  vrf_data_t wdata;
  vrf_be_t wbe;
  logic we, wready = 1;
  spatz_id_t [3:0] ids;
  vrf_addr_t [2:0] raddr;
  logic [2:0] re, rvalid = '1;
  vrf_data_t [2:0] rdata;
  vrf_data_t mem [NrVRFWords];
  logic [NrParallelInstructions-1:0] in_flight = '0;
  int sent=0, completed=0, written=0, released=0, ticks=0;
  int turnovers=0, held_write=0, wait_result=0;
  int expected [Count][8];
  int expected_addr [Count];
  bit expect_early;

  spatz_vfu dut (
    .clk_i(clk), .rst_ni(rst_n), .hart_id_i('0),
    .spatz_req_i(req), .spatz_req_valid_i(req_valid), .spatz_req_ready_o(req_ready),
    .vfu_rsp_valid_o(rsp_valid), .vfu_rsp_ready_i(1'b1), .vfu_rsp_o(rsp),
    .vrf_waddr_o(waddr), .vrf_wdata_o(wdata), .vrf_we_o(we), .vrf_wbe_o(wbe),
    .vrf_wvalid_i(wready), .vrf_id_o(ids), .vrf_raddr_o(raddr),
    .vrf_re_o(re), .vrf_rdata_i(rdata), .vrf_rvalid_i(rvalid), .fpu_status_o()
  );

  always_comb begin
    for (int port=0; port<3; port++) rdata[port] = mem[raddr[port]];
  end
  // Change ready only on falling edges, so neither the DUT nor the scoreboard
  // observes a different handshake value at the same rising edge.
  always @(negedge clk) if (rst_n) begin
    wready = completed < 16 || (ticks % 97 < 35 || ticks % 97 > 62);
    rvalid = (ticks % 13 != 4 && ticks % 13 != 5) ? '1 : '0;
  end

  function automatic int dot_value(int kernel_byte, int feature_byte, int ci, int vl);
    int width, mask, value;
    width=1 << (ci & 3); mask=(1 << width)-1; value=0;
    for (int bitpos=0; bitpos<(vl << (ci & 3)); bitpos+=width)
      value += ((kernel_byte >> (bitpos % 8)) & mask) *
               ((feature_byte >> (bitpos % 8)) & mask);
    return value;
  endfunction

  initial begin
    expect_early=$test$plusargs("expect_early");
    req='0;
    foreach (mem[i]) mem[i]='0;
    for (int regno=0; regno<16; regno++)
      for (int wordno=0; wordno<NrWordsPerVector; wordno++)
        for (int b=0; b<VRFWordWidth/8; b++)
          mem[regno*NrWordsPerVector+wordno][b*8 +: 8]=8'(regno+1);
    for (int wordno=0; wordno<NrWordsPerVector; wordno++) begin
      mem[30*NrWordsPerVector+wordno]={VRFWordWidth/8{8'd2}};
      mem[31*NrWordsPerVector+wordno]={VRFWordWidth/8{8'd1}};
    end
    repeat(3) @(negedge clk);
    rst_n=1;
    for (int n=0; n<Count; n++) begin
      automatic int id=n % NrParallelInstructions;
      automatic int group=n < 16 ? 0 : 8;
      automatic int feature=(n/8)%2 ? 2 : 1;
      automatic int ci=n < 24 ? 3 : n < 32 ? 7 : (n/8)%4 + ((n%2)*4);
      automatic int vl=n >= 56 ? 64 : 128;
      while (in_flight[id]) @(negedge clk);
      req='0;
      req.id=spatz_id_t'(id);
      req.op=DIMC_OP; req.ex_unit=VFU;
      req.vs1=vreg_t'(feature == 1 ? 31 : 30);
      req.vs2=vreg_t'(group); req.vd=vreg_t'(16+id);
      req.use_vs1=1; req.use_vs2=1; req.use_vd=1;
      req.vl=vlen_t'(vl); req.vtype.vsew=EW_8;
      req.op_cfg.dimc.ci=3'(ci);
      req.op_cfg.dimc.kernel_load=n==0 || n==16;
      req.op_cfg.dimc.feature_reuse=n%8 != 0;
      req.op_cfg.dimc.compute_reuse=1;
      for (int row=0; row<8; row++)
        expected[n][row]=dot_value(group+row+1,feature,ci,vl);
      expected_addr[n]=(16+id)*NrWordsPerVector+((ci & 4) ? 1 : 0);
      req_valid=1;
      do @(posedge clk); while (!req_ready);
      @(negedge clk);
      req_valid=0;
    end
    wait(completed==Count);
    repeat(3) @(negedge clk);
    if (written!=Count || released!=Count || held_write==0 || wait_result==0)
      $fatal(1,"coverage writes=%0d releases=%0d held=%0d result_wait=%0d",written,released,held_write,wait_result);
    if (expect_early && turnovers==0) $fatal(1,"no final-row turnover exercised");
    $display("DIMC_EARLY_DONE_PASS instructions=%0d checked_results=%0d cycles=%0d turnovers=%0d held_write=%0d result_wait=%0d early=%0d",
             Count,Count*8,ticks,turnovers,held_write,wait_result,expect_early);
    $finish;
  end

  always @(posedge clk) if (rst_n) begin
    ticks=ticks+1;
    if (ticks>10000) $fatal(1,"timeout sent=%0d completed=%0d",sent,completed);
    if ($test$plusargs("debug") && (dut.dimc_start || dut.dimc_instr_done || rsp_valid))
      $display("tick=%0d state=%0d row=%0d start=%0d active=%0d done=%0d wb=%0d wb_id=%0d capture=%0d completed=%0d req=%0d",
        ticks,dut.dimc_state_q,dut.dimc_row_q,dut.dimc_start,dut.dimc_active_id_q,dut.dimc_instr_done,
        rsp_valid,rsp.id,dut.dimc_capture_complete,completed,dut.spatz_req.id);
    if (rsp_valid) begin
      if (rsp.id != spatz_id_t'(completed % NrParallelInstructions))
        $fatal(1,"wrong completion id n=%0d got=%0d expected=%0d state=%0d row=%0d",completed,rsp.id,completed % NrParallelInstructions,dut.dimc_state_q,dut.dimc_row_q);
      if (!(we && wready)) $fatal(1,"response before final VRF write acceptance");
      in_flight[rsp.id]=0;
      completed=completed+1;
    end
    if (req_valid && req_ready) begin
      if (in_flight[req.id]) $fatal(1,"request id reused before completion");
      in_flight[req.id]=1;
      sent=sent+1;
    end
    if (we && !wready) held_write=held_write+1;
    if (dut.dimc_state_q == 5) wait_result=wait_result+1;
    if (we && wready) begin
      if (ids[3] != spatz_id_t'(written % NrParallelInstructions) || waddr != expected_addr[written])
        $fatal(1,"wrong write tag/address n=%0d id=%0d addr=%0d expected=%0d",written,ids[3],waddr,expected_addr[written]);
      if (wbe != '1) $fatal(1,"partial result word byte enables");
      for (int row=0; row<8; row++)
        if (wdata[row*32 +: 32] != expected[written][row])
          $fatal(1,"mismatch n=%0d row=%0d got=%0d expected=%0d",written,row,wdata[row*32 +: 32],expected[written][row]);
      mem[waddr]=wdata;
      written=written+1;
    end
    if (dut.dimc_instr_done) begin
      if (expect_early ? (dut.dimc_state_q != 4 || dut.dimc_row_q != 6) :
          !((dut.dimc_state_q == 4 && dut.dimc_row_q == 7) || dut.dimc_state_q == 5))
        $fatal(1,"queue release at wrong row/state row=%0d state=%0d",dut.dimc_row_q,dut.dimc_state_q);
      released=released+1;
    end
    if (dut.dimc_start && dut.dimc_state_q == 4) turnovers=turnovers+1;
  end
endmodule
