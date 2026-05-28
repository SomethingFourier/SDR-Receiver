#!/bin/bash

##############################################################################
# RP2350 Pico SDK Build Script
# This script builds the RP2350 project and generates a UF2 file for flashing
##############################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

# Project directories
BUILD_DIR="${SCRIPT_DIR}/build"
SRC_DIR="${SCRIPT_DIR}/src"

##############################################################################
# Functions
##############################################################################

print_header() {
    printf "\n${GREEN}========================================${NC}\n"
    printf "${GREEN}%s${NC}\n" "$1"
    printf "${GREEN}========================================${NC}\n\n"
}

print_error() {
    printf "${RED}Error: %s${NC}\n" "$1" >&2
}

print_warning() {
    printf "${YELLOW}Warning: %s${NC}\n" "$1"
}

check_requirements() {
    print_header "Checking Requirements"
    
    # Check if PICO_SDK_PATH is set
    if [ -z "$PICO_SDK_PATH" ]; then
        print_error "PICO_SDK_PATH environment variable is not set"
        printf "Please set PICO_SDK_PATH to your Pico SDK installation directory\n"
        printf "Example: export PICO_SDK_PATH=/path/to/pico-sdk\n"
        exit 1
    fi
    
    if [ ! -d "$PICO_SDK_PATH" ]; then
        print_error "PICO_SDK_PATH directory does not exist: $PICO_SDK_PATH"
        exit 1
    fi
    
    printf "✓ PICO_SDK_PATH: %s\n" "$PICO_SDK_PATH"
    
    # Check for required tools
    for tool in cmake arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-ar arm-none-eabi-ranlib; do
        if ! command -v $tool >/dev/null 2>&1; then
            print_error "$tool is not installed"
            exit 1
        fi
        printf "✓ %s found\n" "$tool"
    done
}

clean_build() {
    print_header "Cleaning Previous Build"
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        printf "✓ Cleaned build directory\n"
    else
        printf "✓ No previous build found\n"
    fi
}

create_build_dir() {
    print_header "Creating Build Directory"
    
    mkdir -p "$BUILD_DIR"
    printf "✓ Build directory created: %s\n" "$BUILD_DIR"
}

run_cmake() {
    print_header "Running CMake"
    
    cd "$BUILD_DIR"
    
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DPICO_BOARD=pico2 \
          ..
    
    printf "✓ CMake configuration complete\n"
}

build_project() {
    print_header "Building Project"
    
    cd "$BUILD_DIR"
    make -j$(nproc)
    
    printf "✓ Build complete\n"
}

generate_uf2() {
    print_header "Generating UF2 File"
    
    # Find the .elf file
    ELF_FILE=$(find "$BUILD_DIR" -name "rp2350_app.elf" -type f | head -n1)
    
    if [ -z "$ELF_FILE" ]; then
        print_error "Could not find rp2350_app.elf file"
        exit 1
    fi
    
    # CMake should have generated a .uf2 file automatically
    UF2_FILE=$(find "$BUILD_DIR" -name "rp2350_app.uf2" -type f | head -n1)
    if [ -n "$UF2_FILE" ]; then
        printf "✓ UF2 file generated: %s\n" "$UF2_FILE"
    else
        print_warning "UF2 file was not created automatically. Checking for elf2uf2 tool..."
        
        if command -v elf2uf2-rs >/dev/null 2>&1; then
            elf2uf2-rs "$ELF_FILE" "$BUILD_DIR/rp2350_app.uf2"
            printf "✓ UF2 file generated: %s\n" "$BUILD_DIR/rp2350_app.uf2"
        elif command -v elf2uf2 >/dev/null 2>&1; then
            elf2uf2 "$ELF_FILE" "$BUILD_DIR/rp2350_app.uf2"
            printf "✓ UF2 file generated: %s\n" "$BUILD_DIR/rp2350_app.uf2"
        else
            print_error "Could not generate UF2 file. Install elf2uf2-rs: cargo install elf2uf2-rs"
            printf "The ELF file is available at: %s\n" "$ELF_FILE"
        fi
    fi
}

print_summary() {
    print_header "Build Summary"
    
    printf "Build output directory: %s\n" "$BUILD_DIR"
    printf "\n"
    printf "Generated files:\n"
    
    if [ -f "$BUILD_DIR/rp2350_app.elf" ]; then
        printf "  ✓ %s/rp2350_app.elf\n" "$BUILD_DIR"
    fi
    
    if [ -f "$BUILD_DIR/rp2350_app.hex" ]; then
        printf "  ✓ %s/rp2350_app.hex\n" "$BUILD_DIR"
    fi
    
    if [ -f "$BUILD_DIR/rp2350_app.uf2" ]; then
        printf "  ✓ %s/rp2350_app.uf2\n" "$BUILD_DIR"
    fi
    
    if [ -f "$BUILD_DIR/rp2350_app.bin" ]; then
        printf "  ✓ %s/rp2350_app.bin\n" "$BUILD_DIR"
    fi
    
    printf "\n"
    printf "${GREEN}To flash the RP2350:${NC}\n"
    printf "1. Connect your RP2350 to your computer via USB\n"
    printf "2. Hold the BOOTSEL button and reset (or plug in)\n"
    printf "3. Copy the .uf2 file to the mounted drive:\n"
    printf "   cp %s/rp2350_app.uf2 /path/to/RPI-RP2350/\n" "$BUILD_DIR"
    printf "   OR use: sudo mount | grep RPI to find the mount point\n"
}

print_usage() {
    printf "Usage: %s [OPTIONS]\n" "$0"
    printf "\n"
    printf "Options:\n"
    printf "  clean      Clean build directory and exit\n"
    printf "  build      Build project (default action)\n"
    printf "  rebuild    Clean and build\n"
    printf "  help       Show this help message\n"
    printf "\n"
}

##############################################################################
# Main Script
##############################################################################

# Parse command line arguments
REBUILD=false
CLEAN_ONLY=false

while [ $# -gt 0 ]; do
    case $1 in
        clean)
            CLEAN_ONLY=true
            shift
            ;;
        rebuild)
            REBUILD=true
            shift
            ;;
        build)
            shift
            ;;
        help)
            print_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Main execution
printf "${GREEN}"
printf "╔════════════════════════════════════════════════╗\n"
printf "║  RP2350 Pico SDK Build Script                  ║\n"
printf "╚════════════════════════════════════════════════╝\n"
printf "${NC}"

check_requirements

if [ "$CLEAN_ONLY" = true ]; then
    clean_build
    printf "${GREEN}Clean complete${NC}\n"
    exit 0
fi

if [ "$REBUILD" = true ]; then
    clean_build
fi

create_build_dir
run_cmake
build_project
generate_uf2
print_summary

printf "\n${GREEN}Build completed successfully!${NC}\n\n"
