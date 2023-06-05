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
    output logic s_cc_intf_qreq_valid_o,
  	// Response interface between cc-controller and cc-controller
  	output logic s_cc_intf_prsp_valid_o,
  	input  logic s_cc_intf_prsp_ready_i,
    // Request interface between cc-controller and cc-controller
    output acc_issue_req_t m_cc_intf_qreq_o,
    output logic m_cc_intf_qreq_valid_o,
    input  logic m_cc_intf_qreq_ready_i,
    input  logic m_cc_intf_qreq_valid_i,
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
  logic is_ready;

  // Temporary combinatinal logic to set the correct state
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

                        // Directly connect the request interface Spatz<->Snitch
                        acc_intf_qreq_o         = cpu_intf_qreq_i;
                        // Tag instruction as coming from internal
                        acc_intf_qreq_o.core_id = 1'b0;
                        acc_intf_qreq_valid_o   = cpu_intf_qreq_valid_i;
                        cpu_intf_qreq_ready_o   = acc_intf_qreq_ready_i;

                        ////////////////
                        //  Response  //
                        ////////////////

                        // Directly connect the response interface Spatz<->Snitch
                        cpu_intf_prsp_o       = acc_intf_prsp_i;
                        cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                        acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;

                        /////////////////////////
                        //  FPU: Side Channel  //
                        /////////////////////////

                        // Directly connec the FPU side-channel Spatz<->Snitch
                        acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;

                        ////////////////////
                        //  CC<->CC Intf  //
                        ////////////////////

                        // Do not drive the CC<->CC interface
                        m_cc_intf_qreq_o       = 'z;
                        m_cc_intf_qreq_valid_o = 1'bz;
                        m_cc_intf_prsp_ready_o = 1'bz;
                        s_cc_intf_qreq_ready_o = 1'bz;
                        s_cc_intf_prsp_valid_o = 1'bz;


                      end
      MERGE_SLAVE  :  begin

                        ////////////////////////////////////////////////////////////////////////////
                        //                                                                        //
                        //   Default Master<->Slave relation  [Request & FPU]                     //
                        //                                                                        //
                        ////////////////////////////////////////////////////////////////////////////

                        // By default Spatz should obey its adopter
                        acc_intf_qreq_o = s_cc_intf_qreq_i;
                        // Tag incoming instruction as forwarded from adopter
                        acc_intf_qreq_o.core_id = 1'b1;
                        acc_intf_qreq_valid_o   = s_cc_intf_qreq_valid_i;
                        s_cc_intf_qreq_ready_o  = acc_intf_qreq_ready_i;

                        // Break ties with "biological" Snitch
                        cpu_intf_qreq_ready_o = 1'b0;
                        // Inform adopter that "biological" Snitch is not trying to offload 
                        s_cc_intf_qreq_valid_o = 1'b0;

                        // For now forward issue response to "biological" Snitch
                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;

                        /////////////////////////
                        //  FPU: Side Channel  //
                        /////////////////////////

                        acc_intf_fpu_rnd_mode_o = s_cc_intf_fpu_rnd_mode_i;
                        acc_intf_fpu_fmt_mode_o = s_cc_intf_fpu_fmt_mode_i;

                        // Does the "biological" Snitch try to offload an instruction?
                        if (cpu_intf_qreq_valid_i) begin

                          // Inform adopter that "biological" Snitch is trying to offload
                          s_cc_intf_qreq_valid_o = 1'b1;
                          // Notify adopter that Spatz is temporarily unready
                          s_cc_intf_qreq_ready_o = 1'b0;

                          // Re-establish ties with "biological" Snitch
                          acc_intf_qreq_o = cpu_intf_qreq_i;
                          // Tag instruction as coming form the "biological" Snitch
                          acc_intf_qreq_o.core_id = 1'b0;
                          acc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                          cpu_intf_qreq_ready_o = acc_intf_qreq_ready_i;

                          /////////////////////////
                          //  FPU: Side Channel  //
                          /////////////////////////

                          acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                          acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;

                        end

                        ////////////////////
                        //  CC<->CC Intf  //
                        ////////////////////

                        // Do not drive the master ports
                        m_cc_intf_qreq_o       = 'z;
                        m_cc_intf_qreq_valid_o = 1'bz;
                        m_cc_intf_prsp_ready_o = 1'bz;

                        ////////////////////////////////////////////////////////////////////////////
                        //                                                                        //
                        //   Default Master<->Slave relation  [Response]                          //
                        //                                                                        //
                        ////////////////////////////////////////////////////////////////////////////

                        s_cc_intf_prsp_valid_o = 1'b0;
                        acc_intf_prsp_ready_o  = 1'b0;
                        
                        // Break ties with the "biological" Snitch
                        cpu_intf_prsp_valid_o = 1'b0;
                        cpu_intf_prsp_o       = '0; 

                        if ((acc_intf_prsp_i.core_id == 1'b1) && acc_intf_prsp_valid_i) begin
                          // In merge mode we usually want to directly report to the adopter Snitch
                          s_cc_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                          acc_intf_prsp_ready_o  = s_cc_intf_prsp_ready_i;
                          
                          // Break ties with the "biological" Snitch
                          cpu_intf_prsp_valid_o = 1'b0;
                          cpu_intf_prsp_o       = '0;
                        end
                        
                        ////////////////////////////////////////////////////////////////////////////
                        //                                                                        //
                        //   Temporary re-establish "biological" relation  [Response]             //
                        //                                                                        //
                        ////////////////////////////////////////////////////////////////////////////

                        // If Spatz has a response to an offloaded instruction tagges as internal
                        // reconfigure the connection to report back to "biological" Snitch
                        else if ((acc_intf_prsp_i.core_id == 1'b0) && acc_intf_prsp_valid_i) begin

                          acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
                          cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                          cpu_intf_prsp_o       = acc_intf_prsp_i;
                          // Break ties with adopter
                          s_cc_intf_prsp_valid_o = 1'b0;
                          
                        end          
                      end                   
      MERGE_MASTER :  begin

                        ///////////////
                        //  Request  //
                        ///////////////

                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;
                        acc_intf_qreq_o = cpu_intf_qreq_i;
                        // Tag incoming instruction as coming from "biological" Snitch
                        acc_intf_qreq_o.core_id = 1'b0;
                        // Forward the instruction to the adopted Spatz
                        m_cc_intf_qreq_o = cpu_intf_qreq_i;
                        // Tag forwarded instruction as coming from adopter Snitch
                        m_cc_intf_qreq_o.core_id = 1'b1;

                        // Are both Spatzes ready for request and other Snitch does not want to re -
                        // gain control over Spatz?
                        is_ready = acc_intf_qreq_ready_i && m_cc_intf_qreq_ready_i && 
                                   !m_cc_intf_qreq_valid_i;

                        // Forward the valid request of Snitch to "biological" Spatz if we're ready
                        acc_intf_qreq_valid_o = is_ready ? cpu_intf_qreq_valid_i : 1'b0;

                        // Forward the valid request also to the adopted Spatz
                        m_cc_intf_qreq_valid_o = acc_intf_qreq_valid_o;

                        // Notify Snitch that both Spatz are ready for the request
                        cpu_intf_qreq_ready_o = is_ready;

                        /////////////////////////
                        //  FPU: Side Channel  //
                        /////////////////////////

                        // Internal
                        acc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        acc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;
                        // Forwarded
                        m_cc_intf_fpu_rnd_mode_o = cpu_intf_fpu_rnd_mode_i;
                        m_cc_intf_fpu_fmt_mode_o = cpu_intf_fpu_fmt_mode_i;

                        ////////////////
                        //  Response  //
                        ////////////////

                        // Take the response from the "biological" Snitch
                        cpu_intf_prsp_o = acc_intf_prsp_i;
                        // Wait for the "biological" and adopted Spatz to generate a valid response
                        // before assinign valid to adopting Snitch
                        cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i && m_cc_intf_prsp_valid_i;
                        // When both Spatz have a valid response forward the ready signal to both
                        // Spatzes
                        acc_intf_prsp_ready_o = cpu_intf_prsp_valid_o ? cpu_intf_prsp_ready_i : 
                                                                        1'b0;
                        m_cc_intf_prsp_ready_o = acc_intf_prsp_ready_o;
                        
                        ////////////////////
                        //  CC<->CC Intf  //
                        ////////////////////

                        // Do not drive the CC<->CC interface
                        s_cc_intf_qreq_ready_o = 1'b0;
                        s_cc_intf_prsp_valid_o = 1'b0;

                      end
    endcase

    // TODO: Fix bug in response for merge_slave and add initial values for merge
  end
endmodule : merge_interface