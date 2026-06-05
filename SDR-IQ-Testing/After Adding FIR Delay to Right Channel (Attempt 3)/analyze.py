import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

def parse_frequency(filename):
    """Extracts frequency in Hz from the filename."""
    match = re.search(r'sdr-test-([\d\.]+)(Hz|kHz|MHz)\.wav', filename, re.IGNORECASE)
    if match:
        value = float(match.group(1))
        unit = match.group(2).lower()
        
        if unit == 'khz':
            value *= 1000.0
        elif unit == 'mhz':
            value *= 1000000.0
            
        return value
    return None

def calculate_rms(signal):
    """Calculates the Root Mean Square amplitude of a signal."""
    sig_float = signal.astype(np.float64)
    return np.sqrt(np.mean(sig_float**2))

def calc_db_ratio(amp1, amp2):
    """Safely calculates 20 * log10(amp1 / amp2)."""
    if amp2 == 0:
        return np.inf if amp1 > 0 else 0.0
    if amp1 == 0:
        return -np.inf
    return 20.0 * np.log10(amp1 / amp2)

def calc_phase_difference(left, right):
    """Calculates the absolute phase difference in degrees."""
    # Compute the mean of the pointwise product
    dot_product = np.mean(left * right)
    
    rms_left = calculate_rms(left)
    rms_right = calculate_rms(right)
    
    if rms_left == 0 or rms_right == 0:
        return 0.0
        
    # Calculate cos(phi) and clip to [-1, 1] to prevent floating point domain errors
    cos_phi = dot_product / (rms_left * rms_right)
    cos_phi = np.clip(cos_phi, -1.0, 1.0)
    
    # Return phase in degrees
    return np.degrees(np.arccos(cos_phi))

def analyze_adc_files(directory="."):
    """Finds WAV files, analyzes L/R channels, and plots amplitude and phase results."""
    search_pattern = os.path.join(directory, "sdr-test-*.wav")
    files = glob.glob(search_pattern)
    
    if not files:
        print(f"No files matching 'sdr-test-*.wav' found in {directory}")
        return

    results = []

    print(f"Found {len(files)} test files. Analyzing...")
    
    for file in files:
        freq_hz = parse_frequency(os.path.basename(file))
        if freq_hz is None:
            print(f"Skipping {file}: Could not parse frequency.")
            continue
            
        sample_rate, data = wavfile.read(file)
        
        if len(data.shape) < 2 or data.shape[1] < 2:
            print(f"Skipping {file}: Not a stereo WAV file.")
            continue
            
        left = data[:, 0].astype(np.float64)
        right = data[:, 1].astype(np.float64)
        
        sum_sig = left + right
        diff_sig = left - right
        
        amp_left = calculate_rms(left)
        amp_right = calculate_rms(right)
        amp_sum = calculate_rms(sum_sig)
        amp_diff = calculate_rms(diff_sig)
        
        # Calculate Metrics
        lr_imbalance_db = calc_db_ratio(amp_left, amp_right)
        sum_diff_ratio_db = calc_db_ratio(amp_sum, amp_diff)
        phase_diff_deg = calc_phase_difference(left, right)
        
        results.append({
            'freq': freq_hz,
            'lr_imbalance_db': lr_imbalance_db,
            'sum_diff_ratio_db': sum_diff_ratio_db,
            'phase_diff_deg': phase_diff_deg
        })
        
        print(f"Processed {freq_hz/1000:.2f} kHz -> Imbalance: {lr_imbalance_db:+.3f} dB | Phase Error: {phase_diff_deg:.3f}°")

    if not results:
        return

    # Sort results by frequency for clean plotting
    results.sort(key=lambda x: x['freq'])
    
    frequencies = [r['freq'] for r in results]
    sum_diff_ratios = [r['sum_diff_ratio_db'] for r in results]
    lr_imbalances = [r['lr_imbalance_db'] for r in results]
    phase_differences = [r['phase_diff_deg'] for r in results]

    # Plotting (Increased figure size to accommodate 3 plots)
    plt.figure(figsize=(12, 12))

    # Chart 1: Sum vs Difference Amplitude Ratio (dB)
    plt.subplot(3, 1, 1)
    plt.plot(frequencies, sum_diff_ratios, marker='o', linestyle='-', color='b')
    plt.title("Sum vs. Difference Ratio (Higher is better alignment)")
    plt.ylabel("Ratio (Sum / Diff) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')

    # Chart 2: Left vs Right Amplitude Imbalance (dB)
    plt.subplot(3, 1, 2)
    plt.plot(frequencies, lr_imbalances, marker='s', linestyle='-', color='r')
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Left vs. Right Amplitude Imbalance (Closer to 0 is better)")
    plt.ylabel("Imbalance (Left / Right) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')

    # Chart 3: Phase Difference (Degrees)
    plt.subplot(3, 1, 3)
    plt.plot(frequencies, phase_differences, marker='^', linestyle='-', color='g')
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Phase Difference Between Channels (Closer to 0° is better)")
    plt.ylabel("Absolute Phase Difference [Degrees]")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    analyze_adc_files()
