import numpy as np
import torch

dtype = torch.int32

# Define the matrices as 1D arrays
gemm_A_dram = np.array([0x315, 0x269, 0xfa, 0x31, 0x3f, 0x1b2, 0x19c, 0x3ad, 0x2cb, 0x362, 0x28, 0x340, 0x39e, 0x183, 0x99, 0x112, 0x29c, 0x205, 0x2f7, 0x31, 0xf7, 0x194, 0x3cd, 0xb0, 0x317, 0x12d, 0x1bb, 0x75, 0x1, 0x1, 0x36c, 0x255])
gemm_B_dram = np.array([0x1ed, 0x2c0, 0x1cb, 0xa3, 0x3dc, 0x3a2, 0xda, 0x7a, 0x320, 0x12a, 0x2ee, 0x2a8, 0x3af, 0x93, 0x13b, 0x15, 0x311, 0x2ca, 0x334, 0x107, 0x20f, 0x22d, 0x2d, 0x1c5, 0x2bf, 0xe6, 0x394, 0x29c, 0x9c, 0x28, 0x29e, 0x178])

# Reshape the 1D arrays into 2D arrays
gemm_A_dram = gemm_A_dram.reshape((8, 4))
gemm_B_dram = gemm_B_dram.reshape((4, 8))

# Perform the matrix multiplication with original B
# result = np.matmul(gemm_A_dram, gemm_B_dram)


# Convert numpy arrays to PyTorch tensors
gemm_A_dram_torch = torch.from_numpy(gemm_A_dram).type(torch.int32)
gemm_B_dram_torch = torch.from_numpy(gemm_B_dram).type(torch.int32)
result = torch.matmul(gemm_A_dram_torch, gemm_B_dram_torch)

print("Mat_A:")
for row in gemm_A_dram_torch:
    print([hex(x) for x in row])

print("Mat_B:")
for row in gemm_B_dram_torch:
    print([hex(x) for x in row])

# Print the result in hexadecimal format
print("Result with GEMM:")
for row in result:
    print([hex(x) for x in row])
