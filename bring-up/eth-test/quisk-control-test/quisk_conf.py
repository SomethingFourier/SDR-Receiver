import os
import sys

# Add the current directory to sys.path so Quisk can find hardware.py
sys.path.append(os.path.dirname(__file__))

# Specify our custom hardware module
# Quisk will look for a class named 'Hardware' inside this file.
import hardware

# Some basic default configurations to allow Quisk to start without
# an audio card connected, since we are just testing the control logic.
# Once you hook up actual audio, you may need to adjust these.
sample_rate = 48000
name_of_sound_capt = ""
name_of_sound_play = ""

channel_i = 0
channel_q = 1

# VFO setup (Hz)
default_tune = 7074000
