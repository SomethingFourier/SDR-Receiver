#include "dEthernet.hpp"

#include <pico/stdlib.h>
#include <hardware/clocks.h>

#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/ip4_addr.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/tcp.h>

#include <rmii_ethernet/netif.h>
#include <rmii_ethernet_phy_rx.pio.h>

#include "dI2Srx.hpp"

#define LAN_CLOCK 23

#define AUDIO_UDP_PORT 4951
#define AUDIO_SAMPLE_RATE_HZ 48000u
#define AUDIO_CHANNEL_COUNT 2u
#define AUDIO_BYTES_PER_SAMPLE 3u
#define AUDIO_PACKET_MS 5u
#define AUDIO_RING_OVERRUN_THRESHOLD 96u
#define AUDIO_RING_RECOVER_FRAMES 32u
#define AUDIO_HEADER_MAGIC 0x30445541u  // 'AUD0'

// Set to 1 to use DHCP, or 0 to use the static IPv4 settings below.
#define USE_DHCP 1
#define STATIC_IP_ADDR(ipaddr)      IP4_ADDR((ipaddr), 192, 168, 1, 100)
#define STATIC_NETMASK_ADDR(ipaddr) IP4_ADDR((ipaddr), 255, 255, 255, 0)
#define STATIC_GATEWAY_ADDR(ipaddr)  IP4_ADDR((ipaddr), 192, 168, 1, 1)

dEthernet::dEthernet() {
    gpio_init(LAN_CLOCK);
    gpio_set_function(LAN_CLOCK, GPIO_FUNC_GPCK);
    clock_gpio_init(LAN_CLOCK, CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS, 6); // 50 MHz clock
}

void dEthernet::Init() {}