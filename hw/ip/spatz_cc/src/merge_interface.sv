module merge_interface 
  import spatz_pkg::*; #(
    parameter type acc_issue_req_t          = logic,
    parameter type acc_issue_rsp_t          = logic,
    parameter type acc_rsp_t                = logic
  ) (
  	input logic clk_i, 
  	input logic rst_ni,
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
  	output acc_rsp_t s_cc_intf_prsp_o,
  	output logic s_cc_intf_prsp_valid_o,
  	input  logic s_cc_intf_prsp_ready_i,
    // Request interface between cc-controller and cc-controller
    output acc_issue_req_t m_cc_intf_qreq_o,
    output logic m_cc_intf_qreq_valid_o,
    input  logic m_cc_intf_qreq_ready_i,
    // Response interface between cc-controller and cc-controller
    input  acc_rsp_t m_cc_intf_prsp_i,
    input  logic m_cc_intf_prsp_valid_i,
    output logic m_cc_intf_prsp_ready_o,
  	// Signal if merge mode is active and if controller has to act as slave or master device
    input merge_intf_e state_i,
  	input merge_mode_t merge_mode_i
  );

  // TODO: Add a controller or predecoder to this logic which sets the branching logic 


  // Combinational logic for request interface
  always_comb begin
    
    case (state_i)
      // Standard non-merge mode configuration
      NON_MERGE    :  begin
                        // Connect the request-interface
                        acc_intf_qreq_o       = cpu_intf_qreq_i;
                        acc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
                        cpu_intf_qreq_ready_o = acc_intf_qreq_ready_i;
                        // Do not drive the CC-Interface
                        m_cc_intf_qreq_o       = '0;
                        m_cc_intf_qreq_valid_o = 1'b0;
                        s_cc_intf_qreq_ready_o = 1'b0;

                        // Connect the response-interface
                        cpu_intf_prsp_o       = acc_intf_prsp_i;
                        cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
                        acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
                        cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;
                        // Do not drive the CC-interface
                        s_cc_intf_prsp_o       = '0;
                        s_cc_intf_prsp_valid_o = 1'b0;
                      end
    endcase
    
    /*
    // Re-configure if in merge mode and slave
    if (merge_mode_i.is_merge && !merge_mode_i.is_master) begin
      acc_intf_qreq_o       = s_cc_intf_qreq_i;
      acc_intf_qreq_valid_o = s_cc_intf_qreq_valid_i;
      cpu_intf_qreq_ready_o = 0;
    end else if (merge_mode_i.is_merge && merge_mode_i.is_masater) begin
      m_cc_intf_qreq_o       = cpu_intf_qreq_i;
      m_cc_intf_qreq_valid_o = cpu_intf_qreq_valid_i;
    end
    
  end

  // Combinational logic for response interface
  always_comb begin
    // Standard non-merge mode configuration
    cpu_intf_prsp_o       = acc_intf_prsp_i;
    cpu_intf_prsp_valid_o = acc_intf_prsp_valid_i;
    acc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
    cpu_intf_issue_prsp_o = acc_intf_issue_prsp_i;
    // Do not drive the CC-interface
    s_cc_intf_prsp_o       = '0;
    s_cc_intf_prsp_valid_o = 1'b0;
    // Re-configure if in merge mode and slave
    if (merge_mode_i.is_merge && !merge_mode_i.is_master) begin
      cpu_intf_prsp_o       = '0;
      cpu_intf_prsp_valid_o = '0;
      acc_intf_prsp_ready_o = s_cc_intf_prsp_ready_i;

      s_cc_intf_prsp_o       = acc_intf_prsp_i;
      s_cc_intf_prsp_valid_o = acc_intf_prsp_valid_i;   
    end else if (merge_mode_i.is_merge && merge_mode_i.is_master) begin
      m_cc_intf_prsp_ready_o = cpu_intf_prsp_ready_i;
    end
    */

  end





endmodule : merge_interface