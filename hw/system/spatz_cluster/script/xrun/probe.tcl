# Copyright 2021 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

database -open waves -shm -into waves.shm -default
probe -create tb_bin -all -depth all -database waves
run
