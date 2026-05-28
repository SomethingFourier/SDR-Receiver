cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2350_ethernet C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

find_package(Python3 REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs.c
    COMMAND ${Python3_EXECUTABLE} 
            ${PICO_SDK_PATH}/src/rp2_common/pico_lwip/tools/makefsdata.py
            -i ${CMAKE_CURRENT_LIST_DIR}/www/index.html
            -o ${CMAKE_CURRENT_BINARY_DIR}/fs.c
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/www/index.html
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/www
    COMMENT "Generating embedded filesystem (fs.c) from HTML files..."
)

add_executable(ethernet_app
    src/main.c
    src/rmii_ethernet.c
    src/sys_arch.c
    ${CMAKE_CURRENT_BINARY_DIR}/fs.c
)

pico_generate_pio_header(ethernet_app ${CMAKE_CURRENT_LIST_DIR}/src/rmii_ethernet_phy_rx.pio)
pico_generate_pio_header(ethernet_app ${CMAKE_CURRENT_LIST_DIR}/src/rmii_ethernet_phy_tx.pio)

target_include_directories(ethernet_app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PICO_SDK_PATH}/lib/lwip/src/include
)

target_link_libraries(ethernet_app
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_timer
    hardware_structs
    hardware_gpio
    hardware_pwm
    hardware_vreg
    pico_lwip
)

target_include_directories(ethernet_app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(ethernet_app PRIVATE
    LWIP_DEBUG=0
    NO_SYS=1
)

pico_enable_stdio_usb(ethernet_app 1)
pico_enable_stdio_uart(ethernet_app 0)

set(PICO_PLATFORM rp2350)
set(PICO_NO_PICOTOOL 1)

# Generate binary and other standard outputs
pico_add_extra_outputs(ethernet_app)

# === Custom UF2 Generation for RP2350 ===
# Create Python UF2 generator with proper NOT_FINAL flags
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gen_uf2.py" "#!/usr/bin/env python3
import struct
import sys

def generate_uf2(bin_file, uf2_file, family_id, load_addr):
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    num_blocks = (len(data) + 475) // 476
    
    blocks = []
    for block_idx in range(num_blocks):
        start = block_idx * 476
        end = min(start + 476, len(data))
        chunk = data[start:end]
        
        block = bytearray(512)
        # UF2 magic
        struct.pack_into('<I', block, 0, 0x0A324655)   # 'UF2\\\\n'
        struct.pack_into('<I', block, 4, 0x9E5D4951)   # Magic 2
        
        # Flags: set NOT_FINAL (bit 0) for all non-last blocks
        flags = 0x1 if block_idx < num_blocks - 1 else 0x0
        struct.pack_into('<I', block, 8, flags)
        
        struct.pack_into('<I', block, 12, load_addr + start)   # Target address
        struct.pack_into('<I', block, 16, len(chunk))           # Data length
        struct.pack_into('<I', block, 20, block_idx)            # Block number
        struct.pack_into('<I', block, 24, num_blocks)           # Total blocks
        struct.pack_into('<I', block, 28, family_id)            # Family ID (RP2350)
        
        # Data payload
        block[32:32+len(chunk)] = chunk
        
        # Final magic
        struct.pack_into('<I', block, 508, 0x0AB16F30)
        
        blocks.append(bytes(block))
    
    with open(uf2_file, 'wb') as f:
        for b in blocks:
            f.write(b)
    
    print(f'✓ Generated {num_blocks} UF2 blocks ({len(data)} bytes)')
    print(f'  Family: 0x{family_id:08x} (RP2350)')
    print(f'  Load:   0x{load_addr:08x}')

if __name__ == '__main__':
    bin_f = sys.argv[1]
    uf2_f = sys.argv[2]
    fam = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x540ddf62
    addr = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x10000000
    generate_uf2(bin_f, uf2_f, fam, addr)
")

# Generate UF2 after binary is created (must be AFTER pico_add_extra_outputs)
add_custom_command(TARGET ethernet_app POST_BUILD
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/gen_uf2.py"
            "${CMAKE_CURRENT_BINARY_DIR}/ethernet_app.bin"
            "${CMAKE_CURRENT_BINARY_DIR}/ethernet_app.uf2"
            "0x540ddf62"
            "0x10000000"
    COMMENT "Generating RP2350 UF2 with proper NOT_FINAL flags..."
    VERBATIM
)
