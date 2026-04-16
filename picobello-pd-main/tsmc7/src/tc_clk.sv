// Copyright (c) 2025 ETH Zurich.
// Tim Fischer <fischeti@iis.ee.ethz.ch>

module tc_clk_and2 (
  input  logic clk0_i,
  input  logic clk1_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKAN2D1BWP240H11P57PDSVT i_clk_and2 (
    .A1(clk0_i),
    .A2(clk1_i),
    .Z(clk_o)
  );

endmodule

module tc_clk_buffer (
  input  logic clk_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKBD4BWP240H11P57PDSVT i_clk_buffer (
    .I(clk_i),
    .Z(clk_o)
  );

endmodule

module tc_clk_gating #(
  parameter bit IS_FUNCTIONAL = 1'b1
)(
   input  logic clk_i,
   input  logic en_i,
   input  logic test_en_i,
   output logic clk_o
);

  (* tc_clk_size_only *)
  CKLNQD4BWP240H11P57PDSVT i_clk_gating (
    .CP(clk_i),
    .E(en_i),
    .TE(test_en_i),
    .Q(clk_o)
  );

endmodule

module tc_clk_inverter (
  input  logic clk_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKND4BWP240H11P57PDSVT i_clk_inverter (
    .I(clk_i),
    .ZN(clk_o)
  );

endmodule

module tc_clk_mux2 (
  input  logic clk0_i,
  input  logic clk1_i,
  input  logic clk_sel_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKMUX2D4BWP240H11P57PDSVT i_clk_mux2 (
    .S(clk_sel_i),
    .I0(clk0_i),
    .I1(clk1_i),
    .Z(clk_o)
  );

endmodule

module tc_clk_xor2 (
  input  logic clk0_i,
  input  logic clk1_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKXOR2D4BWP240H11P57PDSVT i_clk_xor2 (
    .A1(clk0_i),
    .A2(clk1_i),
    .Z(clk_o)
  );

endmodule

module tc_clk_or2 (
  input logic clk0_i,
  input logic clk1_i,
  output logic clk_o
);

  (* tc_clk_size_only *)
  CKOR2D4BWP240H11P57PDSVT i_clk_or2 (
    .A1(clk0_i),
    .A2(clk1_i),
    .Z(clk_o)
  );

endmodule
