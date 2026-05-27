cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350)
set(PICO_BOARD pico2)
include(pico_sdk_import.cmake)

project(rp2350_lan8720_eth C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(LWIP_PATH "${PICO_SDK_PATH}/lib/lwip")

target_sources(pico_lwip INTERFACE
    ${LWIP_PATH}/src/core/altcp.c
    ${LWIP_PATH}/src/core/altcp_alloc.c
    ${LWIP_PATH}/src/core/altcp_tcp.c
    ${LWIP_PATH}/src/core/def.c
    ${LWIP_PATH}/src/core/dns.c
    ${LWIP_PATH}/src/core/inet_chksum.c
    ${LWIP_PATH}/src/core/init.c
    ${LWIP_PATH}/src/core/ip.c
    ${LWIP_PATH}/src/core/mem.c
    ${LWIP_PATH}/src/core/memp.c
    ${LWIP_PATH}/src/core/netif.c
    ${LWIP_PATH}/src/core/pbuf.c
    ${LWIP_PATH}/src/core/raw.c
    ${LWIP_PATH}/src/core/stats.c
    ${LWIP_PATH}/src/core/sys.c
    ${LWIP_PATH}/src/core/tcp.c
    ${LWIP_PATH}/src/core/tcp_in.c
    ${LWIP_PATH}/src/core/tcp_out.c
    ${LWIP_PATH}/src/core/timeouts.c
    ${LWIP_PATH}/src/core/udp.c
    ${LWIP_PATH}/src/core/ipv4/autoip.c
    ${LWIP_PATH}/src/core/ipv4/dhcp.c
    ${LWIP_PATH}/src/core/ipv4/etharp.c
    ${LWIP_PATH}/src/core/ipv4/icmp.c
    ${LWIP_PATH}/src/core/ipv4/igmp.c
    ${LWIP_PATH}/src/core/ipv4/ip4.c
    ${LWIP_PATH}/src/core/ipv4/ip4_addr.c
    ${LWIP_PATH}/src/core/ipv4/ip4_frag.c
    ${LWIP_PATH}/src/netif/ethernet.c
    ${CMAKE_CURRENT_LIST_DIR}/src/lwip/sys_arch.c
)

target_include_directories(pico_lwip INTERFACE
    ${LWIP_PATH}/src/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/lwip
)

add_executable(rp2350_lan8720_eth
    src/main.c
    src/lan8720.c
    src/rmii_refclk.c
    src/web_server.c
)

pico_generate_pio_header(rp2350_lan8720_eth ${CMAKE_CURRENT_LIST_DIR}/pio/rmii_refclk.pio)

target_include_directories(rp2350_lan8720_eth PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(rp2350_lan8720_eth
    pico_stdlib
    pico_multicore
    pico_unique_id
    pico_lwip
    hardware_pio
    hardware_gpio
    hardware_clocks
    hardware_irq
)

pico_enable_stdio_usb(rp2350_lan8720_eth 1)
pico_enable_stdio_uart(rp2350_lan8720_eth 1)

pico_add_extra_outputs(rp2350_lan8720_eth)