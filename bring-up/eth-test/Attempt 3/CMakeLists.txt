cmake_minimum_required(VERSION 3.13)

# Initialize the SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2350_project C CXX ASM)

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add the external pico-rmii-ethernet library
add_subdirectory(external/pico-rmii-ethernet)

# Create the executable for the main project
add_executable(rp2350_app
    src/main.c
)

# Link required libraries
target_link_libraries(rp2350_app
    pico_stdlib
    pico_multicore
    pico_rmii_ethernet
    pico_lwip
    hardware_timer
)

# Add pico_stdlib stdio support (USB serial)
pico_enable_stdio_usb(rp2350_app 1)
pico_enable_stdio_uart(rp2350_app 1)

# Use J7 as serial port
target_compile_definitions(rp2350_app PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=16
  PICO_DEFAULT_UART_RX_PIN=17
)

# Create map/bin/hex file etc.
pico_add_extra_outputs(rp2350_app)

# Print build information
message(STATUS "Building for RP2350")
message(STATUS "Pico SDK path: $ENV{PICO_SDK_PATH}")
