cmake_minimum_required(VERSION 3.13)

# Initialize the SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2350_project C CXX ASM)

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Define custom Ethernet GPIO Pin Configuration before adding subdirectories
# These definitions will be used by pico-rmii-ethernet library
set(ETHEREUM_PIN_DEFS
  "-DPICO_RMII_ETHERNET_TX_EN_PIN=10"      # TX Enable
  "-DPICO_RMII_ETHERNET_TXD0_PIN=11"       # TXD0
  "-DPICO_RMII_ETHERNET_TXD1_PIN=12"       # TXD1
  "-DPICO_RMII_ETHERNET_RXD0_PIN=14"       # RXD0
  "-DPICO_RMII_ETHERNET_RXD1_PIN=15"       # RXD1
  "-DPICO_RMII_ETHERNET_CRS_DV_PIN=13"     # CRS/DV (Carrier Sense/Data Valid)
  "-DPICO_RMII_ETHERNET_MDIO_PIN=16"       # MDIO (Management Data Input/Output)
  "-DPICO_RMII_ETHERNET_MDC_PIN=17"        # MDC (Management Data Clock)
  "-DPICO_RMII_ETHERNET_RST_PIN=18"        # Reset
  "-DPICO_RMII_ETHERNET_RETCLK_PIN=23"     # RMII Reference Clock (50MHz)
)

# Add compiler flags for the ethernet library compilation
add_compile_options(${ETHERNET_PIN_DEFS})

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

# Custom Ethernet GPIO Pin Configuration
# These definitions override the default pin assignments
target_compile_definitions(rp2350_app PRIVATE
  # Ethernet Data Pins
  PICO_RMII_ETHERNET_TX_EN_PIN=10       # TX Enable
  PICO_RMII_ETHERNET_TXD0_PIN=11        # TXD0
  PICO_RMII_ETHERNET_TXD1_PIN=12        # TXD1
  PICO_RMII_ETHERNET_RXD0_PIN=14        # RXD0
  PICO_RMII_ETHERNET_RXD1_PIN=15        # RXD1
  PICO_RMII_ETHERNET_CRS_DV_PIN=13      # CRS/DV (Carrier Sense/Data Valid)
  
  # LAN8720 Management Interface
  PICO_RMII_ETHERNET_MDIO_PIN=16        # MDIO (Management Data Input/Output)
  PICO_RMII_ETHERNET_MDC_PIN=17         # MDC (Management Data Clock)
  PICO_RMII_ETHERNET_RST_PIN=18         # Reset
  
  # 50MHz Reference Clock Output for LAN8720
  PICO_RMII_ETHERNET_RETCLK_PIN=23      # RMII Reference Clock (50MHz)
  
  # Serial Port Configuration
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=16
  PICO_DEFAULT_UART_RX_PIN=17
)

# Create map/bin/hex file etc.
pico_add_extra_outputs(rp2350_app)

# Print build information
message(STATUS "Building for RP2350")
message(STATUS "Pico SDK path: $ENV{PICO_SDK_PATH}")
