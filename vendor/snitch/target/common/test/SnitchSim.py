#!/usr/bin/env python3
# Copyright 2020 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#
# Paul Scheffler <paulsc@iis.ee.ethz.ch>
# Luca Colagrande <colluca@iis.ee.ethz.ch>
#
# This class implements a minimal wrapping IPC server for `tb_lib`.
# `__main__` shows a demonstrator for it, running a simulation and accessing its memory.

import os
import sys
import tempfile
import subprocess
import struct
import functools
import threading
import time
import signal

# Simulation monitor polling period (in seconds)
SIM_MONITOR_POLL_PERIOD = 2


class SnitchSim:

    def __init__(self, sim_bin: str, snitch_bin: str, log: str = None):
        self.sim_bin = sim_bin
        self.snitch_bin = snitch_bin
        self.sim = None
        self.tmpdir = None
        self.log = open(log, 'w+') if log else log

    def start(self):
        # Create FIFOs
        self.tmpdir = tempfile.TemporaryDirectory()
        tx_fd = os.path.join(self.tmpdir.name, 'tx')
        os.mkfifo(tx_fd)
        rx_fd = os.path.join(self.tmpdir.name, 'rx')
        os.mkfifo(rx_fd)
        # Start simulator process
        ipc_arg = f'--ipc,{tx_fd},{rx_fd}'
        self.sim = subprocess.Popen([self.sim_bin, self.snitch_bin, ipc_arg], stdout=self.log)
        # Open FIFOs
        self.tx = open(tx_fd, 'wb', buffering=0)  # Unbuffered
        self.rx = open(rx_fd, 'rb')
        # Create thread to monitor simulation
        self.stop_sim_monitor = threading.Event()
        self.sim_monitor = threading.Thread(target=self.__monitor_sim)
        self.sim_monitor.start()

    def __sim_active(func):
        @functools.wraps(func)
        def inner(self, *args, **kwargs):
            if self.sim is None:
                raise RuntimeError(f'Snitch is not running (simulation `{self.sim_bin}`'
                                   f'binary `{self.snitch_bin}`)')
            # Catch SIGINT raised by simulation monitor
            try:
                return func(self, *args, **kwargs)
            except KeyboardInterrupt:
                print('Simulation monitor detected a simulation failure.')
                self.stop_sim_monitor.set()
                self.sim_monitor.join()
                sys.exit(1)
        return inner

    def __monitor_sim(self):
        while not self.stop_sim_monitor.is_set():
            if self.sim.poll() is not None:
                # Raise SIGINT to interrupt main thread, could be blocked on a read
                os.kill(os.getpid(), signal.SIGINT)
            time.sleep(SIM_MONITOR_POLL_PERIOD)

    @__sim_active
    def read(self, addr: int, length: int) -> bytes:
        op = struct.pack('=QQQ', 0, addr, length)
        self.tx.write(op)
        return self.rx.read(length)

    @__sim_active
    def write(self, addr: int, data: bytes):
        op = struct.pack('=QQQ', 1, addr, len(data))
        self.tx.write(op)
        self.tx.write(data)

    @__sim_active
    def poll(self, addr: int, mask32: int, exp32: int):
        op = struct.pack('=QQLL', 2, addr, mask32, exp32)
        while True:
            try:
                self.tx.write(op)
            except IOError:
                print('Broken pipe error')
                continue
            break
        bytestring = self.rx.read(4)
        return int.from_bytes(bytestring, byteorder='little')

    @__sim_active
    def finish(self, wait_for_sim: bool = True):
        # Close FIFOs (simulator can exit only once TX FIFO closes)
        self.rx.close()
        self.tx.close()
        # Close simulation monitor
        self.stop_sim_monitor.set()
        self.sim_monitor.join()
        # Wait for simulation or terminate
        if (wait_for_sim):
            self.sim.wait()
        else:
            self.sim.terminate()
        # Cleanup
        self.tmpdir.cleanup()
        self.sim = None


if __name__ == "__main__":
    sim = SnitchSim(*sys.argv[1:])
    sim.start()

    wstr = b'This is a test string to be written to testbench memory.'
    sim.write(0xdeadbeef, wstr)
    rstr = sim.read(0xdeadbeef, len(wstr)+5)
    print(f'Read back string: `{rstr}`')

    sim.finish(wait_for_sim=False)
