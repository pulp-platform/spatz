// Copyright 2025 ETH Zurich and University of Bologna.
// Copyright and related rights are licensed under the Solderpad Hardware
// License, Version 0.51 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License at
// http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// or agreed to in writing, software, hardware and materials distributed under
// this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

/// Spatz_cache_mux: 2-1 MUX for spatz cache side interconnection
/// selection are optimized for avoiding bank conflicts

// Author: Hong Pang <hopang@iis.ee.ethz.ch>
//         Diyou Shen <dishen@iis.ee.ethz.ch>

`include "common_cells/registers.svh"

module spatz_cache_mux #(
  parameter type DATA_T = logic,  // Vivado requires a default value for type parameters.
  parameter int  Offset = 0
) (
  input  logic            clk_i,
  input  logic            rst_ni,

  input  DATA_T [1:0]     inp_data_i,
  input  logic  [1:0]     inp_valid_i,
  output logic  [1:0]     inp_ready_o,

  output DATA_T           oup_data_o,
  output logic            oup_valid_o,
  input  logic            oup_ready_i
);

  // The bit we want to check for L1Cache Controller performance consideration
  logic [1:0] bank_bit, bit_check;
  // Tell mux which port shall we pass the data to process
  logic       sel_d, sel_q;
  // The bit we selected last time
  logic       last_bit_sel_d, last_bit_sel_q;
  // The port it has chosen to process last time
  logic       last_port_sel_d, last_port_sel_q;
  // last_bit_sel_q is only valid for 1 cycle.
  logic       last_bit_sel_valid_d, last_bit_sel_valid_q;
  // sel signal locked signal
  logic       sel_lock_d, sel_lock_q;

  for (genvar port = 0; port < 2; port++) begin
    // Extract from the address bit
    assign bank_bit[port]  = inp_data_i[port].addr[Offset];
    // If it is opposite from last selection?
    assign bit_check[port] = (bank_bit[port] != last_bit_sel_q);
  end

  // Last touched bank record is renewed only when the data is ready to be accepted by downstream
  `FFLARN(last_bit_sel_q, last_bit_sel_d, oup_ready_i, 1'b1, clk_i, rst_ni)
  `FFLARN(last_port_sel_q, last_port_sel_d, oup_ready_i, 1'b1, clk_i, rst_ni)
  `FF(sel_q, sel_d, 1'b1, clk_i, rst_ni)
  `FF(last_bit_sel_valid_q, last_bit_sel_valid_d, 1'b0, clk_i, rst_ni)
  `FF(sel_lock_q, sel_lock_d, '0, clk_i, rst_ni)

  // Lock the sel signal state when there is possibility to take new valid input 
  // during the downstream unready to process the already chosen valid data. (i.e. 
  // When a valid signal is taken when ready signal is low, it couldn't change anymore.)
  always_comb begin
    sel_lock_d = sel_lock_q;
    if ((|inp_valid_i) && (!oup_ready_i)) begin
      sel_lock_d = 1'b1;
    end
    if (oup_ready_i) begin
      sel_lock_d = 1'b0;
    end
  end

  always_comb begin
    sel_d = sel_q;
    last_bit_sel_valid_d = (|inp_valid_i) && oup_ready_i;
    if (!sel_lock_q) begin
      unique case (inp_valid_i)
        // No valid data
        2'b00: /*do nothing*/;
        // Only data 0 is valid => select data 0
        2'b01: sel_d = 1'b0;
        // Only data 1 is valid => select data 1
        2'b10: sel_d = 1'b1;
        // Both data are valid, avoid bank conflict first
        2'b11: begin
          // Check whether the bank that data goes conflicts with the mostly recent touched bank
          // (0: conflict, 1: unconflict) 
          if (last_bit_sel_valid_q) begin
            unique case (bit_check)
              // Both (not) conflict => Round robin
              2'b00, 2'b11: sel_d = ~last_port_sel_q;
              // Data 0 not conflicted => Choose data 0
              2'b01: sel_d = 1'b0;
              // Data 1 not conflicted => Choose data 1
              2'b10: sel_d = 1'b1;
              default: /*do nothing*/;
            endcase // bit_check
          end else begin
            sel_d = ~last_port_sel_q;
          end
        end
        default: /*do nothing*/;
      endcase // inp_valid_i
    end

    // Last touched bank record is updated only when there exists valid input data
    last_bit_sel_d = (|inp_valid_i) ? bank_bit[sel_d] : last_bit_sel_q;
    last_port_sel_d = (|inp_valid_i) ? sel_d : last_port_sel_q;
  end

  // Assign output
  always_comb begin
    inp_ready_o = '0;
    inp_ready_o[sel_d] = oup_ready_i;
  end
  assign oup_data_o   = inp_data_i[sel_d];
  assign oup_valid_o  = inp_valid_i[sel_d];
endmodule
