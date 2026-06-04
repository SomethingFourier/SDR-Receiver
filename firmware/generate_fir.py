import numpy as np

fs = 48000
delay_us = 1.958
delay_samples = delay_us * 1e-6 * fs

N = 31 # Number of taps
center = (N - 1) / 2
n = np.arange(N)

# sinc(x) in numpy is sin(pi*x)/(pi*x)
x = n - center - delay_samples
h = np.sinc(x) * np.blackman(N)
h = h / np.sum(h) # Normalize gain to 1

print(f"const float right_channel_fir[{N}] = {{")
for val in h:
    print(f"    {val:.10f}f,")
print("};")
