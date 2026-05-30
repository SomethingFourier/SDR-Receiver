cmake_minimum_required(VERSION 3.13)

# Default to the RP2350 platform if not otherwise specified by the compiler kit or environment
if (NOT PICO_PLATFORM)
    set(PICO_PLATFORM "rp2350")
endif()

# Initialize the SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2350_project C CXX ASM)

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Create the executable for the main project
add_executable(rp2350_app
    src/main.c
    src/usb_descriptors.c
    src/audio_i2s.c
)

target_include_directories(rp2350_app PRIVATE src)

# Link required libraries
target_link_libraries(rp2350_app
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_timer
    hardware_pio
    hardware_dma
    hardware_irq
    tinyusb_device
    tinyusb_board
)

pico_generate_pio_header(rp2350_app ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)

# Add pico_stdlib stdio support (USB serial)
pico_enable_stdio_usb(rp2350_app 0)
pico_enable_stdio_uart(rp2350_app 1)

# Create map/bin/hex file etc.
pico_add_extra_outputs(rp2350_app)

# Print build information
message(STATUS "Building for RP2350")
message(STATUS "Pico SDK path: $ENV{PICO_SDK_PATH}")
