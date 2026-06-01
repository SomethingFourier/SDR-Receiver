import socket
import struct
import time
from quisk.quisk_hardware_model import Hardware as BaseHardware
from quisk import _quisk as QS

HEADER_FMT = "<IBBBBIIIHH"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
MAGIC = 0x30445541

class Hardware(BaseHardware):
    def __init__(self, app, conf):
        BaseHardware.__init__(self, app, conf)
        
        # Configure Quisk to expect 3-byte (24-bit) little-endian samples
        self.InitSamples(3, 0)
        
        self.sock = None
        self.rp2350_ip = getattr(self.conf, "rp2350_ip", "192.168.1.5")
        self.udp_port = getattr(self.conf, "rp2350_port", 4951)
        self.bind_ip = getattr(self.conf, "bind_ip", "0.0.0.0")
        self.last_hello_time = 0

    def open(self):
        # Open the UDP socket
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind((self.bind_ip, self.udp_port))
            self.sock.setblocking(False)
            return f"RP2350 Ethernet Audio (Listening on {self.bind_ip}:{self.udp_port})"
        except Exception as e:
            return f"Error opening socket: {e}"

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def StartSamples(self):
        # Called from the sound thread when Quisk starts receiving samples.
        # We send the HELLO packet here to initiate the Unicast stream.
        if self.sock and self.rp2350_ip:
            try:
                self.sock.sendto(b"HELLO", (self.rp2350_ip, self.udp_port))
                self.last_hello_time = time.time()
                print(f"Sent initial HELLO packet to RP2350 at {self.rp2350_ip}:{self.udp_port}")
            except Exception as e:
                print(f"Warning: Failed to send HELLO packet to {self.rp2350_ip}: {e}")

    def StopSamples(self):
        pass

    def GetRxSamples(self):
        # Called frequently from the sound thread to poll for hardware samples.
        if not self.sock:
            return
            
        now = time.time()
        if self.rp2350_ip and now - self.last_hello_time > 1.0:
            try:
                self.sock.sendto(b"HELLO", (self.rp2350_ip, self.udp_port))
            except Exception:
                pass
            self.last_hello_time = now
            
        while True:
            try:
                packet, addr = self.sock.recvfrom(4096)
                
                if len(packet) >= HEADER_SIZE:
                    fields = struct.unpack_from(HEADER_FMT, packet)
                    if fields[0] == MAGIC:
                        # Magic matches, valid packet
                        frame_count = fields[8]
                        channels = fields[2]
                        bytes_per_sample = fields[3]
                        
                        expected_payload = frame_count * channels * bytes_per_sample
                        payload = packet[HEADER_SIZE:]
                        
                        if len(payload) == expected_payload:
                            # Pass the raw 24-bit interleaved I/Q payload directly to Quisk
                            self.AddRxSamples(payload)
                        else:
                            self.GotReadError(True, f"Payload size mismatch: expected {expected_payload}, got {len(payload)}")
            except BlockingIOError:
                # No more packets in the UDP buffer
                break
            except Exception as e:
                self.GotReadError(True, f"UDP Receive Error: {e}")
                break
