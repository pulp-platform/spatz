// Copyright 2026 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Compile-time selection of the instruction-trace sink.
//
//   (neither define)       $fopen/$fwrite/$fclose into a plain .dasm file.
//   SNITCH_TRACE_GZ        DPI-C writer (hw/ip/snitch_test/src/trace_dpi.c)
//                          producing a gzip stream, .dasm.gz. The default
//                          backend is "thr:gz6": a worker thread deflates so
//                          the simulator thread only memcpys into a block.
//                          Override at run time with +trace_mode=<mode>.
//   SNITCH_TRACE_DISABLE   no tracer instantiated at all; takes precedence.
//
// The macros keep both cores' tracers on one code path. A core declares the
// DPI imports once with `SNITCH_TRACE_DECLS, then uses OPEN/WRITE/CLOSE.
// OPEN builds the file name itself so the extension can follow the backend.

`ifndef SNITCH_TRACE_WRITER_SVH_
`define SNITCH_TRACE_WRITER_SVH_

`ifdef SNITCH_TRACE_GZ

`define SNITCH_TRACE_DECLS \
  import "DPI-C" function int    tw_open (input string path, input string mode); \
  import "DPI-C" function void   tw_write(input int h, input string s); \
  import "DPI-C" function void   tw_close(input int h); \
  import "DPI-C" function string tw_ext  (input string mode); \
  string snitch_trace_mode;

// `fmt` is the file name up to the extension, with one format specifier for
// `id`. It is passed to $sformat on its own and the suffix is concatenated
// afterwards: Verilator only substitutes a format string that is a literal at
// the call site, and silently emits it verbatim if it is an expression.
// The suffix comes from the backend actually selected, so a run with
// +trace_mode=plain writes a .dasm rather than an uncompressed .dasm.gz that
// `make traces` would then fail to decompress.
//
// tw_open returns -1 on failure; tw_write/tw_close ignore a negative handle,
// so a failed open degrades to dropping the trace rather than killing the sim.
`define SNITCH_TRACE_OPEN(handle, fn, fmt, id) \
  if (!$value$plusargs("trace_mode=%s", snitch_trace_mode)) \
    snitch_trace_mode = "thr:gz6"; \
  $sformat(fn, fmt, id); \
  fn = {fn, tw_ext(snitch_trace_mode)}; \
  handle = tw_open(fn, snitch_trace_mode)

`define SNITCH_TRACE_WRITE(handle, str) tw_write(handle, str)
`define SNITCH_TRACE_CLOSE(handle)      tw_close(handle)

`else // plain

`define SNITCH_TRACE_DECLS
`define SNITCH_TRACE_OPEN(handle, fn, fmt, id) \
  $sformat(fn, fmt, id); \
  fn = {fn, "dasm"}; \
  handle = $fopen(fn, "w")
`define SNITCH_TRACE_WRITE(handle, str) $fwrite(handle, str)
`define SNITCH_TRACE_CLOSE(handle)      $fclose(handle)

`endif

`endif // SNITCH_TRACE_WRITER_SVH_
