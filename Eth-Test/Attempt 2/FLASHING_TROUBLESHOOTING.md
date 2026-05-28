# RP2350 UF2 Flashing Troubleshooting Guide

## UF2 File Status ✓
The `ethernet_app.uf2` file is **correctly formatted**:
- Family ID: `0x540ddf62` (RP2350) ✓
- Load address: `0x10000000` (Flash start) ✓
- Total blocks: 159 ✓
- Data integrity: Valid ✓

## Standard Flashing Procedure

### Method 1: Drag & Drop (Recommended)
1. **Enter bootloader mode:**
   - Connect RP2350 to USB
   - Hold down the **BOOT** button (usually labeled "BOOT" or "B")
   - While holding BOOT, briefly press the **RESET** button (or power cycle)
   - Release BOOT
   - Your PC should detect `RPI-RP2350` USB drive

2. **Verify USB drive appears:**
   ```bash
   # macOS/Linux
   ls /Volumes/*RP2* 2>/dev/null || ls /media/$USER/*RP2* 2>/dev/null
   
   # or check dmesg
   dmesg | grep -i RP2350 | tail -5
   ```

3. **Drag and drop UF2:**
   - Copy `ethernet_app.uf2` to the `RPI-RP2350` drive
   - Or: `cp ethernet_app.uf2 /media/<user>/RPI-RP2350/`
   - Board should reboot automatically

### Method 2: Command-line Copy
```bash
# Find the mount point
MOUNT=$(lsblk -o NAME,MOUNTPOINT | grep -i RP2350 | awk '{print $2}')

# Copy UF2
cp ethernet_app.uf2 "$MOUNT/"

# Eject safely
eject "$MOUNT"
```

## Issues & Solutions

### Issue 1: USB Drive Not Appearing
**Symptom:** After BOOT+RESET, no `RPI-RP2350` drive appears

**Solutions:**
1. **Check USB cable:** Use a **data cable** (not charge-only), preferably USB-C or micro-USB
2. **Try different USB port:** Connect to a different USB port on the computer
3. **Check USB hub:** Try connecting directly to computer (not through hub)
4. **Timing issue:** Hold BOOT slightly longer (2-3 seconds before RESET)
5. **Power issue:** Provide external 5V if board has slow USB enumeration
6. **Linux permis sions:** 
   ```bash
   sudo chmod 666 /dev/ttyACM* 2>/dev/null
   ```

### Issue 2: "Unable to Copy" or "Disk Full" Error
**Symptom:** Drive shows insufficient space or refuses copy

**Solutions:**
1. **Don't copy using drag-drop:** Use command line instead:
   ```bash
   cp ethernet_app.uf2 /media/$USER/RPI-RP2350/
   sync  # Force flush to drive
   ```
2. **Eject before copy:** Give bootloader time to prepare:
   ```bash
   eject /media/$USER/RPI-RP2350/
   sleep 1
   ```
3. **Try a different OS:** Windows, macOS, or Linux may have different USB handling

### Issue 3: UF2 Copied But Old Program Still Runs
**Symptom:** LED blinks or old behavior, not the new ethernet program

**Possible causes:**
- ① **Bootloader didn't erase flash** (firmware conflict)
- ② **Board in wrong pin configuration** (GPIO not set correctly)
- ③ **RP2350 has dual flash** (boot from wrong bank)
- ④ **Previous program has bootloader** (multi-stage boot)

**Debug steps:**
1. **Check if board is alive:**
   ```bash
   # Monitor serial console at 115200 baud
   screen /dev/ttyUSB0 115200
   # or miniterm
   python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
   ```
   **Look for:**
   - `Interface UP` message
   - `Starting Ethernet...`
   - Any printf output from main.c

2. **Force full erase before flashing:**
   - Install `picotool`: `pip install picotool`
   - Enter bootloader mode
   - Run: `picotool erase -a` (erase all flash)
   - Then drag-drop UF2

3. **Check LED on GPIO4:**
   - LED should blink at **1 Hz** (on for 500ms, off for 500ms)
   - If LED blinks differently, the old program is running
   - If no LED blink, new program might be running but not in bootloader mode

### Issue 4: "Operation Not Permitted" Error
**Symptom:** macOS/Linux shows permission denied when copying

**Solution:**
```bash
# Check mount permissions
ls -la /media/$USER/RPI-RP2350/
# You might need to use sudo or change permissions
sudo cp ethernet_app.uf2 /media/$USER/RPI-RP2350/
```

### Issue 5: Board Crashes or Bootloops
**Symptom:** Board reboots repeatedly, no stable USB, or LED goes crazy

**Causes:**
- Voltage regulation issue (vreg might need 1.3V but board can't handle it)
- GPIO conflicts (Ethernet pins interfere with other hardware)
- Missing XTAL or wrong clock source

**Solutions:**
1. **Lower CPU clock in source (temporary test):**
   - Edit `src/main.c`: Change `set_sys_clock_khz(300000, ...)` to `set_sys_clock_khz(200000, ...)`
   - Rebuild: `cd build && make -j4`
   
2. **Check pin conflicts:**
   - Verify PIO pins don't conflict with LED (GPIO4 is safe)
   - Check that MDIO pins (GPIO16, GPIO17) have pullups
   - Ensure GPIO18 (reset) is properly wired

## Advanced Debugging

### Verify Binary Before Flashing
```bash
cd build

# Check binary size
ls -lh ethernet_app.bin ethernet_app.uf2

# Verify binary is not corrupted
md5sum ethernet_app.bin
file ethernet_app.uf2

# Parse UF2 blocks
arm-none-eabi-objdump -h ethernet_app.elf | grep -A1 "\.text"
```

### Serial Monitor Output
```bash
# Linux/macOS
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200

# or with picocom
picocom -b 115200 /dev/ttyUSB0

# Expected output after boot:
# "Initializing lwIP..."
# "Interface UP: 192.168.1.100"
# "HTTP server running..."
```

### Force Bootloader Entry on Windows

```batch
REM On Windows, RP2350 bootloader is automatic on first power-on
REM If stuck, try:
1. Hold BOOT button
2. Power cycle (unplug/plug USB)
3. Release BOOT
4. Drive should appear as "RPI-RP2350"
```

## Verification After Flashing

### Test 1: LED Blink
✓ **Expected:** GPIO4 LED blinks at ~1 Hz (steady pulse)  
✗ **Failure:** No LED, or blinks differently → old program still running

### Test 2: Serial Console
✓ **Expected:** USB serial at 115200 baud shows `Interface UP` message  
✗ **Failure:** No data or garbage → bootloader issue

### Test 3: Network Ping
✓ **Expected:** `ping 192.168.1.100` succeeds  
✗ **Failure:** No response → Ethernet not initialized

### Test 4: HTTP Server
✓ **Expected:** `curl http://192.168.1.100` returns HTML  
✗ **Failure:** Connection refused → server not listening

## If All Else Fails

### Option 1: Rebuild from source
```bash
cd build
rm -rf *
cmake -DPICO_BOARD=pico2 ..
make -j4
```
Then try flashing again.

###  Option 2: Use alternative programming method
```bash
# If bootloader USB fails, try JTAG/SWD programmer:
openocd -f interface/jlink.cfg -f target/rp2350.cfg -c "init" -c "halt" -c "program {ethernet_app.elf} verify reset exit"
```

### Option 3: Check hardware rev
```bash
# Print RP2350 revision ID from bootloader
# (requires custom debug script)
```

## Reference

- **RP2350 Datasheet:** [raspberrypi.com/datasheets](https://www.raspberrypi.com/datasheets)
- **Pico SDK:** [github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- **UF2 Spec:** [github.com/microsoft/uf2](https://github.com/microsoft/uf2)
- **lwIP Docs:** [savannah.nongnu.org/lwip](https://savannah.nongnu.org/lwip)

---

**Last updated:** Build with CMake UF2 generation  
**Status:** Verified UF2 format is correct; issue likely in bootloader USB enumeration
