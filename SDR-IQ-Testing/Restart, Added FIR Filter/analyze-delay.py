import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

# --- CONFIGURATION ---
# Set your desired phase delay here in microseconds (us). 
CORRECTION_DELAY_US = 1.93  

# Set your desired amplitude correction in Decibels (dB).
# A positive value boosts the Right channel. A negative value attenuates it.
CORRECTION_GAIN_DB = -0.005 

TEST_DIRECTORY = "." 

# FIR Filter Configuration
FIR_TAPS = 127 
# ---------------------

def parse_frequency(filename):
    match = re.search(r'sdr-test-([\d\.]+)(Hz|kHz|MHz)\.wav', filename, re.IGNORECASE)
    if match:
        value = float(match.group(1))
        unit = match.group(2).lower()
        if unit == 'khz': value *= 1000.0
        elif unit == 'mhz': value *= 1000000.0
        return value
    return None

def calculate_rms(signal):
    sig_float = signal.astype(np.float64)
    return np.sqrt(np.mean(sig_float**2))

def calc_db_ratio(amp1, amp2):
    if amp2 == 0: return np.inf if amp1 > 0 else 0.0
    if amp1 == 0: return -np.inf
    return 20.0 * np.log10(amp1 / amp2)

def calc_phase_difference(left, right):
    dot_product = np.mean(left * right)
    rms_left, rms_right = calculate_rms(left), calculate_rms(right)
    if rms_left == 0 or rms_right == 0: return 0.0
    cos_phi = np.clip(dot_product / (rms_left * rms_right), -1.0, 1.0)
    return np.degrees(np.arccos(cos_phi))

def apply_fir_fractional_delay(signal, sample_rate, delay_us, num_taps):
    delay_sec = delay_us * 1e-6
    delay_samples = delay_sec * sample_rate
    
    n = np.arange(num_taps) - (num_taps - 1) / 2.0
    h = np.sinc(n - delay_samples)
    
    window = np.blackman(num_taps)
    h = h * window
    
    h = h / np.sum(h)
    
    return np.convolve(signal, h, mode='same')

def analyze_adc_with_corrections(directory, delay_us, gain_db, num_taps):
    search_pattern = os.path.join(directory, "sdr-test-*.wav")
    files = glob.glob(search_pattern)
    
    if not files:
        print(f"No files matching 'sdr-test-*.wav' found in '{directory}'")
        return

    # Convert the dB gain correction into a linear multiplier
    linear_gain_scalar = 10.0 ** (gain_db / 20.0)

    results = []
    print(f"Analyzing {len(files)} files.")
    print(f"Applying {delay_us}µs FIR delay and {gain_db:+.2f} dB gain to Right channel...")
    
    for file in files:
        freq_hz = parse_frequency(os.path.basename(file))
        if freq_hz is None: continue
            
        sample_rate, data = wavfile.read(file)
        if len(data.shape) < 2 or data.shape[1] < 2: continue
            
        left = data[:, 0].astype(np.float64)
        right_orig = data[:, 1].astype(np.float64)
        
        # --- 1. Calculate Original Baseline ---
        sum_orig = left + right_orig
        diff_orig = left - right_orig
        
        lr_imb_orig = calc_db_ratio(calculate_rms(left), calculate_rms(right_orig))
        sd_ratio_orig = calc_db_ratio(calculate_rms(sum_orig), calculate_rms(diff_orig))
        phase_orig = calc_phase_difference(left, right_orig)
        
        # --- 2. Apply Corrections & Calculate Fixed Metrics ---
        # First, correct the phase with the FIR filter
        right_fixed = apply_fir_fractional_delay(right_orig, sample_rate, delay_us, num_taps)
        
        # Second, correct the amplitude imbalance
        right_fixed = right_fixed * linear_gain_scalar
        
        sum_fixed = left + right_fixed
        diff_fixed = left - right_fixed
        
        lr_imb_fixed = calc_db_ratio(calculate_rms(left), calculate_rms(right_fixed))
        sd_ratio_fixed = calc_db_ratio(calculate_rms(sum_fixed), calculate_rms(diff_fixed))
        phase_fixed = calc_phase_difference(left, right_fixed)
        
        results.append({
            'freq': freq_hz,
            'orig_sd_ratio': sd_ratio_orig, 'orig_lr_imb': lr_imb_orig, 'orig_phase': phase_orig,
            'fixed_sd_ratio': sd_ratio_fixed, 'fixed_lr_imb': lr_imb_fixed, 'fixed_phase': phase_fixed
        })
        print(f"  {freq_hz/1000:.2f} kHz -> Imbalance: {lr_imb_orig:+.3f}dB -> {lr_imb_fixed:+.3f}dB | Phase: {phase_orig:.3f}° -> {phase_fixed:.3f}°")

    results.sort(key=lambda x: x['freq'])
    plot_comparison(results, delay_us, gain_db, num_taps)

def plot_comparison(results, delay_us, gain_db, num_taps):
    frequencies = [r['freq'] for r in results]

    def ext(metric): return [r[metric] for r in results]
    
    label_text = f"Fixed ({delay_us}µs, {gain_db:+.3f}dB)"

    # ==========================================
    # WINDOW 1: Amplitude Metrics (Logarithmic)
    # ==========================================
    plt.figure(figsize=(12, 8))

    # Chart 1: Sum vs Difference Ratio
    plt.subplot(2, 1, 1)
    plt.plot(frequencies, ext('orig_sd_ratio'), marker='o', linestyle='--', color='lightsteelblue', label="Unaltered")
    plt.plot(frequencies, ext('fixed_sd_ratio'), marker='o', linestyle='-', color='mediumblue', label=label_text)
    plt.title("Sum vs. Difference Ratio (Higher is better isolation/alignment)")
    plt.ylabel("Ratio (Sum / Diff) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

    # Chart 2: Left vs Right Imbalance
    plt.subplot(2, 1, 2)
    plt.plot(frequencies, ext('orig_lr_imb'), marker='s', linestyle='--', color='lightcoral', label="Unaltered")
    plt.plot(frequencies, ext('fixed_lr_imb'), marker='s', linestyle='-', color='darkred', label=label_text)
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Left vs. Right Amplitude Imbalance (Closer to 0 is better)")
    plt.ylabel("Imbalance [dB]")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

    plt.tight_layout()

    # ==========================================
    # WINDOW 2: Phase Metric (Linear)
    # ==========================================
    plt.figure(figsize=(12, 6))

    plt.plot(frequencies, ext('orig_phase'), marker='^', linestyle='--', color='darkseagreen', label="Unaltered")
    plt.plot(frequencies, ext('fixed_phase'), marker='^', linestyle='-', color='darkgreen', label=label_text)
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title(f"Phase Difference (FIR Filter, {num_taps} Taps) - Linear Frequency Scale")
    plt.ylabel("Absolute Phase Difference [Degrees]")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True, which="both", ls="--")
    plt.xscale('linear') # Explicitly set to linear scale
    plt.legend()

    plt.tight_layout()
    
    # Render both windows simultaneously
    plt.show()

if __name__ == "__main__":
    analyze_adc_with_corrections(TEST_DIRECTORY, CORRECTION_DELAY_US, CORRECTION_GAIN_DB, FIR_TAPS)
