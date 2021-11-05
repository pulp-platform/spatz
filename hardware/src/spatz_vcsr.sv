// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Domenic Wüthrich, ETH Zurich

module spatz_vcsr import spatz_pkg::*; import rvv_pkg::*; (
	input	 logic 		clk_i,
	input  logic 		rst_ni,
  // CSR request
  input  spatz_req_t vcsr_req_i,
  // CSR registers
	output vtype_t 	vtype_o,
	output vlen_t 	vl_o,
	output vlen_t 	vstart_o
);

	// Include FF
	`include "common_cells/registers.svh"

	// CSR registers
  vlen_t  vstart_d, vstart_q;
  vlen_t  vl_d, vl_q;
  vtype_t vtype_d, vtype_q;

  `FF(vstart_q, vstart_d, '0)
  `FF(vl_q, vl_d, '0)
  `FF(vtype_q, vtype_d, '{vill: 1'b1, default: '0})

  always_comb begin : proc_vcsr
    vstart_d = vstart_q;
    vl_d     = vl_q;
    vtype_d  = vtype_q;

    // Reset vstart to zero
    if (vcsr_req_i.op_cgf.reset_vstart) begin
      vstart_d = '0;
    end

    // Set new vstart when written over vcsrs
    if (vcsr_req_i.op == VCSR) begin
      if (vcsr_req_i.op_cgf.write_vstart) begin
        vstart_d = vlen_t'(vcsr_req_i.rs1);
      end else if (vcsr_req_i.op_cgf.set_vstart) begin
        vstart_d = vstart_q | vlen_t'(vcsr_req_i.rs1);
      end else if (vcsr_req_i.op_cgf.clear_vstart) begin
        vstart_d = vstart_q & ~vlen_t'(vcsr_req_i.rs1);
      end
    end

    if (vcsr_req_i.op == VCFG) begin
      // Check if vtype is valid
      if ((vtype_d.vsew > EW_32) || (vtype_d.vlmul == LMUL_RES) || (vtype_d.vlmul + $clog2(ELEN) - 1 < vtype_d.vsew)) begin
        // Invalid
        vtype_d = '{vill: 1'b1, default: '0};
        vl_d    = '0;
      end else begin
        // Valid

        // Set new vtype
        vtype_d = vcsr_req_i.vtype;
        if (~vcsr_req_i.op_cgf.keep_vl) begin
          // Normal stripmining mode or set to MAXVL
          automatic int unsigned vlmax = 0;
          vlmax = VLENB >> vcsr_req_i.vtype.vsew;

          unique case (vcsr_req_i.vtype.vlmul)
            LMUL_F2: vlmax >>= 1;
            LMUL_F4: vlmax >>= 2;
            LMUL_F8: vlmax >>= 3;
            LMUL_1: vlmax <<= 0;
            LMUL_2: vlmax <<= 1;
            LMUL_4: vlmax <<= 2;
            LMUL_8: vlmax <<= 3;
          endcase

          vl_d = (vcsr_req_i.rs1 == '1) ? MAXVL : (vlmax < vcsr_req_i.rs1) ? vlmax : vcsr_req_i.rs1;
        end else begin
          // Keep vl mode

          // If new vtype changes ration, mark as illegal
          if (($signed(vcsr_req_i.vtype.vsew) - $signed(vtype_q.vsew)) != ($signed(vcsr_req_i.vtype.vlmul) - $signed(vtype_q.vlmul))) begin
            vtype_d = '{vill: 1'b1, default: '0};
            vl_d    = '0;
          end
        end
      end
    end
  end

  // Assign outputs
  assign vtype_o 	= vtype_q;
  assign vl_o 		= vl_q;
  assign vstart_o = vstart_q;

endmodule : spatz_vcsr