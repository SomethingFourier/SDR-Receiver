# RP2350 Pico SDK Template

This is a complete template for developing with the Raspberry Pi RP2350 microcontroller using the Pico SDK.

## Project Structure

```
rp2350-template/
├── src/
│   └── main.c              # Main application code
├── CMakeLists.txt          # CMake configuration
├── build.sh                # Build and compile script
├── .gitignore              # Git ignore file
└── README.md               # This file
```

## Prerequisites

Before building, you need to install:

1. **Pico SDK**: Clone the pico-sdk repository
   ```bash
   git clone https://github.com/raspberrypi/pico-sdk.git
   export PICO_SDK_PATH=$(pwd)/pico-sdk
   ```

2. **ARM Toolchain**: GNU Arm Embedded Toolchain
   ```bash
   # Ubuntu/Debian
   sudo apt-get install cmake gcc-arm-none-eabi binutils-arm-none-eabi libstdc++-arm-none-eabi-newlib

   # macOS
   brew install cmake arm-none-eabi-gcc
   ```

3. **Build Tools**:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential cmake

   # macOS
   brew install cmake
   ```

4. **Optional - UF2 Converter** (for UF2 generation):
   ```bash
   cargo install elf2uf2-rs
   ```

## Setup

1. Clone or download this template

2. Set the PICO_SDK_PATH environment variable:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```
   
   Add to your `~/.bashrc` or `~/.zshrc` for persistence:
   ```bash
   echo 'export PICO_SDK_PATH=/path/to/pico-sdk' >> ~/.bashrc
   source ~/.bashrc
   ```

## Building

### Basic Build
```bash
chmod +x build.sh
./build.sh
```

### Clean Build
```bash
./build.sh clean
```

### Rebuild (clean + build)
```bash
./build.sh rebuild
```

The build script will:
- Verify all requirements are installed
- Create a `build/` directory
- Run CMake with RP2350-specific configurations
- Compile the project
- Generate UF2 file for flashing

## Output Files

After a successful build, the following files are generated in the `build/` directory:
- `rp2350_app.elf` - Executable ELF file
- `rp2350_app.uf2` - UF2 format (for flashing via USB)
- `rp2350_app.hex` - Intel HEX format
- `rp2350_app.bin` - Binary format
- `rp2350_app.map` - Memory map

## Flashing the RP2350

### Method 1: USB Bootsel Mode (Recommended)
1. Connect the RP2350 to your computer via USB
2. Press and hold the **BOOTSEL** button
3. Press and release the **RESET** button (or plug in the USB)
4. Release the **BOOTSEL** button
5. The RP2350 will appear as a mass storage device (RPI-RP2350)
6. Copy the UF2 file to the mounted drive:
   ```bash
   cp build/rp2350_app.uf2 /media/$(whoami)/RPI-RP2350/
   ```
   The board will automatically reboot and run your code

### Method 2: Using picotool
```bash
# Build and install picotool first
git clone https://github.com/raspberrypi/picotool.git
cd picotool
mkdir build && cd build
cmake ..
make
sudo make install

# Then flash
picotool load -v build/rp2350_app.uf2
```

## Serial Output

The template is configured to output via USB serial. To view output:

```bash
# Linux
screen /dev/ttyACM0 115200

# macOS
screen /dev/tty.usbmodem* 115200

# or use minicom, picocom, etc.
picocom /dev/ttyACM0 -b 115200
```

To exit screen: `Ctrl+A` then `Ctrl+X`

## Customization

### Adding Libraries

Edit `CMakeLists.txt` and add to the `target_link_libraries()` section:
```cmake
target_link_libraries(rp2350_app
    pico_stdlib
    hardware_spi      # Add SPI support
    hardware_i2c      # Add I2C support
    # ... add more as needed
)
```

### Changing GPIO Pins

In `src/main.c`, modify the `LED_PIN` definition:
```c
#define LED_PIN 25  // Change to your desired GPIO pin
```

### Disabling USB Serial

If you don't need USB serial, change in `CMakeLists.txt`:
```cmake
pico_enable_stdio_usb(rp2350_app 0)  # Disable USB
pico_enable_stdio_uart(rp2350_app 1) # Enable UART on GPIO 0-1
```

## Troubleshooting

**Build fails with CMake error:**
- Ensure PICO_SDK_PATH is set correctly
- Verify the ARM toolchain is installed
- Try: `./build.sh clean` then `./build.sh`

**Can't find /dev/ttyACM0:**
- Try: `ls /dev/tty*` to see available ports
- May be `/dev/ttyUSB0`, `/dev/ttyS0`, or `/dev/tty.usbmodem*`
- Ensure the RP2350 is connected and recognized

**elf2uf2 not found:**
- The UF2 file should be generated automatically by CMake
- If not, install `elf2uf2-rs`: `cargo install elf2uf2-rs`

**Permission denied when mounting:**
- Create a udev rule or use `sudo` with `sudo mount` commands

## Resources

- [Official Pico SDK Documentation](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Pico SDK GitHub](https://github.com/raspberrypi/pico-sdk)
- [Getting Started with RP2350](https://www.raspberrypi.com/documentation/microcontrollers/)

## License

This template is provided as-is for use with the Pico SDK, which is licensed under the BSD 3-Clause License.
