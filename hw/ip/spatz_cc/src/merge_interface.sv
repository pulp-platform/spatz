// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Author: Michele Raeber, <micraebe@student.ethz.ch>

module merge_interface

  import spatz_pkg::*;
  import fpnew_pkg::*; #(
    parameter type acc_issue_req_t          = logic,
    parameter type acc_issue_rsp_t          = logic,
    parameter type acc_rsp_t                = logic
  ) (
  	input logic clk_i, 
  	input logic rst_ni,
    ///////////////
    // 
  	// Request interface between accelerator and cc-controller
  	input  logic acc_intf_qreq_ready_i,
    input  acc_issue_rsp_t acc_intf_issue_prsp_i,
  	output logic acc_intf_qreq_valid_o,
  	output acc_issue_req_t acc_intf_qreq_o,
  	// Response interface between accelerator and cc-controller
  	input  acc_rsp_t acc_intf_prsp_i,
  	input  logic acc_intf_prsp_valid_i,
  	output logic acc_intf_prsp_ready_o,
  	// Request interface between scalar core and cc-controller
  	input  acc_issue_req_t cpu_intf_qreq_i,
  	input  logic cpu_intf_qreq_valid_i,
  	output logic cpu_intf_qreq_ready_o,
    output acc_issue_rsp_t cpu_intf_issue_prsp_o,
  	// Response interface between scalar core and cc-controller
  	output acc_rsp_t cpu_intf_prsp_o,
  	output logic cpu_intf_prsp_valid_o,
  	input  logic cpu_intf_prsp_ready_i,
  	// Request interface between cc-controller and cc-controller
  	input  acc_issue_req_t s_cc_intf_qreq_i,
  	input  logic s_cc_intf_qreq_valid_i,
  	output logic s_cc_intf_qreq_ready_o,
  	// Response interface between cc-controller and cc-controller
  	output logic s_cc_intf_prsp_valid_o,
  	input  logic s_cc_intf_prsp_ready_i,
    // Request interface between cc-controller and cc-controller
    output acc_issue_req_t m_cc_intf_qreq_o,
    output logic m_cc_intf_qreq_valid_o,
    input  logic m_cc_intf_qreq_ready_i,
    // Response interface between cc-controller and cc-controller
    input  logic m_cc_intf_prsp_valid_i,
    output logic m_cc_intf_prsp_ready_o,
  	// Signal if merge mode is active and if controller has to act as slave or master device
    input merge_intf_e state_i,
  	input merge_mode_t merge_mode_i,

    input  roundmode_e  cpu_intf_fpu_rnd_mode_i,
    input  fmt_mode_t   cpu_intf_fpu_fmt_mode_i,

    output roundmode_e acc_intf_fpu_rnd_mode_o,
    output fmt_mode_t  acc_intf_fpu_fmt_mode_o,

    output roundmode_e m_cc_intf_fpu_rnd_mode_o,
    output fmt_mode_t  m_cc_intf_fpu_fmt_mode_o,

    input  roundmode_e s_cc_intf_fpu_rnd_mode_i,
    input  fmt_mode_t  s_cc_intf_fpu_fmt_mode_i
  );

  merge_intf_e state;

  // Combinational logic for request interface
  always_comb begin

    if (!merge_mode_i.is_merge) begin
      state = NON_MERGE;
    end else if (merge_mode_i.is_merge && merge_mode_i.is_master) begin
      state = MERGE_MASTER;
    end else if (merge_mode_i.is_merge && !merge_mode_i.is_master) begin
      state = MERGE_SLAVE;
    end
    
    case (state)
      NON_MERGE    :  begin

                        ///////////////
                        //  Request  //
                        ///////////////

                        // Connect the request-interface
                        acc_intf_qreq_o       = cpu_intf_qreq_i;
                        acc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                        cpu_intf_qreq_ready_o = acc_intf_qreq_ready_i;

                        ////////////////
                        //  Response  //
                        ////////////////

                        // Connect the response-interface
                        cpu_intf_prsp_o       = acc_intf_prsp_i;
                        cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                        acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;

                        /////////////////////////
                        //  FPU: Side Channel  //
                        /////////////////////////

                        acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;

                        // Do not drive the CC-interface
                        m_cc_intf_qreq_o       = '0;
                        m_cc_intf_qreq_valid_o = 1'b0;
                        s_cc_intf_qreq_ready_o = 1'b0;
                        s_cc_intf_prsp_valid_o = 1'b0;


                      end
      MERGE_SLAVE  :  begin
                        // Original owner Snitch is not trying to offload instructions  
                        if (!cpu_intf_qreq_valid_i) begin
                          acc_intf_qreq_o = s_cc_intf_qreq_i;
                          // Tag the instruction marking it as coming from the merge snitch
                          acc_intf_qreq_o.id[6] = 1'b1;
                          acc_intf_qreq_valid_o = s_cc_intf_qreq_valid_i;
                          s_cc_intf_qreq_ready_o = acc_intf_qreq_ready_i;

                          // Do not communicate with the owner Snitch
                          cpu_intf_qreq_ready_o = 1'b0;
                          cpu_intf_issue_prsp_o = '0;

                          /////////////////////////
                          //  FPU: Side Channel  //
                          /////////////////////////

                          acc_intf_fpu_rnd_mode_o = s_cc_intf_fpu_rnd_mode_i;
                          acc_intf_fpu_fmt_mode_o = s_cc_intf_fpu_fmt_mode_i;


                        end else begin
                          acc_intf_qreq_o = cpu_intf_qreq_i;
                          // Tag the instruction marking it as coming from the owner snitch
                          acc_intf_qreq_o.id[6] = 1'b0;
                          acc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                          // Communicate to merge snitch that spatz is unable to process requests
                          s_cc_intf_qreq_ready_o = 1'b0;
                          cpu_intf_qreq_ready_o = acc_intf_qreq_ready_i;
                          cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;

                          /////////////////////////
                          //  FPU: Side Channel  //
                          /////////////////////////

                          acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                          acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;
                        end

                        // Check if response is directed to merge snitch or owner snitch
                        if (acc_intf_prsp_valid_i && acc_intf_prsp_i.id[6] == 1'b1) begin
                          // Do not forward response, but forward valid signal
                          s_cc_intf_prsp_valid_o = acc_intf_prsp_valid_i;

                          // Do not communicate with owner Snitch
                          cpu_intf_prsp_o = '0;
                          cpu_intf_prsp_valid_o = 1'b0;
                        end else if (acc_intf_prsp_valid_i && acc_intf_prsp_i.id[6] == 1'b0) begin
                          cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                          cpu_intf_prsp_o = acc_intf_prsp_i;
                          acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;

                          // Do not communicate with merge Snitch
                          s_cc_intf_prsp_valid_o = 1'b0;
                        end

                        // Do not drive the master ports
                        m_cc_intf_qreq_o       = '0;
                        m_cc_intf_qreq_valid_o = 1'b0;
                        m_cc_intf_prsp_ready_o = 1'b0;                        
                      end
      MERGE_MASTER :  begin

                        // Connect the Spatz<->CPU request-interface
                        acc_intf_qreq_o = cpu_intf_qreq_i;
                        acc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                        cpu_intf_qreq_ready_o = acc_intf_qreq_ready_i && m_cc_intf_qreq_ready_i;
                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;

                        // Connect the Spatz<->CPU response-interface
                        cpu_intf_prsp_o = acc_intf_prsp_i;
                        cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i && m_cc_intf_prsp_valid_i;
                        acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i && m_cc_intf_prsp_valid_i;
                        m_cc_intf_prsp_ready_o = cpu_intf_prsp_ready_i && m_cc_intf_prsp_valid_i;

                        // Connect the CC-interface to forward to CC-slave
                        m_cc_intf_qreq_o = cpu_intf_qreq_i;
                        // Tag instruction with a 1
                        m_cc_intf_qreq_o.id[6] = merge_mode_i.is_master;
                        m_cc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                        m_cc_intf_prsp_ready_o = cpu_intf_prsp_ready_i && m_cc_intf_prsp_valid_i;

                        // Do not drive the CC-interface slave ports
                        s_cc_intf_qreq_ready_o = 1'b0;
                        s_cc_intf_prsp_valid_o = 1'b0;

                        /////////////////////////
                        //  FPU: Side Channel  //
                        /////////////////////////

                        acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;

                        m_cc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        m_cc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;
                      end
    endcase
  end
endmodule : merge_interface