import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

def parse_frequency(filename):
    match = re.search(r'sdr-test-([\d\.]+)(Hz|kHz|MHz)\.wav', filename, re.IGNORECASE)
    if match:
        value = float(match.group(1))
        unit = match.group(2).lower()
        if unit == 'khz': value *= 1000.0
        elif unit == 'mhz': value *= 1000000.0
        return value
    return None

def measure_time_offset(left, right, freq_hz):
    """
    Measures the time offset between left and right channels using FFT.
    Returns the delay in microseconds.
    Positive delay: Right channel is delayed (lagging) relative to Left.
    Negative delay: Left channel is delayed (lagging) relative to Right.
    """
    # Use blackman window to reduce spectral leakage
    window = np.blackman(len(left))
    
    fft_l = np.fft.rfft(left * window)
    fft_r = np.fft.rfft(right * window)
    
    # Find the bin with the strongest signal (the test frequency)
    idx = np.argmax(np.abs(fft_l))
    
    # Calculate phase at the dominant frequency
    phase_l = np.angle(fft_l[idx])
    phase_r = np.angle(fft_r[idx])
    
    # Phase difference: Left - Right
    phase_diff_rad = phase_l - phase_r
    
    # Wrap to [-pi, pi]
    phase_diff_rad = (phase_diff_rad + np.pi) % (2 * np.pi) - np.pi
    
    # Calculate time delay: t = delta_phi / (2 * pi * f)
    # If phase_diff is positive, phase_l > phase_r, so right is lagging left.
    delay_sec = phase_diff_rad / (2.0 * np.pi * freq_hz)
    
    # Convert to microseconds
    delay_us = delay_sec * 1e6
    
    return delay_us

def analyze_directory(directory):
    search_pattern = os.path.join(directory, "sdr-test-*.wav")
    files = glob.glob(search_pattern)
    
    if not files:
        print(f"No files matching 'sdr-test-*.wav' found in '{directory}'")
        return

    results = []

    print(f"Analyzing {len(files)} files...\n")
    print(f"{'Frequency (Hz)':>15} | {'Delay (µs)':>15} | {'Direction'}")
    print("-" * 55)
    
    for file in files:
        freq_hz = parse_frequency(os.path.basename(file))
        if freq_hz is None: 
            continue
            
        sample_rate, data = wavfile.read(file)
        if len(data.shape) < 2 or data.shape[1] < 2: 
            continue
            
        left = data[:, 0].astype(np.float64)
        right = data[:, 1].astype(np.float64)
        
        delay_us = measure_time_offset(left, right, freq_hz)
        
        results.append({
            'freq': freq_hz,
            'delay_us': delay_us
        })

    # Sort results by frequency
    results.sort(key=lambda x: x['freq'])
    
    for r in results:
        direction = "Right is delayed" if r['delay_us'] > 0 else "Left is delayed"
        if abs(r['delay_us']) < 1e-6:
            direction = "Perfectly aligned"
        print(f"{r['freq']:15.2f} | {abs(r['delay_us']):15.6f} | {direction}")

    # Plotting
    frequencies = [r['freq'] for r in results]
    delays = [r['delay_us'] for r in results]
    
    plt.figure(figsize=(12, 7))
    
    plt.plot(frequencies, delays, marker='o', linestyle='-', color='purple', linewidth=2, markersize=6)
    plt.axhline(y=0, color='k', linestyle='--', alpha=0.7)
    
    # Fill colors based on which channel is delayed
    plt.fill_between(frequencies, 0, delays, where=(np.array(delays) > 0), color='red', alpha=0.15, label='Right Channel Delayed (+)')
    plt.fill_between(frequencies, 0, delays, where=(np.array(delays) < 0), color='blue', alpha=0.15, label='Left Channel Delayed (-)')

    plt.title("L-R (I-Q) Sub-sample Time Offset vs Frequency", fontsize=14, pad=15)
    plt.ylabel("Time Offset / Delay (µs)", fontsize=12)
    plt.xlabel("Frequency (Hz)", fontsize=12)
    
    # Formatting the axes
    plt.grid(True, which="both", ls="-", alpha=0.3)
    plt.xscale('log')
    
    # Add a custom legend
    plt.legend(loc='upper right', fontsize=10)
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    # Target the directory containing the script
    target_dir = os.path.dirname(os.path.abspath(__file__))
    analyze_directory(target_dir)
