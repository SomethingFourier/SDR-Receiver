#!/bin/bash

##############################################################################
# RP2350 Flash Script
# Automatically detects mounted RPI-RP2350 and flashes the UF2 file
##############################################################################

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

print_header() {
    echo -e "\n${BLUE}════════════════════════════════════════${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}════════════════════════════════════════${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}" >&2
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Find UF2 file
print_header "RP2350 Flash Utility"

UF2_FILE=$(find "$BUILD_DIR" -name "*.uf2" -type f 2>/dev/null | head -n1)

if [ -z "$UF2_FILE" ]; then
    print_error "No .uf2 file found in build directory"
    echo "Please build the project first: ./build.sh"
    exit 1
fi

print_success "Found UF2 file: $UF2_FILE"
echo "File size: $(du -h "$UF2_FILE" | cut -f1)"

# Find mounted RP2350
print_info "Searching for mounted RP2350 device..."

MOUNT_POINT=""

# Try multiple ways to find the mount point
if mount | grep -q 'RPI-RP2350'; then
    MOUNT_POINT=$(mount | grep 'RPI-RP2350' | awk '{print $3}' | head -n1)
elif mount | grep -q 'RPI-RP2'; then
    MOUNT_POINT=$(mount | grep 'RPI-RP2' | awk '{print $3}' | head -n1)
fi

if [ -z "$MOUNT_POINT" ]; then
    print_error "RP2350 not found as mounted drive"
    echo ""
    echo "To put your RP2350 in BOOTSEL mode:"
    echo "  1. Hold down the BOOTSEL button"
    echo "  2. Press and release the RESET button (or connect USB while holding BOOTSEL)"
    echo "  3. Release the BOOTSEL button"
    echo ""
    echo "The device should appear as 'RPI-RP2350' in your file manager or mount output."
    echo ""
    echo "Current mounts:"
    mount | grep -i rpi || echo "No RPI devices found"
    exit 1
fi

print_success "Found RP2350 at: $MOUNT_POINT"

# Confirm before flashing
echo ""
echo "Ready to flash the following:"
echo "  Device:   $MOUNT_POINT"
echo "  File:     $UF2_FILE"
echo "  Size:     $(du -h "$UF2_FILE" | cut -f1)"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 1
fi

# Copy file
print_header "Flashing..."

if cp "$UF2_FILE" "$MOUNT_POINT/"; then
    print_success "File copied successfully"
    echo ""
    echo "The RP2350 should now automatically:"
    echo "  1. Eject from your computer"
    echo "  2. Boot into the new firmware"
    echo "  3. Appear as /dev/ttyACM0 (or similar) for serial output"
    echo ""
    echo "To view serial output:"
    echo "  screen /dev/ttyACM0 115200"
    echo "  OR"
    echo "  picocom /dev/ttyACM0 -b 115200"
else
    print_error "Failed to copy file"
    exit 1
fi

print_header "Flash complete!"
echo -e "${GREEN}Your RP2350 is running the new firmware!${NC}\n"
