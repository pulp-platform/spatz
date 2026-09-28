// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Indexed (gather) extension for the Snitch DMA frontend.
//
// Sibling of axi_dma_twod_ext: instead of expanding a 2D descriptor with a fixed
// stride, it expands an *indexed* descriptor into a stream of 1D burst requests,
// where the per-transfer source offset is read from an index stream in L1/TCDM:
//
//     src_addr[i] = req.src + idx[i] * req.stride_src        (i = 0 .. G-1)
//     dst_addr[i] = req.dst + i      * req.stride_dst
//     len         = req.num_bytes
//     G           = req.num_repetitions   (index count)
//
// The index stream is fetched from req.idx_addr over a dedicated TCDM read port,
// TcdmDataWidth bits per read = N packed indices (N = TcdmDataWidth / elem_width).
// A spill_register decouples index fetch from consumption (1-deep prefetch); a
// per-word lane counter selects the index within the current word so we do NOT
// re-read a word per index.
//
// ADDRESS GENERATION: req.stride_src is constrained to be a power of two, so the
// source offset idx[i]*stride_src is computed with a shift (idx << log2(stride)),
// not a multiplier. log2(stride) is derived once per descriptor with a
// trailing-zero count (lzc) and registered.
//
// NOTE (stage 1): element widths 8b/16b are exercised; 32b/64b are wired but
// untested. All bursts of one gather carry the same req.id (single AXI id — the
// stage-1 plan; multi-id is a later experiment).

`include "common_cells/registers.svh"

module axi_dma_gather_ext #(
    parameter int unsigned ADDR_WIDTH     = -1,
    parameter int unsigned REQ_FIFO_DEPTH = -1,
    /// Width of one index-stream read from TCDM (default matches cluster: 64b).
    parameter int unsigned TcdmDataWidth  = 64,
    parameter type burst_req_t  = logic,
    /// Indexed transfer descriptor (the shared twod_req_t from the frontend).
    parameter type gather_req_t = logic
) (
    input  logic                       clk_i,
    input  logic                       rst_ni,
    /// 1D burst request stream to the iDMA backend
    output burst_req_t                 burst_req_o,
    output logic                       burst_req_valid_o,
    input  logic                       burst_req_ready_i,
    /// Indexed (gather) request (only descriptors with is_gather are routed here)
    input  gather_req_t                gather_req_i,
    input  logic                       gather_req_valid_i,
    output logic                       gather_req_ready_o,
    /// Asserted with the last burst of a gather (completion of the descriptor)
    output logic                       gather_req_last_o,
    /// Index-stream read port to L1/TCDM (read-only, req/gnt + valid response)
    output logic                       idx_req_o,
    output logic [ADDR_WIDTH-1:0]      idx_addr_o,
    input  logic                       idx_gnt_i,
    input  logic                       idx_rvalid_i,
    input  logic [TcdmDataWidth-1:0]   idx_rdata_i
);

    localparam int unsigned WordBytes     = TcdmDataWidth / 8;
    localparam int unsigned MaxNPerWord   = TcdmDataWidth / 8;      // 8-bit indices -> most lanes
    localparam int unsigned LaneCntW      = (MaxNPerWord > 1) ? $clog2(MaxNPerWord) : 1;
    localparam int unsigned NPerWordW     = $clog2(MaxNPerWord + 1); // holds n_per_word (1..MaxN)
    localparam int unsigned WordBitOffW   = $clog2(TcdmDataWidth);   // bit offset within a word
    localparam int unsigned StrideLog2W   = $clog2(ADDR_WIDTH);      // holds log2(stride) (0..AW-1)

    typedef logic [ADDR_WIDTH-1:0] addr_t;

    //--------------------------------------
    // Descriptor FIFO (mirror axi_dma_twod_ext)
    //--------------------------------------
    gather_req_t req;                // currently-processed descriptor (fifo head)
    logic req_fifo_full, req_fifo_empty, req_fifo_pop;

    fifo_v3 #(
        .DEPTH ( REQ_FIFO_DEPTH ),
        .dtype ( gather_req_t   )
    ) i_req_fifo (
        .clk_i, .rst_ni,
        .flush_i    ( 1'b0                                    ),
        .testmode_i ( 1'b0                                    ),
        .full_o     ( req_fifo_full                           ),
        .empty_o    ( req_fifo_empty                          ),
        .usage_o    ( /* unused */                            ),
        .data_i     ( gather_req_i                            ),
        .push_i     ( gather_req_valid_i & gather_req_ready_o ),
        .data_o     ( req                                     ),
        .pop_i      ( req_fifo_pop                            )
    );
    assign gather_req_ready_o = ~req_fifo_full;

    //--------------------------------------
    // Derived per-descriptor quantities
    //--------------------------------------
    // Number of indices packed in one fetched word, and the mask to isolate one.
    logic [NPerWordW-1:0]     n_per_word;
    logic [TcdmDataWidth-1:0] idx_mask;
    always_comb begin
        unique case (req.idx_width)
            2'b00 : begin n_per_word = NPerWordW'(TcdmDataWidth/8);  idx_mask = TcdmDataWidth'(8'hFF);        end // 8b
            2'b01 : begin n_per_word = NPerWordW'(TcdmDataWidth/16); idx_mask = TcdmDataWidth'(16'hFFFF);     end // 16b
            2'b10 : begin n_per_word = NPerWordW'(TcdmDataWidth/32); idx_mask = TcdmDataWidth'(32'hFFFFFFFF); end // 32b
            default: begin n_per_word = NPerWordW'(TcdmDataWidth/64); idx_mask = '1;                          end // 64b
        endcase
    end

    // log2(stride_src): trailing-zero count of the (power-of-two) source stride.
    // Sampled once per descriptor at load; used as the idx->offset shift amount.
    logic [StrideLog2W-1:0] stride_src_log2_comb;
    logic                   stride_src_log2_empty;   // stride==0 (unused)
    lzc #(
        .WIDTH ( ADDR_WIDTH ),
        .MODE  ( 1'b0       )                         // 0 = count trailing zeros
    ) i_stride_src_lzc (
        .in_i    ( req.stride_src        ),
        .cnt_o   ( stride_src_log2_comb  ),
        .empty_o ( stride_src_log2_empty )
    );

    //--------------------------------------
    // State
    //--------------------------------------
    typedef enum logic {GIDLE, GRUN} state_e;
    state_e state_q, state_d;

    addr_t                  g_q,   g_d;            // indices consumed so far (0 .. G)
    logic [LaneCntW-1:0]    lane_q, lane_d;        // index lane within current word (0 .. n_per_word-1)
    addr_t                  dst_addr_q, dst_addr_d;// running destination pointer
    addr_t                  idx_ptr_q,  idx_ptr_d; // next index-word address to fetch
    addr_t                  fetched_q,  fetched_d; // #indices fetched so far (= words*n_per_word)
    logic                   fetch_outst_q, fetch_outst_d; // one index read in flight
    logic [StrideLog2W-1:0] stride_log2_q, stride_log2_d; // registered log2(stride_src)

    //--------------------------------------
    // Index-word spill register (1-deep prefetch)
    //--------------------------------------
    logic [TcdmDataWidth-1:0] idx_word;
    logic spill_valid, spill_ready, spill_pop;

    spill_register #(
        .T ( logic [TcdmDataWidth-1:0] )
    ) i_idx_spill (
        .clk_i, .rst_ni,
        .valid_i ( idx_rvalid_i ),   // TCDM response cannot be back-pressured;
        .ready_o ( spill_ready  ),   //   we only issue a read when there is room
        .data_i  ( idx_rdata_i  ),
        .valid_o ( spill_valid  ),
        .ready_i ( spill_pop    ),
        .data_o  ( idx_word     )
    );

    //--------------------------------------
    // Index extraction + address generation
    //--------------------------------------
    // Bit position of index[lane] within the fetched word:
    //   elem_bits = 8 << idx_width  =>  offset = lane * elem_bits = lane << (3 + idx_width)
    logic [WordBitOffW-1:0] idx_bit_offset;
    addr_t idx_val, src_addr;

    always_comb begin
        idx_bit_offset = WordBitOffW'(lane_q) << (3 + req.idx_width);
        idx_val        = addr_t'((idx_word >> idx_bit_offset) & idx_mask);
        // src_addr = base + idx*stride_src, stride_src a power of two => shift, no multiplier
        src_addr       = req.src + (idx_val << stride_log2_q);
    end

    //--------------------------------------
    // Burst generation (consume side)
    //--------------------------------------
    logic consume_accept, lane_wrap, last_index;

    assign burst_req_valid_o = (state_q == GRUN) & spill_valid & (g_q < req.num_repetitions);
    assign consume_accept    = burst_req_valid_o & burst_req_ready_i;
    assign lane_wrap         = (lane_q == LaneCntW'(n_per_word - NPerWordW'(1)));
    assign last_index        = (g_q == (req.num_repetitions - 1));

    always_comb begin
        burst_req_o             = '0;
        burst_req_o.id          = req.id;
        burst_req_o.src         = src_addr;
        burst_req_o.dst         = dst_addr_q;
        burst_req_o.num_bytes   = req.num_bytes;
        burst_req_o.cache_src   = req.cache_src;
        burst_req_o.cache_dst   = req.cache_dst;
        burst_req_o.burst_src   = req.burst_src;
        burst_req_o.burst_dst   = req.burst_dst;
        burst_req_o.decouple_rw = req.decouple_rw;
        burst_req_o.deburst     = req.deburst;
    end

    //--------------------------------------
    // Index fetch (request side)
    //--------------------------------------
    logic fetch_en;
    // fetch while: running, more indices needed, no read in flight, room in spill
    assign fetch_en   = (state_q == GRUN)
                      & (fetched_q < req.num_repetitions)
                      & ~fetch_outst_q
                      & spill_ready;
    assign idx_req_o  = fetch_en;
    assign idx_addr_o = idx_ptr_q;

    //--------------------------------------
    // Control / next-state
    //--------------------------------------
    always_comb begin
        // defaults: hold
        state_d           = state_q;
        g_d               = g_q;
        lane_d            = lane_q;
        dst_addr_d        = dst_addr_q;
        idx_ptr_d         = idx_ptr_q;
        fetched_d         = fetched_q;
        fetch_outst_d     = fetch_outst_q;
        stride_log2_d     = stride_log2_q;
        req_fifo_pop      = 1'b0;
        spill_pop         = 1'b0;
        gather_req_last_o = 1'b0;

        // --- fetch handshakes (independent of consume) ---
        if (idx_req_o & idx_gnt_i) begin
            idx_ptr_d     = idx_ptr_q + WordBytes;
            fetched_d     = fetched_q + addr_t'(n_per_word);
            fetch_outst_d = 1'b1;
        end
        if (idx_rvalid_i) begin
            fetch_outst_d = 1'b0;    // response captured into the spill register
        end

        unique case (state_q)
            //--------------------------------------------------------------
            GIDLE : begin
                if (~req_fifo_empty) begin
                    if (req.is_gather && req.num_repetitions != '0) begin
                        // load a fresh gather descriptor
                        g_d           = '0;
                        lane_d        = '0;
                        dst_addr_d    = req.dst;
                        idx_ptr_d     = req.idx_addr;
                        fetched_d     = '0;
                        fetch_outst_d = 1'b0;
                        stride_log2_d = stride_src_log2_comb;
                        state_d       = GRUN;
                    end else begin
                        // not a gather (shouldn't be routed here) or empty -> drop it
                        req_fifo_pop  = 1'b1;
                    end
                end
            end
            //--------------------------------------------------------------
            GRUN : begin
                if (consume_accept) begin
                    g_d        = g_q + 1;
                    dst_addr_d = dst_addr_q + req.stride_dst;
                    // advance the lane; pop the word when it is exhausted or on the
                    // final index (flush the residual partial word)
                    if (lane_wrap || last_index) begin
                        lane_d    = '0;
                        spill_pop = 1'b1;
                    end else begin
                        lane_d = lane_q + 1;
                    end
                    // whole gather done
                    if (last_index) begin
                        gather_req_last_o = 1'b1;
                        req_fifo_pop      = 1'b1;
                        state_d           = GIDLE;
                    end
                end
            end
            default: ;
        endcase
    end

    //--------------------------------------
    // Registers
    //--------------------------------------
    `FF(state_q,       state_d,       GIDLE)
    `FF(g_q,           g_d,           '0)
    `FF(lane_q,        lane_d,        '0)
    `FF(dst_addr_q,    dst_addr_d,    '0)
    `FF(idx_ptr_q,     idx_ptr_d,     '0)
    `FF(fetched_q,     fetched_d,     '0)
    `FF(fetch_outst_q, fetch_outst_d, 1'b0)
    `FF(stride_log2_q, stride_log2_d, '0)

endmodule
