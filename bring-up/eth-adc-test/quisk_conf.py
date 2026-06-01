import os
import sys

# Quisk changes its working directory on startup, so we must add our dir to sys.path
config_dir = "/home/pkcubed/Documents/GitHub/SDR-Receiver/bring-up/eth-adc-test"
if config_dir not in sys.path:
    sys.path.insert(0, config_dir)

# Tell Quisk which hardware module to use
import quisk_hardware_rp2350
Hardware = quisk_hardware_rp2350.Hardware
quisk_hardware = quisk_hardware_rp2350
hardware_file_name = ""

# Configure the IP address of the RP2350 (Replace this with the actual IP from the serial monitor)
rp2350_ip = "192.168.1.5"
rp2350_port = 4951

# Audio rate coming from the ADC (Must match the hardware's actual sample rate)
sample_rate = 48000
name_of_sound_capt = "" # Empty string tells Quisk to use the Python hardware module instead of ALSA
name_of_sound_play = "pulse"

# Provide a simple band config so Quisk starts successfully
band_info = {
    '40': {
        'edge_lower': 7000000,
        'edge_upper': 7300000,
    }
}
default_band = '40'
with open("/tmp/quisk_conf_load.txt", "w") as f:
    f.write("quisk_conf.py loaded!\n")
