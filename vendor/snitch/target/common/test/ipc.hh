// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Paul Scheffler <paulsc@iis.ee.ethz.ch>

#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>

class IpcIface {
   private:
    static const int IPC_BUF_SIZE = 4096;
    static const int IPC_BUF_SIZE_STRB = IPC_BUF_SIZE / 8 + 1;
    static const int IPC_ERR_DOUBLE_ARG = 30;
    static const long IPC_POLL_PERIOD_NS = 100000L;

    // Possible IPC operations
    enum ipc_opcode_e {
        Read = 0,
        Write = 1,
        Poll = 2,
    };

    // Operations are 3 doubles, followed by data streams in either direction
    typedef struct {
        uint64_t opcode;
        uint64_t addr;
        uint64_t len;
    } ipc_op_t;

    // Args passed to IPC thread
    typedef struct {
        char* tx;
        char* rx;
    } ipc_targs_t;

    // Thread to asynchronously handle FIFOs
    ipc_targs_t targs;
    pthread_t thread;
    bool active;

    static void* ipc_thread_handle(void* in);

   public:
    IpcIface(int argc, char** argv);
    ~IpcIface();
};
