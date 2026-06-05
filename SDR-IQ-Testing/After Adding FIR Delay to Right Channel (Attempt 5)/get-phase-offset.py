import numpy as np
from scipy.io import wavfile
from scipy import signal

def calculate_phase_offset(wav_path):
    """
    Reads a stereo WAV file and calculates the phase offset between 
    the left and right channels in microseconds.
    """
    # 1. Read the audio file
    sample_rate, data = wavfile.read(wav_path)
    
    if data.ndim != 2 or data.shape[1] != 2:
        raise ValueError("The provided WAV file is not stereo.")

    # 2. Extract channels and convert to float64 to prevent overflow
    left_channel = data[:, 0].astype(np.float64)
    right_channel = data[:, 1].astype(np.float64)

    # 3. Compute cross-correlation using FFT for high performance
    # This slides the right channel across the left channel to find the best match
    correlation = signal.correlate(left_channel, right_channel, mode='full', method='fft')

    # 4. Find the discrete peak (sample-level accuracy)
    peak_idx = np.argmax(correlation)

    # 5. Apply Parabolic Interpolation for sub-sample accuracy
    # This finds the "true" peak between the discrete samples
    if 0 < peak_idx < len(correlation) - 1:
        y_minus = correlation[peak_idx - 1]
        y_0 = correlation[peak_idx]
        y_plus = correlation[peak_idx + 1]

        # Denominator of the parabolic vertex formula
        denominator = y_minus - 2 * y_0 + y_plus
        
        if denominator == 0:
            shift = 0.0
        else:
            shift = 0.5 * (y_minus - y_plus) / denominator
    else:
        shift = 0.0

    # 6. Calculate the true lag in samples
    # The zero-delay point in a 'full' mode correlation is at len(right_channel) - 1
    zero_lag_idx = len(right_channel) - 1
    true_lag_samples = (peak_idx + shift) - zero_lag_idx

    # 7. Convert samples to microseconds
    offset_seconds = true_lag_samples / sample_rate
    offset_microseconds = offset_seconds * 1_000_000

    return offset_microseconds

if __name__ == "__main__":
    # Replace with the path to your stereo WAV file
    FILE_PATH = "sdr-test-4.02kHz.wav"
    
    try:
        offset_us = calculate_phase_offset(FILE_PATH)
        
        # Formatting the output
        if offset_us > 0:
            print(f"Left channel is DELAYED by {abs(offset_us):.3f} microseconds relative to the right.")
        elif offset_us < 0:
            print(f"Left channel is AHEAD by {abs(offset_us):.3f} microseconds relative to the right.")
        else:
            print("Both channels are perfectly in phase (0.000 microseconds offset).")
            
    except Exception as e:
        print(f"Error: {e}")
