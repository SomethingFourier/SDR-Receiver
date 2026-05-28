from quisk import hardware
import socket

# This class must be named "Hardware" and inherit from quisk.hardware.Hardware
class Hardware(hardware.Hardware):
    def __init__(self, app, conf):
        super().__init__(app, conf)
        self.rp2350_ip = "192.168.1.100"
        self.rp2350_port = 50000
        
        # Open a UDP socket for communication
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(0.5) # 500ms timeout for response
        
    def ChangeFrequency(self, tune, vfo, source='', tune_return=None):
        """
        Quisk calls this method when the user changes the frequency.
        tune: Requested tuning frequency in Hz
        vfo: VFO frequency (often same as tune for simple SDRs)
        """
        # We only really care about the 'tune' frequency for the SDR.
        print(f"Quisk requesting tune to: {tune} Hz")
        
        try:
            # Send the TUNE command to the RP2350
            msg = f"TUNE {tune}\n".encode('ascii')
            self.sock.sendto(msg, (self.rp2350_ip, self.rp2350_port))
            
            # Wait for the TUNED response
            data, addr = self.sock.recvfrom(1024)
            response = data.decode('ascii').strip()
            
            if response.startswith("TUNED "):
                actual_tune = int(response.split()[1])
                print(f"RP2350 successfully tuned to: {actual_tune} Hz")
                
                # Quisk expects a tuple of (actual_tune, actual_vfo) to be returned
                # or updated via tune_return.
                # If tune_return is a list, we can append to it, or just return the tuple.
                return (actual_tune, actual_tune)
            else:
                print(f"Unexpected response from RP2350: {response}")
                
        except socket.timeout:
            print("Timeout waiting for response from RP2350.")
        except Exception as e:
            print(f"Error communicating with RP2350: {e}")
            
        # If we failed to get a response, return the requested tune so Quisk doesn't break,
        # but the UI might not reflect the hardware's true state.
        return (tune, vfo)
