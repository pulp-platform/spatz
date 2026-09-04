/*
 *  Copyright 2023 Commissariat a l'Energie Atomique et aux Energies Alternatives (CEA)
 *
 *  SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
 */
/*
 *  Description: tc_sram_impl-backed override of cv-hpdcache's
 *  hpdcache_sram_1rw (rtl/src/common/macros/behav/hpdcache_sram_1rw.sv).
 *  Same module name and port list as the original -- this file is placed
 *  later in the vlog compile order (see Bender.yml), so it silently
 *  replaces the original definition in the compiled library instead of
 *  requiring any parameter threading through hpdcache_sram.sv,
 *  hpdcache_memctrl.sv, hpdcache_ctrl.sv, hpdcache.sv,
 *  hpd_spatz_cache_ctrl.sv, and spatz_cluster_hpd.sv. Uses tc_sram_impl's
 *  default ImplKey="none" (behavioral fallback in sim, same as InSitu's
 *  own banks) -- real macro binding is a PD-side concern, not wired here.
 */
module hpdcache_sram_1rw
#(
    parameter int unsigned ADDR_SIZE = 0,
    parameter int unsigned DATA_SIZE = 0,
    parameter int unsigned DEPTH = 2**ADDR_SIZE,
    parameter int unsigned NDATA = 1
)
(
    input  logic                            clk,
    input  logic                            rst_n,
    input  logic                            cs,
    input  logic                            we,
    input  logic [ADDR_SIZE-1:0]            addr,
    input  logic [NDATA-1:0][DATA_SIZE-1:0] wdata,
    output logic [NDATA-1:0][DATA_SIZE-1:0] rdata
);

    tc_sram_impl #(
        .NumWords  (DEPTH          ),
        .DataWidth (NDATA*DATA_SIZE),
        .ByteWidth (NDATA*DATA_SIZE), // no byte enable -- one "byte" = the whole word
        .NumPorts  (1              ),
        .Latency   (1              ),
        .SimInit   ("zeros"        )
    ) i_sram (
        .clk_i  (clk  ),
        .rst_ni (rst_n),
        .impl_i ('0   ),
        .impl_o (     ),
        .req_i  (cs   ),
        .we_i   (we   ),
        .addr_i (addr ),
        .wdata_i(wdata),
        .be_i   (1'b1 ),
        .rdata_o(rdata)
    );
endmodule
// vim: ts=4 : sts=4 : sw=4 : et : tw=100 : spell : spelllang=en
