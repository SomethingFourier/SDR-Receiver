#!/bin/bash

##############################################################################
# Quick Setup Script for RP2350 Pico SDK Template
# This script helps set up the development environment
##############################################################################

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_header() {
    echo -e "\n${GREEN}========================================${NC}"
    echo -e "${GREEN}$1${NC}"
    echo -e "${GREEN}========================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}" >&2
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

check_command() {
    if command -v $1 &> /dev/null; then
        print_success "$1 is installed"
        return 0
    else
        print_error "$1 is not installed"
        return 1
    fi
}

print_header "RP2350 Development Environment Setup"

echo "This script will help you set up the RP2350 Pico SDK development environment."
echo ""

# Check for required tools
print_header "Checking for Required Tools"

MISSING_TOOLS=false

if ! check_command cmake; then
    MISSING_TOOLS=true
    echo "Install: sudo apt-get install cmake (Ubuntu/Debian)"
    echo "         brew install cmake (macOS)"
fi

if ! check_command arm-none-eabi-gcc; then
    MISSING_TOOLS=true
    echo "Install: sudo apt-get install gcc-arm-none-eabi (Ubuntu/Debian)"
    echo "         brew install arm-none-eabi-gcc (macOS)"
fi

if ! check_command arm-none-eabi-g++; then
    MISSING_TOOLS=true
    echo "Install: sudo apt-get install g++-arm-none-eabi (Ubuntu/Debian)"
fi

if [ "$MISSING_TOOLS" = true ]; then
    print_warning "Some tools are missing. Please install them above."
    exit 1
fi

# Check PICO_SDK_PATH
print_header "Checking Pico SDK"

if [ -z "$PICO_SDK_PATH" ]; then
    print_error "PICO_SDK_PATH is not set"
    echo ""
    echo "To set up Pico SDK:"
    echo "1. Clone the SDK:"
    echo "   git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk"
    echo ""
    echo "2. Set the environment variable:"
    echo "   export PICO_SDK_PATH=\$HOME/pico-sdk"
    echo ""
    echo "3. Make it permanent by adding to ~/.bashrc or ~/.zshrc:"
    echo "   echo 'export PICO_SDK_PATH=\$HOME/pico-sdk' >> ~/.bashrc"
    echo "   source ~/.bashrc"
    exit 1
else
    if [ -d "$PICO_SDK_PATH" ]; then
        print_success "PICO_SDK_PATH is set: $PICO_SDK_PATH"
    else
        print_error "PICO_SDK_PATH directory does not exist: $PICO_SDK_PATH"
        exit 1
    fi
fi

# Check optional tools
print_header "Checking Optional Tools"

if check_command elf2uf2-rs; then
    echo "(UF2 conversion tool - recommended but not required)"
else
    print_warning "elf2uf2-rs not found (optional)"
    echo "Install with: cargo install elf2uf2-rs"
fi

if check_command picotool; then
    echo "(Flash tool - optional)"
else
    print_warning "picotool not found (optional)"
    echo "This can be installed from: https://github.com/raspberrypi/picotool"
fi

# All checks passed
print_header "Setup Complete!"

echo "Everything is ready to build! To get started:"
echo ""
echo "  1. Modify src/main.c with your code"
echo "  2. Run the build script:"
echo "     ./build.sh"
echo ""
echo "After building, flash the RP2350:"
echo "  1. Hold BOOTSEL and press RESET"
echo "  2. Copy build/rp2350_app.uf2 to the mounted RPI-RP2350 drive"
echo ""
echo "For more information, see README.md"
echo ""
