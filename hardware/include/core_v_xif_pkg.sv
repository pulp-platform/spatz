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

package core_v_xif_pkg;

  // ADJUST THE PARAMETERS ACCORDING TO YOUR IMPLEMENTATION
  localparam X_DATAWIDTH = 32;
  localparam X_NUM_RS    = 2;
  localparam X_NUM_FRS   = 0;
  localparam X_ID_WIDTH  = 5;
  localparam X_MEM_WIDTH = 32;
  localparam X_RFR_WIDTH = 32;
  localparam X_RFW_WIDTH = 32;
  localparam X_MISA      = 32'h0000_0000;
  localparam FLEN        = 32;
  localparam XLEN        = 32;

  // DO NOT CHANGE THE STRUCTS
  typedef struct packed {
    logic [          15:0] instr;
    logic [           1:0] mode;
    logic [X_ID_WIDTH-1:0] id;
  } x_compressed_req_t;

  typedef struct packed {
    logic [31:0] instr;
    logic        accept;
  } x_compressed_resp_t;

  typedef struct packed {
    logic [31:0] instr;
    logic [ 1:0] mode;
    logic [X_ID_WIDTH-1:0] id;
    logic [X_NUM_RS-1:0][X_RFR_WIDTH-1:0] rs;
    logic [X_NUM_RS-1:0] rs_valid;
    logic [X_NUM_FRS-1:0][FLEN-1:0] frs;
    logic [X_NUM_FRS-1:0] frs_valid;
  } x_issue_req_t;

  typedef struct packed {
    logic accept;
    logic writeback;
    logic float;
    logic dualwrite;
    logic dualread;
    logic loadstore;
    logic exc;
  } x_issue_resp_t;

  typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic commit_kill;
  } x_commit_t;

  typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic [31:0] addr;
    logic [1:0] mode;
    logic [1:0] size;
    logic we;
    logic [X_MEM_WIDTH-1:0] wdata;
    logic last;
    logic spec;
  } x_mem_req_t;

  typedef struct packed {
    logic exc;
    logic [5:0] exccode;
  } x_mem_resp_t;

    typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic [X_MEM_WIDTH-1:0] rdata;
    logic err;
  } x_mem_result_t;

  typedef struct packed {
    logic [X_ID_WIDTH-1:0] id;
    logic [X_RFW_WIDTH-1:0] data;
    logic [4:0] rd;
    logic [X_RFW_WIDTH-XLEN:0] we;
    logic float;
    logic exc;
    logic [5:0] exccode;
  } x_result_t;
endpackage
