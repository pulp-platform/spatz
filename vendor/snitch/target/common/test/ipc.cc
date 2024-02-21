// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Paul Scheffler <paulsc@iis.ee.ethz.ch>

#include "ipc.hh"
#include "tb_lib.hh"

void* IpcIface::ipc_thread_handle(void* in) {
    ipc_targs_t* targs = (ipc_targs_t*)in;
    // Open FIFOs
    FILE* tx = fopen(targs->tx, "rb");
    FILE* rx = fopen(targs->rx, "wb");
    // Prepare data and full-strobe array
    uint8_t buf_data[IPC_BUF_SIZE];
    uint8_t buf_strb[IPC_BUF_SIZE_STRB];
    std::fill_n(buf_strb, IPC_BUF_SIZE_STRB, 0xFF);
    // Handle commands
    ipc_op_t op;

    while (1) {
        if (!fread(&op, sizeof(ipc_op_t), 1, tx)) {
            if (feof(tx)) {
                printf(
                    "[IPC] All messages read. Closing FIFOs and joining main "
                    "thread.\n");
                break;
            }
        } else {
            switch (op.opcode) {
                case Read:
                    // Read full blocks until one full block or less left
                    printf("[IPC] Read from 0x%x len 0x%x ...\n", op.addr,
                           op.len);
                    for (uint64_t i = op.len; i > IPC_BUF_SIZE;
                         i -= IPC_BUF_SIZE) {
                        sim::MEM.read(op.addr, IPC_BUF_SIZE, buf_data);
                        fwrite(buf_data, IPC_BUF_SIZE, 1, rx);
                        op.addr += IPC_BUF_SIZE;
                        op.len -= IPC_BUF_SIZE;
                    }
                    sim::MEM.read(op.addr, op.len, buf_data);
                    fwrite(buf_data, op.len, 1, rx);
                    fflush(rx);
                    break;
                case Write:
                    // Write full blocks until one full block or less left
                    printf("[IPC] Write to 0x%x len %d ...\n", op.addr, op.len);
                    for (uint64_t i = op.len; i > IPC_BUF_SIZE;
                         i -= IPC_BUF_SIZE) {
                        fread(buf_data, IPC_BUF_SIZE, 1, tx);
                        sim::MEM.write(op.addr, IPC_BUF_SIZE, buf_data,
                                       buf_strb);
                        op.addr += IPC_BUF_SIZE;
                        op.len -= IPC_BUF_SIZE;
                    }
                    fread(buf_data, op.len, 1, tx);
                    sim::MEM.write(op.addr, op.len, buf_data, buf_strb);
                    break;
                case Poll:
                    // Unpack 32b checking mask and expected value from length
                    uint32_t mask = op.len & 0xFFFFFFFF;
                    uint32_t expected = (op.len >> 32) & 0xFFFFFFFF;
                    printf("[IPC] Poll on 0x%x mask 0x%x expected 0x%x ...\n",
                           op.addr, mask, expected);
                    uint32_t read;
                    do {
                        sim::MEM.read(op.addr, sizeof(uint32_t),
                                      (uint8_t*)(void*)&read);
                        nanosleep(
                            (const struct timespec[]){{0, IPC_POLL_PERIOD_NS}},
                            NULL);
                    } while ((read & mask) == (expected & mask));
                    // Send back read 32b word
                    fwrite(&read, sizeof(uint32_t), 1, rx);
                    fflush(rx);
                    break;
            }
        }
    }

    // TX FIFO closed at other end: close both FIFOs and join main thread
    fclose(tx);
    fclose(rx);
    pthread_exit(NULL);
}

// Conditionally construct IPC iff any arguments specify it
IpcIface::IpcIface(int argc, char** argv) {
    static constexpr char IPC_FLAG[6] = "--ipc";
    active = false;
    for (auto i = 1; i < argc; ++i) {
        if (strncmp(argv[i], IPC_FLAG, strlen(IPC_FLAG)) == 0) {
            // Check for duplicate args
            if (active) {
                fprintf(stderr, "[IPC] Duplicate IPC thread args: %s", argv[i]);
                exit(IPC_ERR_DOUBLE_ARG);
            }
            // Parse IPC thread arguments
            char* ipc_args = argv[i] + strlen(IPC_FLAG) + 1;
            char* tx = strtok(ipc_args, ",");
            char* rx = strtok(NULL, ",");
            // Store arguments persistently
            targs.tx = (char*)malloc(strlen(tx));
            targs.rx = (char*)malloc(strlen(rx));
            strcpy(targs.tx, tx);
            strcpy(targs.rx, rx);
            // Initialize IO thread which will handle TX, RX pipes
            pthread_create(&thread, NULL, *ipc_thread_handle, (void*)&targs);
            printf("[IPC] Thread launched with TX FIFO `%s`, RX FIFO `%s`\n",
                   targs.tx, targs.rx);
            active = true;
        }
    }
}

// Conditionally destroy IPC iff it is enabled
IpcIface::~IpcIface() {
    if (active) {
        pthread_join(thread, NULL);
        printf("[IPC] Thread joined\n");
        active = false;
        free(targs.tx);
        free(targs.rx);
    }
}
