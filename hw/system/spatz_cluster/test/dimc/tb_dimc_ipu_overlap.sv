// SPDX-License-Identifier: Apache-2.0
// Mixed DIMC/IPU results, shared-port backpressure, dependencies and FPU barrier.
module tb_dimc_ipu_overlap;
  import spatz_pkg::*;
  import rvv_pkg::*;
  logic clk=0, rst_n=0;
  always #5 clk=~clk;
  spatz_req_t req;
  logic req_valid=0, req_ready, rsp_valid;
  vfu_rsp_t rsp;
  vrf_addr_t waddr;
  vrf_data_t wdata;
  vrf_be_t wbe;
  logic we, wready, write_allow=1, read_allow=1;
  logic force_write_block=0;
  spatz_id_t [3:0] ids;
  vrf_addr_t [2:0] raddr;
  logic [2:0] re, rvalid;
  vrf_data_t [2:0] rdata;
  vrf_data_t mem [NrVRFWords];
  logic [NrParallelInstructions-1:0] in_flight='0;
  logic [NrParallelInstructions-1:0] deps [NrParallelInstructions];
  int expected [NrParallelInstructions][16];
  int base_addr [NrParallelInstructions], words [NrParallelInstructions], writes [NrParallelInstructions];
  int ticks=0, sent=0, completed=0, checked=0;
  int overlap=0, issue_overlap=0, write_conflict=0, held_result=0, read_contention=0, dependency_wait=0;

  spatz_vfu #(.FPUImplementation(spatz_cluster_pkg::FPUImplementation[0])) dut (
    .clk_i(clk), .rst_ni(rst_n), .hart_id_i('0),
    .spatz_req_i(req), .spatz_req_valid_i(req_valid), .spatz_req_ready_o(req_ready),
    .vfu_rsp_valid_o(rsp_valid), .vfu_rsp_ready_i(1'b1), .vfu_rsp_o(rsp),
    .vrf_waddr_o(waddr), .vrf_wdata_o(wdata), .vrf_we_o(we), .vrf_wbe_o(wbe),
    .vrf_wvalid_i(wready), .vrf_id_o(ids), .vrf_raddr_o(raddr),
    .vrf_re_o(re), .vrf_rdata_i(rdata), .vrf_rvalid_i(rvalid), .fpu_status_o()
  );

  always_comb begin
    wready=write_allow && !(| (deps[ids[3]] & in_flight));
    for (int p=0;p<3;p++) begin
      rdata[p]=mem[raddr[p]];
      rvalid[p]=rst_n && !(| (deps[ids[p]] & in_flight)) && read_allow;
    end
  end
  always @(negedge clk) if (rst_n) begin
    write_allow=!force_write_block && (ticks%71<21 || ticks%71>47);
    read_allow=ticks%17!=3;
  end

  function automatic int fp32_from_uint(int value);
    int exponent;
    exponent=0;
    while ((value >> exponent) > 1) exponent++;
    return ((127+exponent)<<23) | ((value-(1<<exponent))<<(23-exponent));
  endfunction

  // kind 0: DIMC, kind 1: sixteen integer sums, kind 2: eight floating sums.
  task automatic issue(int kind, int id, int vd, bit kernel_load=0,
                       int feature=31, int dependency=-1, bit reuse=0);
    while (in_flight[id]) @(negedge clk);
    req='0;
    req.id=spatz_id_t'(id); req.ex_unit=VFU; req.vd=vreg_t'(vd);
    req.use_vs1=1; req.use_vs2=1; req.use_vd=1;
    req.op_arith.vm=1; // These arithmetic checks use unmasked instructions.
    deps[id]='0;
    if (dependency>=0) deps[id][dependency]=1;
    base_addr[id]=vd*NrWordsPerVector;
    words[id]=kind==1 ? 2 : 1;
    writes[id]=0;
    if (kind==0) begin
      req.op=DIMC_OP; req.vs1=vreg_t'(feature); req.vs2=0;
      req.vl=128; req.vtype.vsew=EW_8;
      req.op_cfg.dimc.ci=3; req.op_cfg.dimc.kernel_load=kernel_load;
      req.op_cfg.dimc.feature_reuse=reuse;
      req.op_cfg.dimc.compute_reuse=1;
      for (int lane=0;lane<8;lane++) expected[id][lane]=(lane+1)*(feature==30 ? 2560 : 256);
    end else begin
      req.op=kind==1 ? VADD : VFADD;
      req.vs1=vreg_t'(kind==1 ? 20 : 22);
      req.vs2=vreg_t'(kind==1 ? 21 : 23);
      req.vl=vlen_t'(kind==1 ? 16 : 8); req.vtype.vsew=EW_32;
      for (int lane=0;lane<16;lane++)
        expected[id][lane]=kind==1 ? 400+2*lane : fp32_from_uint(3+2*lane);
    end
    req_valid=1;
    do @(posedge clk); while (!req_ready);
    @(negedge clk);
    req_valid=0;
  endtask

  initial begin
    req='0;
    foreach(mem[i]) mem[i]='0;
    foreach(deps[i]) deps[i]='0;
    for(int regno=0;regno<8;regno++)
      for(int wordno=0;wordno<NrWordsPerVector;wordno++)
        mem[regno*NrWordsPerVector+wordno]={VRFWordWidth/8{8'(regno+1)}};
    for(int wordno=0;wordno<NrWordsPerVector;wordno++)
      mem[31*NrWordsPerVector+wordno]={VRFWordWidth/8{8'd2}};
    for(int lane=0;lane<16;lane++) begin
      mem[20*NrWordsPerVector+lane/8][32*(lane%8)+:32]=100+lane;
      mem[21*NrWordsPerVector+lane/8][32*(lane%8)+:32]=300+lane;
      mem[22*NrWordsPerVector+lane/8][32*(lane%8)+:32]=fp32_from_uint(1+lane);
      mem[23*NrWordsPerVector+lane/8][32*(lane%8)+:32]=fp32_from_uint(2+lane);
    end
    repeat(3) @(negedge clk);
    rst_n=1;
    issue(0,0,16,1);
    wait(in_flight=='0); @(negedge clk);
    // Hold both independent result producers at the shared write port.
    force_write_block=1;
    issue(0,0,16);
    issue(1,2,24);
    wait(dut.dimc_write_valid && (&dut.ipu_result_valid));
    repeat(3) @(negedge clk);
    force_write_block=0;
    wait(in_flight=='0); @(negedge clk);
    for(int round=0;round<24;round++) begin
      issue(0,0,16);
      issue(1,2,24);
      issue(0,1,17);
      issue(1,3,25);
      wait(in_flight=='0); @(negedge clk);
    end
    // A reuse instruction can compute without reads, then wait to overwrite
    // an older IPU destination. It must yield the write port to that IPU.
    issue(1,2,16);
    issue(0,0,16,0,31,2,1);
    wait(in_flight=='0); @(negedge clk);
    // DIMC waits for an older IPU-produced feature. The read arbiter must let
    // the IPU finish even while DIMC's scoreboard blocks its own operand read.
    issue(1,2,30);
    issue(0,0,16,0,30,2);
    wait(in_flight=='0); @(negedge clk);
    // FPU work must drain on either side of the independent DIMC queue.
    issue(0,0,16);
    issue(2,3,26);
    issue(0,1,17);
    wait(in_flight=='0); repeat(3) @(negedge clk);
    if(sent!=completed || overlap==0 || issue_overlap==0 || write_conflict==0 || held_result==0 ||
       read_contention==0 || dependency_wait==0)
      $fatal(1,"coverage sent=%0d done=%0d overlap=%0d collision=%0d held=%0d reads=%0d dep=%0d",
        sent,completed,overlap,write_conflict,held_result,read_contention,dependency_wait);
    $display("DIMC_IPU_OVERLAP_PASS instructions=%0d checked_values=%0d cycles=%0d compute_ipu_overlap=%0d compute_ipu_issue_overlap=%0d write_conflict=%0d held_result=%0d read_contention=%0d dependency_wait=%0d",
      completed,checked,ticks,overlap,issue_overlap,write_conflict,held_result,read_contention,dependency_wait);
    $finish;
  end

  always @(posedge clk) if(rst_n) begin
    ticks++;
    if ($test$plusargs("debug") && sent<8 &&
        ((req_valid&&req_ready) || dut.word_issued || dut.result_ready || rsp_valid))
      $display("tick=%0d sent=%0d done=%0d accept=%0d reqid=%0d state=%0d op=%0d valid=%0d word=%0d last=%0d input_id=%0d fpu=%0d fvalid=%h outready=%0d outlast=%0d outid=%0d we=%0d addr=%0d rsp=%0d rspid=%0d writes3=%0d",
        ticks,sent,completed,req_valid&&req_ready,req.id,dut.state_q,dut.spatz_req.op,
        dut.spatz_req_valid,dut.word_issued,dut.last_request,dut.input_tag.id,dut.is_fpu_insn,
        dut.fpu_result_valid,dut.result_ready,dut.result_tag.last,dut.result_tag.id,
        we,waddr,rsp_valid,rsp.id,writes[3]);
    if ($test$plusargs("debug") && sent<8 && (we || dut.word_issued))
      $display("ARBITER ipu_valid=%h ready=%b write_allow=%b wready=%b pending=%h result_valid=%h tagwb=%b dimc_valid=%b dimc_grant=%b",dut.ipu_result_valid,dut.result_ready,write_allow,wready,dut.pending_results,dut.result_valid,dut.result_tag.wb,dut.dimc_write_valid,dut.dimc_write_grant);
    if(ticks>20000) $fatal(1,"timeout sent=%0d done=%0d inflight=%b state=%0d req=%0d op=%0d vl=%0d dimc=%0d fpu_busy=%0d fpu_valid=%h result_ready=%0d we=%0d mask=%h",
      sent,completed,in_flight,dut.state_q,dut.spatz_req_valid,dut.spatz_req.op,dut.vl_q,
      dut.dimc_req_valid,dut.is_fpu_busy,dut.fpu_result_valid,dut.result_ready,we,dut.pending_results);
    if(dut.dimc_compute_fire && dut.is_ipu_busy) overlap++;
    if(dut.dimc_compute_fire && dut.word_issued) issue_overlap++;
    if(dut.dimc_read_request && dut.normal_read_request) read_contention++;
    if(dut.dimc_read_request && (|(deps[dut.dimc_active_id_q]&in_flight))) dependency_wait++;
    if(dut.dimc_write_valid && (&dut.ipu_result_valid)) begin
      write_conflict++;
      if(dut.dimc_write_grant && dut.result_ready) $fatal(1,"IPU result consumed during granted DIMC write");
    end
    if((&dut.ipu_result_valid) && !dut.result_ready) held_result++;
    if(dut.dimc_compute_fire && dut.is_fpu_busy) $fatal(1,"exclusive FPU barrier bypassed");
    if(we && wready) begin
      automatic int id=int'(ids[3]);
      if(!in_flight[id] || waddr!=base_addr[id]+writes[id] || writes[id]>=words[id] || wbe!='1)
        $fatal(1,"wrong write id=%0d address=%0d index=%0d expected_base=%0d",id,waddr,writes[id],base_addr[id]);
      for(int lane=0;lane<8;lane++) begin
        if(wdata[32*lane+:32]!=expected[id][writes[id]*8+lane])
          $fatal(1,"wrong result id=%0d word=%0d lane=%0d got=%0d expected=%0d",id,writes[id],lane,wdata[32*lane+:32],expected[id][writes[id]*8+lane]);
        checked++;
      end
      mem[waddr]=wdata;
      writes[id]++;
    end
    if(rsp_valid) begin
      if(!in_flight[rsp.id] || writes[rsp.id]!=words[rsp.id] || !(we&&wready))
        $fatal(1,"completion before final accepted write id=%0d",rsp.id);
      in_flight[rsp.id]=0;
      completed++;
    end
    if(req_valid && req_ready) begin
      if(in_flight[req.id]) $fatal(1,"duplicate live request ID");
      in_flight[req.id]=1;
      sent++;
    end
  end
endmodule
