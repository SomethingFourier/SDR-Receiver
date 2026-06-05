import numpy as np

fs = 48000
# We need exact delay of 18.931 us
target_delay_us = 18.931
delay_samples = target_delay_us * 1e-6 * fs  # 0.908688 samples

N = 31 # Number of taps
center = (N - 1) / 2
n = np.arange(N)

x = n - center - delay_samples

def continuous_blackman(n_val, M):
    return 0.42 - 0.5 * np.cos(2 * np.pi * n_val / (M - 1)) + 0.08 * np.cos(4 * np.pi * n_val / (M - 1))

# Shift the window so its center matches the sinc peak exactly
window_new = continuous_blackman(n - delay_samples, N)
window_new = np.where((n - delay_samples >= 0) & (n - delay_samples <= N - 1), window_new, 0)

h = np.sinc(x) * window_new
h = h / np.sum(h)

print(f"const float right_channel_fir[{N}] = {{")
for val in h:
    print(f"    {val:.10f}f,")
print("};")
