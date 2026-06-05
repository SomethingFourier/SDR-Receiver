import numpy as np

N = 31
center = 15
delay_samples = 0.906864

n = np.arange(N)
x = n - center - delay_samples

# Old way (window centered at 15)
h_old = np.sinc(x) * np.blackman(N)
h_old = h_old / np.sum(h_old)

# Calculate expected delay (center of gravity)
delay_old = np.sum(n * h_old)
print(f"Old effective delay: {delay_old - 15:.6f} samples. Expected: {delay_samples:.6f}")

# New way (window centered at 15 + delay_samples)
# We can just sample the continuous Blackman window
def continuous_blackman(n_val, M):
    # M is length. n_val goes from 0 to M-1.
    return 0.42 - 0.5 * np.cos(2 * np.pi * n_val / (M - 1)) + 0.08 * np.cos(4 * np.pi * n_val / (M - 1))

# We want the window centered at center + delay_samples.
# So we shift the window function.
# The standard window is centered at (N-1)/2. 
# We evaluate it at n - delay_samples so it moves with the sinc.
window_new = continuous_blackman(n - delay_samples, N)
# Zero out values outside the valid range [0, N-1] for the window
window_new = np.where((n - delay_samples >= 0) & (n - delay_samples <= N - 1), window_new, 0)

h_new = np.sinc(x) * window_new
h_new = h_new / np.sum(h_new)
delay_new = np.sum(n * h_new)
print(f"New effective delay: {delay_new - 15:.6f} samples")

