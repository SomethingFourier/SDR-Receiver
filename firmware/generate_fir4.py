import numpy as np

fs = 48000
delay_samples = 0.906528

N = 127 # Number of taps
center = (N - 1) / 2
n = np.arange(N)

x = n - center - delay_samples
h = np.sinc(x) * np.blackman(N)
h = h / np.sum(h)

print(f"const float right_channel_fir[{N}] = {{")
for val in h:
    print(f"    {val:.10f}f,")
print("};")
