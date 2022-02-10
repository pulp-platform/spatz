#!/usr/bin/env python3
# Copyright 2021 ETH Zurich and University of Bologna.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# arg1: image size, arg2: filter size

import numpy as np
import scipy.signal
import sys

def rand_matrix(N, M, seed):
        return np.arange(seed, seed+N*M, dtype=np.float32).reshape(N, M) * 3.141

def emit(name, array, alignment='4'):
        print(".global %s" % name)
        print(".align " + alignment)
        print("%s:" % name)
        bs = array.tobytes()
        for i in range(0, len(bs), 4):
                s = ""
                for n in range(4):
                        s += "%02x" % bs[i+3-n]
                print("    .word 0x%s" % s)

if len(sys.argv) > 1:
        matrix_width = int(sys.argv[1])
        filter_size = int(sys.argv[2])
        # Filter size must be odd
        assert(filter_size % 2 == 1), "The filter size must be an odd integer number"
else:
        matrix_width = 64
        filter_size = 7

# Input image. Take a square image
M = matrix_width
N = matrix_width
# 3 Channels
CH = 3
padding = int(filter_size/2)
M_pad = M + 2*padding
N_pad = N + 2*padding
assert(M % 4 == 0), "Output image dimension must be divisible by 4, pad the input image accordingly"
assert(N % 4 == 0), "Output image dimension must be divisible by 4, pad the input image accordingly"

image = list()
# Generate a random int32 input padded image
for ch in range(CH):
        image += [np.around(rand_matrix(M_pad, N_pad, ch)).astype(np.int32)]
        np.random.shuffle(image[ch].flat)

gen_filter = list()
# Generate a random int32 filter
for ch in range(CH):
        gen_filter += [np.around(rand_matrix(filter_size, filter_size, CH + ch)).astype(np.int32)]
        np.random.shuffle(gen_filter[ch].flat)

# Create the empty o matrix
empty_o = np.zeros((M, N)).astype(np.int32)

# Calculate the output matrix
result = np.zeros((M, N)).astype(np.int32)
for ch in range(CH):
        result += np.around(scipy.signal.convolve2d(np.flip(gen_filter[ch]), image[ch], 'valid')).astype(np.int32) # https://stackoverflow.com/questions/41613155/what-does-scipy-signal-convolve2d-calculate

# Calculate a checksum
checksum = np.sum(result, dtype=np.int32)

# Print information on display
#print("Image:\n")
#print(image)
#print("Filter:\n")
#print(gen_filter)
#print("Results:\n")
#print(result)
#print("\n")
#print(checksum)

# Print information on file
print(".section .data,\"aw\",@progbits")
emit("M", np.array(M, dtype=np.uint32))
emit("N", np.array(N, dtype=np.uint32))
emit("F", np.array(filter_size, dtype=np.uint32))
emit("CH", np.array(CH, dtype=np.uint32))
emit("i", np.concatenate(image))
emit("f", np.concatenate(gen_filter))
emit("o", empty_o)
emit("golden_o", result)
emit("o_checksum", checksum)
