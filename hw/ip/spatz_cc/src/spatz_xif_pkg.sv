// Copyright 2021 ETH Zurich and University of Bologna.
// Copyright and related rights are licensed under the Solderpad Hardware
// License, Version 0.51 (the "License"); you may not use this file except in
// compliance with the License.  You may obtain a copy of the License at
// http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// or agreed to in writing, software, hardware and materials distributed under
// this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// CORE-V-XIF Package
// Contributor: Moritz Imfeld <moimfeld@student.ethz.ch>
//
// For more details see: https://docs.openhwgroup.org/projects/openhw-group-core-v-xif/index.html
//                   or: https://github.com/openhwgroup/core-v-xif
//

/* verilator lint_off SYMRSVDWORD */
package spatz_xif_pkg;

  // ADJUST THE PARAMETERS ACCORDING TO YOUR IMPLEMENTATION
  localparam X_NUM_RS    = 3;
  localparam X_NUM_FRS   = 1;
  localparam X_ID_WIDTH  = 6;
  localparam X_MEM_WIDTH = 32;
  localparam X_RFR_WIDTH = 64;
  localparam X_RFW_WIDTH = 64;
  localparam X_MISA      = 32'h0000_0000;

  // DO NOT CHANGE THE STRUCTS
  typedef struct packed {
    logic [          15:0] instr;
    logic [           1:0] mode;
    logic [X_ID_WIDTH-1:0] id;
  } spatz_x_compressed_req_t;

  typedef struct packed {
    logic [31:0] instr;
    logic        accept;
  } spatz_x_compressed_resp_t;

  typedef struct packed {
    logic [31:0] instr;
    logic [ 1:0] mode;
    logic [X_ID_WIDTH-1:0] id;
    logic [X_NUM_RS-1:0][X_RFR_WIDTH-1:0] rs;
    logic [X_NUM_RS-1:0] rs_valid;
  } spatz_x_issue_req_t;

  typedef struct packed {
    logic accept;
    logic writeback;
    logic float;
    logic dualwrite;
    logic dualread;
    logic loadstore;
    logic exc;
  } spatz_x_issue_resp_t;

  typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic commit_kill;
  } spatz_x_commit_t;

  typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic [X_RFW_WIDTH-1:0] data;
    logic we;
    logic float;
    logic exc;
    logic [5:0] exccode;
  } spatz_x_result_t;

endpackage
/* verilator lint_on SYMRSVDWORD */
