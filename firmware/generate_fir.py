import numpy as np

fs = 48000
delay_us = 18.8803
delay_samples = delay_us * 1e-6 * fs

taps = 31
center = (taps - 1) / 2

# We want the right channel to have a delay of 'center + delay_samples'
shift = center + delay_samples

n = np.arange(taps)
# np.sinc in numpy is sin(pi*x)/(pi*x)
h = np.sinc(n - shift)

# Apply a window to reduce ringing. Blackman window is good.
window = np.blackman(taps)
h = h * window

# Normalize to ensure unity DC gain
h = h / np.sum(h)

print(f"Delay in samples: {delay_samples}")
print("Coefficients:")
for i in range(taps):
    print(f"    {h[i]:.10f}f,")
