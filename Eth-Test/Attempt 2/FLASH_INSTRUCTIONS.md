# RP2350 Ethernet Demo - Flashing Instructions

## Quick Start - Flash the UF2

### Prerequisites
- RP2350 board with LAN8720 Ethernet PHY
- USB cable
- Computer with access to the board's USB mass storage

### Flashing Steps

1. **Put RP2350 in bootloader mode:**
   - Hold the **BOOTSEL** button
   - Plug the USB cable into the RP2350
   - Release BOOTSEL
   - The board should appear as **RPI-RP350** (or similar) USB drive

2. **Drag and drop the UF2 file:**
   ```bash
   cp ethernet_app.uf2 /Volumes/RPI-RP350/
   # or on Linux:
   cp ethernet_app.uf2 /media/RPI-RP350/
   ```
   
   Alternatively, use your file manager to drag `ethernet_app.uf2` to the RPI-RP350 drive.

3. **Wait for flashing to complete:**
   - The board will automatically eject and reboot
   - LED on GPIO4 should start blinking at 1 Hz (500ms on, 500ms off)

## Verify the Flash

### via USB Serial (Recommended)

Connect to the USB serial console (115200 baud):

```bash
# macOS/Linux
screen /dev/tty.usbmodem* 115200

# Or with minicom
minicom -D /dev/ttyACM0 -b 115200

# Or with picocom
picocom /dev/ttyACM0 -b 115200
```

You should see:
```
========================================
  RP2350 PIO Ethernet Demo
  System Clock: 300 MHz
  Static IP: 192.168.1.100
========================================

[CLOCK] Setting system clock to 300 MHz...
[CLOCK] sys_clk_hz = 300000000
[GPIO] Initializing LED on GPIO 4...
[LWIP] Initializing lwIP stack...
[ETH] Initializing RMII Ethernet interface...
[NET] Interface UP: IP 192.168.1.100, Netmask 255.255.255.0, Gateway 192.168.1.1
[ETH] Ethernet link detected
```

### via Network Ping

From another computer on the network:

```bash
ping 192.168.1.100
```

Expected output:
```
PING 192.168.1.100 (192.168.1.100) 56(84) bytes of data.
64 bytes from 192.168.1.100: icmp_seq=1 ttl=64 time=2.34 ms
64 bytes from 192.168.1.100: icmp_seq=2 ttl=64 time=1.89 ms
```

### via LED Indicator

- **Blinking at 1 Hz** = Firmware loaded successfully
- **No blink** = Flash may have failed, re-enter bootloader mode and retry
- **Solid LED** = System halted (check serial console for errors)

## Troubleshooting

### Board won't show up as USB drive after pressing BOOTSEL

- Try a different USB cable (some cables are charge-only)
- Try a different USB port (preferably a 2.0 port on older machines)
- Hold BOOTSEL a bit longer before plugging in
- If using Raspberry Pi Pico board definition, try booting again or reflashing the original Pico firmware first

### No LED blink, no serial output

1. Check all GPIO connections (especially GPIO4 for LED)
2. Verify USB power delivery - try a powered USB hub
3. Try reflashing the UF2 file
4. Try a different USB serial terminal application

### Network not connecting

1. **Check Ethernet wiring:**
   - Verify all 8 wires are crimped correctly in RJ45
   - Test with known-good Ethernet cable first
   - Check for loose connections

2. **Check GPIO mapping:**
   - Verify pins match the pinout in this document
   - Check that GPIO 23 (REF_CLK) toggles at ~50 MHz with oscilloscope (optional)
   - Check MDIO (GPIO16/17) toggling when PHY scans for address (serial log shows "PHY found at address X")

3. **Check PHY reset:**
   - GPIO18 should go low then high during startup
   - Check that RC network on LAN8720 RST pin is present (typically 100nF cap + 47kΩ pull-up)

4. **Check power:**
   - LAN8720 and RP2350 both require stable 3.3V
   - Add 100nF decoupling caps near each IC

5. **Serial log clues:**
   - "Failed to find a PHY register" = MDIO lines not connected or PHY not responding
   - "Link down" = Physical connection not detected

## Building from Source

If you need to rebuild:

```bash
cd Attempt\ 2
mkdir -p build
cd build
cmake -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 ..
make -j4
python3 /tmp/bin2uf2.py ethernet_app.bin ethernet_app.uf2
```

The `ethernet_app.uf2` file will be ready to flash.

## Technical Details

| Parameter | Value |
|-----------|-------|
| Firmware Size | 80 KB (UF2) |
| RAM Used | ~16 KB (lwIP + buffers) |
| System Clock | 300 MHz |
| UART Baud Rate | 115200 baud |
| IP Address | 192.168.1.100 |
| Netmask | 255.255.255.0 |
| Gateway | 192.168.1.1 |
| LED GPIO | 4 (1 Hz blink) |
| Ethernet Speed | 100 Mbps (RMII) |

---

**Firmware built:** May 27, 2025  
**Target:** RP2350 with LAN8720 PHY  
**Family ID:** 0x540ddf62 (RP2350)
