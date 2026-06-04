import numpy as np

# from the C++ file:
h = np.array([
    0.0000000000,
    0.0000252163,
    -0.0001129608,
    0.0002920706,
    -0.0006068017,
    0.0011173534,
    -0.0018998195,
    0.0030468560,
    -0.0046715548,
    0.0069193986,
    -0.0099988508,
    0.0142569993,
    -0.0203791073,
    0.0300061320,
    -0.0482966219,
    0.1034327164,
    0.9824491082,
    -0.0797343920,
    0.0380217824,
    -0.0226100376,
    0.0144260497,
    -0.0093816375,
    0.0060611098,
    -0.0038253847,
    0.0023252815,
    -0.0013400664,
    0.0007157679,
    -0.0003397913,
    0.0001298911,
    -0.0000287070,
    -0.0000000000,
])

w, H = np.fft.fftfreq(1024, 1/48000), np.fft.fft(h, 1024)
idx = np.argsort(w)
w = w[idx]
H = H[idx]

freqs = [1000, 5000, 10000, 20000]
for f in freqs:
    # find closest freq
    i = np.argmin(np.abs(w - f))
    mag = 20 * np.log10(np.abs(H[i]))
    phase = np.angle(H[i])
    print(f"Freq: {f} Hz, Mag: {mag:.3f} dB, Phase: {phase:.3f} rad")

# Check if delay is flat
phases = np.unwrap(np.angle(H[w >= 0]))
w_pos = w[w >= 0]
delays = -np.diff(phases) / np.diff(w_pos) / (2 * np.pi)
print(f"Delay at 1kHz: {delays[np.argmin(np.abs(w_pos - 1000))] * 48000:.3f} samples")
print(f"Delay at 10kHz: {delays[np.argmin(np.abs(w_pos - 10000))] * 48000:.3f} samples")

