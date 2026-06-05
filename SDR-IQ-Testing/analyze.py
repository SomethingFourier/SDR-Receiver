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
    dot_product = np.mean(left * right)
    rms_left = calculate_rms(left)
    rms_right = calculate_rms(right)
    
    if rms_left == 0 or rms_right == 0:
        return 0.0
        
    cos_phi = dot_product / (rms_left * rms_right)
    cos_phi = np.clip(cos_phi, -1.0, 1.0)
    return np.degrees(np.arccos(cos_phi))

def process_directory(directory_name):
    """Processes all valid WAV files in a given directory and returns sorted results."""
    # Build the path relative to where the script is run
    current_dir = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else os.getcwd()
    target_path = os.path.join(current_dir, directory_name)
    search_pattern = os.path.join(target_path, "sdr-test-*.wav")
    
    files = glob.glob(search_pattern)
    results = []

    if not files:
        print(f"Warning: No files matching 'sdr-test-*.wav' found in '{directory_name}'")
        return results

    print(f"\nAnalyzing {len(files)} files in '{directory_name}'...")
    
    for file in files:
        freq_hz = parse_frequency(os.path.basename(file))
        if freq_hz is None:
            continue
            
        sample_rate, data = wavfile.read(file)
        
        if len(data.shape) < 2 or data.shape[1] < 2:
            continue
            
        left = data[:, 0].astype(np.float64)
        right = data[:, 1].astype(np.float64)
        
        sum_sig = left + right
        diff_sig = left - right
        
        amp_left = calculate_rms(left)
        amp_right = calculate_rms(right)
        amp_sum = calculate_rms(sum_sig)
        amp_diff = calculate_rms(diff_sig)
        
        lr_imbalance_db = calc_db_ratio(amp_left, amp_right)
        sum_diff_ratio_db = calc_db_ratio(amp_sum, amp_diff)
        phase_diff_deg = calc_phase_difference(left, right)
        
        results.append({
            'freq': freq_hz,
            'lr_imbalance_db': lr_imbalance_db,
            'sum_diff_ratio_db': sum_diff_ratio_db,
            'phase_diff_deg': phase_diff_deg
        })
        
        print(f"  {freq_hz/1000:.2f} kHz -> Imbalance: {lr_imbalance_db:+.3f} dB | Phase Error: {phase_diff_deg:.3f}°")

    # Sort results by frequency
    results.sort(key=lambda x: x['freq'])
    return results

def compare_adc_tests():
    """Runs the analysis on both folders and plots them together."""
    dir_before = "Before Adding C106, C121"
    dir_after = "After Adding C106, C121"

    results_before = process_directory(dir_before)
    results_after = process_directory(dir_after)

    if not results_before and not results_after:
        print("\nError: No valid data found in either directory. Check your folder names.")
        return

    plt.figure(figsize=(12, 12))

    # Helper function to extract lists for plotting
    def extract(results, key):
        return [r[key] for r in results]

    # --- Chart 1: Sum vs Difference Amplitude Ratio (dB) ---
    plt.subplot(3, 1, 1)
    if results_before:
        plt.plot(extract(results_before, 'freq'), extract(results_before, 'sum_diff_ratio_db'), 
                 marker='o', linestyle='--', color='lightsteelblue', label="Before (C106, C121)")
    if results_after:
        plt.plot(extract(results_after, 'freq'), extract(results_after, 'sum_diff_ratio_db'), 
                 marker='o', linestyle='-', color='mediumblue', label="After (C106, C121)")
                 
    plt.title("Sum vs. Difference Ratio (Higher is better alignment)")
    plt.ylabel("Ratio (Sum / Diff) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

    # --- Chart 2: Left vs Right Amplitude Imbalance (dB) ---
    plt.subplot(3, 1, 2)
    if results_before:
        plt.plot(extract(results_before, 'freq'), extract(results_before, 'lr_imbalance_db'), 
                 marker='s', linestyle='--', color='lightcoral', label="Before (C106, C121)")
    if results_after:
        plt.plot(extract(results_after, 'freq'), extract(results_after, 'lr_imbalance_db'), 
                 marker='s', linestyle='-', color='darkred', label="After (C106, C121)")
                 
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Left vs. Right Amplitude Imbalance (Closer to 0 is better)")
    plt.ylabel("Imbalance (Left / Right) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

    # --- Chart 3: Phase Difference (Degrees) ---
    plt.subplot(3, 1, 3)
    if results_before:
        plt.plot(extract(results_before, 'freq'), extract(results_before, 'phase_diff_deg'), 
                 marker='^', linestyle='--', color='darkseagreen', label="Before (C106, C121)")
    if results_after:
        plt.plot(extract(results_after, 'freq'), extract(results_after, 'phase_diff_deg'), 
                 marker='^', linestyle='-', color='darkgreen', label="After (C106, C121)")
                 
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Phase Difference Between Channels (Closer to 0° is better)")
    plt.ylabel("Absolute Phase Difference [Degrees]")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    compare_adc_tests()
