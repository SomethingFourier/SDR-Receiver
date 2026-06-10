import os
import glob
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

# --- CONFIGURATION ---
# Set these to 0.0 to just run a pure diagnostic on your hardware
CORRECTION_DELAY_US = 0.0  
CORRECTION_GAIN_DB = 0.0 

TEST_DIRECTORY = os.path.dirname(os.path.abspath(__file__))

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

def measure_delay_and_phase(left, right, freq_hz):
    """
    Uses FFT to determine the signed phase difference and microsecond delay.
    Positive delay = Right channel is lagging.
    Negative delay = Left channel is lagging.
    """
    window = np.blackman(len(left))
    
    fft_l = np.fft.rfft(left * window)
    fft_r = np.fft.rfft(right * window)
    
    idx = np.argmax(np.abs(fft_l))
    
    phase_l = np.angle(fft_l[idx])
    phase_r = np.angle(fft_r[idx])
    
    phase_diff_rad = phase_l - phase_r
    
    phase_diff_rad = (phase_diff_rad + np.pi) % (2 * np.pi) - np.pi
    phase_diff_deg = np.degrees(phase_diff_rad)
    
    delay_sec = phase_diff_rad / (2.0 * np.pi * freq_hz)
    delay_us = delay_sec * 1e6
    
    return delay_us, phase_diff_deg

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

    linear_gain_scalar = 10.0 ** (gain_db / 20.0)
    results = []
    
    detected_delays = []
    detected_imbalances = []

    print(f"Analyzing {len(files)} files...\n")
    
    for file in files:
        freq_hz = parse_frequency(os.path.basename(file))
        if freq_hz is None: continue
            
        sample_rate, data = wavfile.read(file)
        if len(data.shape) < 2 or data.shape[1] < 2: continue
            
        left = data[:, 0].astype(np.float64)
        right_orig = data[:, 1].astype(np.float64)
        
        # --- Calculate Original Baseline ---
        sum_orig = left + right_orig
        diff_orig = left - right_orig
        
        lr_imb_orig = calc_db_ratio(calculate_rms(left), calculate_rms(right_orig))
        sd_ratio_orig = calc_db_ratio(calculate_rms(sum_orig), calculate_rms(diff_orig))
        measured_delay_us, phase_orig = measure_delay_and_phase(left, right_orig, freq_hz)
        
        detected_delays.append(measured_delay_us)
        detected_imbalances.append(lr_imb_orig)

        # --- Apply Corrections ---
        right_fixed = apply_fir_fractional_delay(right_orig, sample_rate, delay_us, num_taps)
        right_fixed = right_fixed * linear_gain_scalar
        
        sum_fixed = left + right_fixed
        diff_fixed = left - right_fixed
        
        lr_imb_fixed = calc_db_ratio(calculate_rms(left), calculate_rms(right_fixed))
        sd_ratio_fixed = calc_db_ratio(calculate_rms(sum_fixed), calculate_rms(diff_fixed))
        fixed_delay_us, phase_fixed = measure_delay_and_phase(left, right_fixed, freq_hz)
        
        results.append({
            'freq': freq_hz,
            'orig_sd_ratio': sd_ratio_orig, 'orig_lr_imb': lr_imb_orig, 
            'orig_phase': abs(phase_orig), 'orig_delay': measured_delay_us,
            'fixed_sd_ratio': sd_ratio_fixed, 'fixed_lr_imb': lr_imb_fixed, 
            'fixed_phase': abs(phase_fixed), 'fixed_delay': fixed_delay_us
        })

    results.sort(key=lambda x: x['freq'])

    # --- Print Diagnostic Summary ---
    median_delay = np.median(detected_delays)
    median_imb = np.median(detected_imbalances)
    
    print("="*50)
    print(" SDR HARDWARE DIAGNOSTIC SUMMARY")
    print("="*50)
    
    if median_delay > 0:
        print(f"-> DELAY:     The RIGHT channel is lagging by {abs(median_delay):.3f} µs")
    elif median_delay < 0:
        print(f"-> DELAY:     The LEFT channel is lagging by {abs(median_delay):.3f} µs")
    else:
        print("-> DELAY:     Channels are perfectly time-aligned.")

    if median_imb > 0:
        print(f"-> AMPLITUDE: The LEFT channel is louder by {abs(median_imb):.3f} dB")
    elif median_imb < 0:
        print(f"-> AMPLITUDE: The RIGHT channel is louder by {abs(median_imb):.3f} dB")
    else:
        print("-> AMPLITUDE: Channels are perfectly amplitude-aligned.")
        
    print("="*50 + "\n")

    plot_comparison(results, delay_us, gain_db, num_taps)

def plot_comparison(results, delay_us, gain_db, num_taps):
    frequencies = [r['freq'] for r in results]

    def ext(metric): return [r[metric] for r in results]
    
    label_text = f"Fixed ({delay_us}µs, {gain_db:+.3f}dB)"

    # ==========================================
    # WINDOW 1: Amplitude Metrics (Logarithmic)
    # ==========================================
    plt.figure(figsize=(12, 8))

    plt.subplot(2, 1, 1)
    plt.plot(frequencies, ext('orig_sd_ratio'), marker='o', linestyle='--', color='lightsteelblue', label="Unaltered")
    plt.plot(frequencies, ext('fixed_sd_ratio'), marker='o', linestyle='-', color='mediumblue', label=label_text)
    plt.title("Sum vs. Difference Ratio (Higher is better isolation/alignment)")
    plt.ylabel("Ratio (Sum / Diff) [dB]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('log')
    plt.legend()

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
    # WINDOW 2: Phase & Delay Metrics (Linear)
    # ==========================================
    plt.figure(figsize=(12, 10))

    # Phase Plot
    plt.subplot(2, 1, 1)
    plt.plot(frequencies, ext('orig_phase'), marker='^', linestyle='--', color='darkseagreen', label="Unaltered Absolute Phase")
    plt.plot(frequencies, ext('fixed_phase'), marker='^', linestyle='-', color='darkgreen', label=label_text)
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title(f"Phase Difference (FIR Filter, {num_taps} Taps)")
    plt.ylabel("Absolute Phase Diff [Degrees]")
    plt.grid(True, which="both", ls="--")
    plt.xscale('linear') 
    plt.legend()

    # Delay Plot
    plt.subplot(2, 1, 2)
    plt.plot(frequencies, ext('orig_delay'), marker='d', linestyle='--', color='goldenrod', label="Unaltered Delay")
    plt.plot(frequencies, ext('fixed_delay'), marker='d', linestyle='-', color='darkorange', label=label_text)
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.3)
    plt.title("Calculated Inter-Channel Delay (+ means Right is lagging, - means Left is lagging)")
    plt.ylabel("Delay [µs]")
    plt.xlabel("Frequency (Hz)")
    plt.grid(True, which="both", ls="--")
    plt.xscale('linear')
    plt.legend()

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    analyze_adc_with_corrections(TEST_DIRECTORY, CORRECTION_DELAY_US, CORRECTION_GAIN_DB, FIR_TAPS)