# Generate FFT data

N_CORES   from [1, 2]
N_SAMPLES from [4, 8, 16, 32, 64, 128, 256, 512, 1024]

python script/gen_data.py ${N_CORES} ${N_SAMPLES} > data/data_fft.h
