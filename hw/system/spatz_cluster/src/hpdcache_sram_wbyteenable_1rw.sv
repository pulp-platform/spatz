/*
 *  Copyright 2023 Commissariat a l'Energie Atomique et aux Energies Alternatives (CEA)
 *
 *  SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
 */
/*
 *  Description: tc_sram_impl-backed override of cv-hpdcache's
 *  hpdcache_sram_wbyteenable_1rw (rtl/src/common/macros/behav/
 *  hpdcache_sram_wbyteenable_1rw.sv). Same module name and port list as
 *  the original -- see hpdcache_sram_1rw.sv in this directory for why a
 *  same-name override is used instead of parameter threading. Uses
 *  tc_sram_impl's default ImplKey="none" (behavioral fallback in sim) --
 *  real macro binding is a PD-side concern, not wired here.
 */
module hpdcache_sram_wbyteenable_1rw
#(
    parameter int unsigned ADDR_SIZE = 0,
    parameter int unsigned DATA_SIZE = 0,
    parameter int unsigned DEPTH = 2**ADDR_SIZE,
    parameter int unsigned NDATA = 1
)
(
    input  logic                              clk,
    input  logic                              rst_n,
    input  logic                              cs,
    input  logic                              we,
    input  logic [ADDR_SIZE-1:0]              addr,
    input  logic [NDATA-1:0][DATA_SIZE-1:0]   wdata,
    input  logic [NDATA-1:0][DATA_SIZE/8-1:0] wbyteenable,
    output logic [NDATA-1:0][DATA_SIZE-1:0]   rdata
);

    tc_sram_impl #(
        .NumWords  (DEPTH          ),
        .DataWidth (NDATA*DATA_SIZE),
        .ByteWidth (8              ),
        .NumPorts  (1              ),
        .Latency   (1              ),
        .SimInit   ("zeros"        )
    ) i_sram (
        .clk_i  (clk        ),
        .rst_ni (rst_n      ),
        .impl_i ('0         ),
        .impl_o (           ),
        .req_i  (cs         ),
        .we_i   (we         ),
        .addr_i (addr       ),
        .wdata_i(wdata      ),
        .be_i   (wbyteenable),
        .rdata_o(rdata      )
    );
endmodule
// vim: ts=4 : sts=4 : sw=4 : et : tw=100 : spell : spelllang=en
