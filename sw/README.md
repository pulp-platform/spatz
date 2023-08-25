# Spatz Software

This subdirectory contains the various bits and pieces of software for the Spatz ecosystem.

## Contents

### Libraries

- `cmake`: Bits and pieces for integration with the CMake build system.
- `riscvTests`: Contains small RISC-V tests, meant to test the several instructions supported by Snitch and Spatz.
- `snRuntime`: The fundamental, bare-metal runtime for Snitch systems. Exposes a minimal API to manage execution of code across the available cores and clusters, query information about a thread's context, and to coordinate and exchange data with other threads. This is heavily based on Snitch's own [runtime](https://github.com/pulp-platform/snitch/tree/master/sw/snRuntime), with some adaptations for Spatz.

### Tests

- `spatzBenchmarks`: Benchmarking executables that evaluate the performance characteristics of Spatz. Most binaries should work on any Spatz-based configuration, although some are configuration-specific (e.g., the `dp-fft` kernel only works with two cores).

### Third-Party

The `toolchain` directory contains third-party tools that we inline into this repository for ease of use.

- `toolchain/llvm-project`: A patched LLVM 14 installation with support for RISC-V's Vector Extension and Spatz-specific instructions (e.g., SDOTP).
- `toolchain/riscv-gnu-toolchain`: A patched GCC installation with support for RISC-V's Vector Extension. We use this mainly for libstdc++. This repository compiles all binaries with Clang, and does not support a GCC-based compilation flow.
- `toolchain/riscv-isa-sim`: A Spike installation, used for disassembling the core traces.
- `toolchain/riscv-opcodes`: Utilities to manage instruction encodings and generate functions and data structurse for parsing and representation in various languages.
- `toolchain/verilator`: An updated Verilator installation, with which we guarantee the verilation of Spatz-based systems.
