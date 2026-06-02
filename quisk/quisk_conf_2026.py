# Quisk configuration for the Intro-to-CAD-2026 Pico SDR board
#
# Student Lab 5 copy: place this next to your lab files and use it as the
# host-side Quisk configuration for the CDC control protocol.

from __future__ import print_function
from __future__ import absolute_import
from __future__ import division

import os
import math
import fractions
import serial
import serial.tools.list_ports
import time
from quisk_hardware_model import Hardware as BaseHardware

name_of_sound_capt = "pulse"
name_of_sound_play = "pulse"

sample_rate = 48000

openradio_lower = 3_800_000
openradio_upper = 30_000_000

class Hardware(BaseHardware):
    def open(self):
        """Open the serial connection to the Pico and configure it."""
        baud = 115200
        # Prefer VID:PID detection so the correct port is found even when the
        # Pico Debug Stack (picoprobe) occupies a lower-numbered ACM device.
        port_device = None
        for info in serial.tools.list_ports.comports():
            if (info.vid, info.pid) in ((0xcafe, 0x4011), (0xcafe, 0x4010)):
                port_device = info.device
                break
        if port_device is None:
            import glob
            fallbacks = ["/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3"]
            fallbacks += glob.glob("/dev/cu.usbmodem*")
            for p in fallbacks:
                if os.path.exists(p):
                    port_device = p
                    break
        if port_device is None:
            raise serial.serialutil.SerialException(
                "Pico not found (VID:PID cafe:4011 or /dev/ttyACM0-3)")
        self.or_serial = serial.Serial(port_device, baud, timeout=3)
        print("Opened", port_device)

        self.or_serial.write(b'\x03')
        time.sleep(0.1)
        self.or_serial.write(b'\x04')
        self.or_serial.reset_input_buffer()

        self.or_serial.timeout = 0.2
        deadline = time.time() + 6.0
        while time.time() < deadline:
            line = self.or_serial.readline()
            if b'SDR ready' in line:
                break
        self.or_serial.timeout = 3

        version = str(self._get_parameter("VER"))
        print("Pico firmware:", version)

        xtal_raw = self._get_parameter("XTAL")
        try:
            self._crystal_freq = float(xtal_raw)
            if self._crystal_freq <= 0:
                raise ValueError("invalid")
        except (ValueError, TypeError):
            self._crystal_freq = 24_576_000.0
        print("Crystal freq: %.3f Hz" % self._crystal_freq)

        mode_raw = self._get_parameter("MODE")
        if mode_raw == -1:
            self._johnson_counter = False
        else:
            self._johnson_counter = (str(mode_raw).strip().upper() == "JOHNSON")
        print("Mixer mode: %s" % ("JOHNSON counter (÷4)" if self._johnson_counter else "DIRECT"))

        self._set_parameter("RATE", str(sample_rate))

        self._golden_status = "PLL: ready"
        self._last_lo = None
        self._last_tune = None
        self._pending_vfo_update = True  # Force UI sync after open

        return version + ". Capture from %s at %d Hz." % (
            self.conf.name_of_sound_capt, sample_rate)

    def close(self):
        self.or_serial.close()

    def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
        tune = max(openradio_lower, min(openradio_upper, tune))
        
        # In JOHNSON mode the Si5351a must output 4× the logical LO frequency
        multiplier = 4 if self._johnson_counter else 1
        si5351_hz = int(round(vfo * multiplier))
        self._send(f"FREQ,{si5351_hz}")
        
        self._readline() # read back the requested frequency
        ok_line = self._readline().decode(errors='replace').strip() # read the OK response
        
        parts = ok_line.split(",")
        signed_offset = 0
        if len(parts) >= 3 and parts[0] == "OK":
            ptype = parts[1]
            try:
                signed_offset = int(parts[2])
            except ValueError:
                signed_offset = 0
                
            if ptype == "G":
                self._golden_status = "PLL  {:+d} Hz".format(signed_offset)
            elif ptype == "F":
                self._golden_status = "frac  (exact freq)"
            else:
                self._golden_status = "fallback  (VCO out of spec)"
                
        self._last_tune = int(tune)
        self._last_lo = int(vfo)
        return self._last_tune, self._last_lo

    def ReturnFrequency(self):
        if getattr(self, '_pending_vfo_update', False):
            if self._last_tune is not None and self._last_lo is not None:
                self._pending_vfo_update = False
                return self._last_tune, self._last_lo
        return None, None

    def HeartBeat(self):
        try:
            self.application.StatusScreen(self._golden_status)
        except Exception:
            pass

    def _send(self, line):
        self.or_serial.write((line + "\n").encode())

    def _readline(self):
        return self.or_serial.readline()

    def _get_parameter(self, cmd):
        self._send(cmd)
        return self._get_argument()

    def _set_parameter(self, cmd, arg):
        self._send(cmd + "," + arg)
        for _ in range(5):
            data = self._readline()
            if len(data) == 0 or data.startswith(b'OK'):
                break
        return True

    def _get_argument(self):
        for _ in range(5):
            data = self._readline()
            if len(data) == 0:
                return -1
            if data.startswith(b'OK'):
                continue
            if data.find(b',') != -1:
                value = data.split(b',')[1].rstrip(b'\r\n')
                self._readline()
                return value.decode(errors='replace')
        return -1
