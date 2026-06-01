cmake_minimum_required(VERSION 3.13)

# Default to the RP2350 platform if not otherwise specified by the compiler kit or environment
if (NOT PICO_PLATFORM)
    set(PICO_PLATFORM "rp2350")
endif()

# Locate and include the Pico SDK import script
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)
elseif(DEFINED ENV{PICO_SDK_PATH} AND EXISTS "$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
elseif(DEFINED PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
else()
    message(FATAL_ERROR "PICO_SDK_PATH not found. Please ensure the PICO_SDK_PATH environment variable is set.")
endif()

project(sdr C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(I2S_PIO ${CMAKE_CURRENT_LIST_DIR}/src/i2s_receiver.pio)

add_executable(sdr
    src/main.cpp
    src/dSi5351.cpp
    src/dI2C.cpp
    src/dSSD1306.cpp
    src/dI2Srx.cpp
    src/dMUX.cpp
    src/interrupts.cpp
    src/i2s_receiver.pio
    src/SDR_State_Machine.cpp
    src/USB-cdc_functions.cpp
    src/USB-audio_functions.cpp
    src/usb_descriptors.c
)

pico_generate_pio_header(sdr
    ${I2S_PIO}
)

target_include_directories(sdr PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(sdr
    pico_stdlib
    pico_stdio
    pico_multicore
    pico_lwip
    hardware_clocks
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_irq
    tinyusb_device
    tinyusb_board
)

# Generate map, bin, hex, and UF2 files
pico_add_extra_outputs(sdr)