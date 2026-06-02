/*
 * Copyright (c) 2024 Rob Scott, portions copyrighted as below:
 *
 * Copyright (c) 2021 Sandeep Mistry
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/udp.h"

#include "rmii_ethernet/netif.h"
#include "lwip/tcp.h"
#include "lwip/mem.h"
#include "lwip/def.h"
#include <string.h>
#include "rmii_ethernet_phy_rx.pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "audio_i2s.h"

// Set to 1 to use DHCP, or 0 to use the static IPv4 settings below.
#define USE_DHCP 1
#define STATIC_IP_ADDR(ipaddr)      IP4_ADDR((ipaddr), 192, 168, 1, 100)
#define STATIC_NETMASK_ADDR(ipaddr) IP4_ADDR((ipaddr), 255, 255, 255, 0)
#define STATIC_GATEWAY_ADDR(ipaddr)  IP4_ADDR((ipaddr), 192, 168, 1, 1)

// Set to 1 to completely disable UDP broadcast and require a unicast "HELLO" connect packet.
#define AUDIO_REQUIRE_UNICAST_REQUEST 1

#define AUDIO_UDP_PORT 4951
#define AUDIO_SAMPLE_RATE_HZ 192000u
#define AUDIO_CHANNEL_COUNT 2u
#define AUDIO_BYTES_PER_SAMPLE 3u
#define AUDIO_PACKET_MS 1u
#define AUDIO_RING_OVERRUN_THRESHOLD 384u
#define AUDIO_RING_RECOVER_FRAMES 192u
#define AUDIO_HEADER_MAGIC 0x30445541u  // 'AUD0'

#define AUDIO_FRAMES_PER_PACKET 240u // 1.25ms at 192kHz fits perfectly under 1500 byte MTU
#define AUDIO_PAYLOAD_BYTES (AUDIO_FRAMES_PER_PACKET * AUDIO_CHANNEL_COUNT * AUDIO_BYTES_PER_SAMPLE)

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t version;
  uint8_t channels;
  uint8_t bytes_per_sample;
  uint8_t flags;
  uint32_t sample_rate_hz;
  uint32_t sequence;
  uint32_t timestamp_us;
  uint16_t frame_count;
  uint16_t reserved;
} audio_udp_header_t;

static struct udp_pcb *audio_udp_pcb = NULL;
static ip_addr_t audio_udp_destination;
static ip_addr_t audio_last_bcast_addr;
static bool audio_use_unicast = false;
static uint32_t audio_last_hello_ms = 0;

static void audio_udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  if (p != NULL) {
    bool changed = !audio_use_unicast || (audio_udp_destination.addr != addr->addr);
    ip_addr_copy(audio_udp_destination, *addr);
    audio_use_unicast = true;
    audio_last_hello_ms = to_ms_since_boot(get_absolute_time());
    audio_last_bcast_addr = audio_udp_destination;
    
    if (changed) {
      printf("AUDIO: Switched to UNICAST destination %s\n", ipaddr_ntoa(addr));
    }
    
    pbuf_free(p);
  }
}

static uint32_t audio_read_idx = 0;
static uint32_t audio_sequence = 0;
static uint16_t audio_frames_staged = 0;
static uint8_t audio_packet_buffer[sizeof(audio_udp_header_t) + AUDIO_PAYLOAD_BYTES];

static volatile uint32_t audio_packets_sent = 0;
static volatile uint32_t audio_packets_dropped = 0;
static volatile uint32_t audio_send_errors = 0;
static volatile uint32_t audio_ring_overruns = 0;
static volatile uint32_t audio_frames_lost = 0;
static volatile int32_t audio_last_send_err = 0;

// Setup 50MHz clock output on GPIO23 for LAN8720 reference clock
static void setup_50mhz_clock(uint gpio_pin) {
  // Select which clock output to use (GPIO23 uses GPOUT2)
  // System clock is 200MHz by default after arch_pico_init()
  // To get 50MHz: 200MHz / 4 = 50MHz
  
  // Configure GPIO to output GPOUT2 clock
  gpio_init(gpio_pin);
  
  // Set GPIO to output GPOUT2 
  gpio_set_function(gpio_pin, GPIO_FUNC_GPCK);
  
  // Configure GPOUT2 to output sys clock divided by 4
  // GPOUT typically outputs clk_sys/divisor
  // We need 200MHz / 4 = 50MHz
  clock_gpio_init(gpio_pin, CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS, 4);
  
  printf("50MHz clock configured on GPIO %d\n", gpio_pin);
}

// Simple MDIO bit-bang test to diagnose PHY wiring before driver init
static void mdio_send_bit(bool bit, uint gpio_mdc, uint gpio_mdio) {
  // MDIO is an open-drain line with pull-up: drive low for '0',
  // release (input) for '1'. Data is sampled on the rising edge of MDC.
  if (bit) {
    // Release the line so pull-up pulls it high
    gpio_set_dir(gpio_mdio, GPIO_IN);
  } else {
    // Drive the line low
    gpio_set_dir(gpio_mdio, GPIO_OUT);
    gpio_put(gpio_mdio, 0);
  }
  sleep_us(5);
  gpio_put(gpio_mdc, 1);
  sleep_us(5);
  gpio_put(gpio_mdc, 0);
  sleep_us(5);
}

static int mdio_read_register_bitbang(int phy_addr, int reg_addr) {
  const uint gpio_mdio = PICO_RMII_ETHERNET_MDIO_PIN;
  const uint gpio_mdc = PICO_RMII_ETHERNET_MDC_PIN;
  // Ensure MDC is GPIO output and idle low (no pull-up on MDC)
  gpio_init(gpio_mdc);
  gpio_set_dir(gpio_mdc, GPIO_OUT);
  gpio_put(gpio_mdc, 0);

  // Initialize MDIO as input with internal pull-up (open-drain)
  gpio_init(gpio_mdio);
  gpio_pull_up(gpio_mdio);
  gpio_set_dir(gpio_mdio, GPIO_IN);

  // 32-bit preamble of ones
  for (int i = 0; i < 32; i++) mdio_send_bit(1, gpio_mdc, gpio_mdio);

  // Start '01'
  mdio_send_bit(0, gpio_mdc, gpio_mdio);
  mdio_send_bit(1, gpio_mdc, gpio_mdio);

  // Opcode '10' for read
  mdio_send_bit(1, gpio_mdc, gpio_mdio);
  mdio_send_bit(0, gpio_mdc, gpio_mdio);

  // PHY addr 5 bits MSB first
  for (int i = 4; i >= 0; i--) mdio_send_bit((phy_addr >> i) & 1, gpio_mdc, gpio_mdio);

  // Reg addr 5 bits MSB first
  for (int i = 4; i >= 0; i--) mdio_send_bit((reg_addr >> i) & 1, gpio_mdc, gpio_mdio);

  // Turnaround: release MDIO (Z) for 2 clock cycles
  gpio_set_dir(gpio_mdio, GPIO_IN);
  // Toggle two clocks while MDIO is high-impedance
  for (int i = 0; i < 2; i++) {
    sleep_us(5);
    gpio_put(gpio_mdc, 1);
    sleep_us(5);
    gpio_put(gpio_mdc, 0);
  }

  // Read 16 bits
  uint16_t val = 0;
  for (int i = 15; i >= 0; i--) {
    sleep_us(5);
    gpio_put(gpio_mdc, 1);
    sleep_us(5);
    int b = gpio_get(gpio_mdio);
    val |= (b & 1) << i;
    gpio_put(gpio_mdc, 0);
    sleep_us(5);
  }

  return val;
}

// Run a small MDIO register read test and print common registers
static void run_mdio_test(int phy_addr) {
  printf("\n=== MDIO TEST: PHY %d ===\n", phy_addr);
  fflush(stdout);
  
  for (int r = 0; r <= 4; r++) {
    printf("  Reading REG %02d...", r);
    fflush(stdout);
    int v = mdio_read_register_bitbang(phy_addr, r);
    printf(" Got 0x%04x\n", v & 0xffff);
    fflush(stdout);
  }
  
  printf("  Reading PHY ID registers...");
  fflush(stdout);
  int id1 = mdio_read_register_bitbang(phy_addr, 2);
  int id2 = mdio_read_register_bitbang(phy_addr, 3);
  uint32_t phy_id = ((uint32_t)(id1 & 0xffff) << 16) | (uint32_t)(id2 & 0xffff);
  printf(" ID: 0x%08x\n", phy_id);
  fflush(stdout);
  printf("=== MDIO TEST END ===\n\n");
  fflush(stdout);
}

// Scan MDIO addresses 0..31 and print PHY ID registers (if any respond)
static void scan_mdio_addresses(void) {
  printf("\n=== MDIO ADDR SCAN ===\n");
  for (int a = 0; a < 32; a++) {
    int id1 = mdio_read_register_bitbang(a, 2);
    int id2 = mdio_read_register_bitbang(a, 3);
    if ((id1 & 0xffff) != 0xffff || (id2 & 0xffff) != 0xffff) {
      uint32_t phy_id = ((uint32_t)(id1 & 0xffff) << 16) | (uint32_t)(id2 & 0xffff);
      printf("PHY addr %02d: ID=0x%08x (id1=0x%04x id2=0x%04x)\n", a, phy_id, id1 & 0xffff, id2 & 0xffff);
    }
  }
  printf("=== MDIO ADDR SCAN END ===\n\n");
}


void netif_link_callback(struct netif *netif) {
  printf("netif link status changed %s\n",
         netif_is_link_up(netif) ? "up" : "down");
}

void netif_status_callback(struct netif *netif) {
  printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

static struct netif *status_netif = NULL;

static bool network_status_cb(repeating_timer_t *rt) {
  (void)rt;

  if (status_netif == NULL) {
    return true;
  }

  char ip_buf[16];
  char mask_buf[16];
  char gw_buf[16];

  ip4addr_ntoa_r(netif_ip4_addr(status_netif), ip_buf, sizeof(ip_buf));
  ip4addr_ntoa_r(netif_ip4_netmask(status_netif), mask_buf, sizeof(mask_buf));
  ip4addr_ntoa_r(netif_ip4_gw(status_netif), gw_buf, sizeof(gw_buf));

  printf("NETWORK: link=%s up=%s dhcp=%s ip=%s mask=%s gw=%s\n",
         netif_is_link_up(status_netif) ? "up" : "down",
         netif_is_up(status_netif) ? "yes" : "no",
         (USE_DHCP && dhcp_supplied_address(status_netif)) ? "lease" :
             (USE_DHCP ? "waiting" : "static"),
         ip_buf, mask_buf, gw_buf);

  return true;
}

static void configure_networking(struct netif *netif) {
  if (USE_DHCP) {
    ip4_addr_t zero;
    IP4_ADDR(&zero, 0, 0, 0, 0);
    netif_set_addr(netif, &zero, &zero, &zero);

    printf("Starting DHCP client...\n");
    dhcp_start(netif);
  } else {
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    STATIC_IP_ADDR(&ipaddr);
    STATIC_NETMASK_ADDR(&netmask);
    STATIC_GATEWAY_ADDR(&gateway);
    netif_set_addr(netif, &ipaddr, &netmask, &gateway);

    printf("Static IP configured: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
  }
}

static bool led_blink_fast_cb(repeating_timer_t *rt) {
  static bool on = false;
  on = !on;
  gpio_put(4, on);
  return true;
}

static bool led_blink_normal_cb(repeating_timer_t *rt) {
  static bool on = false;
  on = !on;
  gpio_put(5, on);
  return true;
}

struct http_state {
  bool response_pending;
  bool close_pending;
};

static const char http_response[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 12\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<h1>Hello World!</h1>";

static err_t http_close_connection(struct tcp_pcb *tpcb, struct http_state *state) {
  printf("HTTP: Closing connection\n");
  tcp_arg(tpcb, NULL);
  tcp_recv(tpcb, NULL);
  tcp_sent(tpcb, NULL);
  tcp_poll(tpcb, NULL, 0);
  if (state != NULL) {
    mem_free(state);
  }
  return tcp_close(tpcb);
}

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  struct http_state *state = (struct http_state *)arg;
  (void)len;
  if (state != NULL && state->close_pending) {
    return http_close_connection(tpcb, state);
  }
  return ERR_OK;
}

static err_t http_poll(void *arg, struct tcp_pcb *tpcb) {
  struct http_state *state = (struct http_state *)arg;
  if (state != NULL && state->close_pending) {
    return http_close_connection(tpcb, state);
  }
  return ERR_OK;
}

static void http_err(void *arg, err_t err) {
  struct http_state *state = (struct http_state *)arg;
  printf("HTTP: Connection error: %d\n", err);
  if (state != NULL) {
    mem_free(state);
  }
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  struct http_state *state = (struct http_state *)arg;
  if (err != ERR_OK) {
    printf("HTTP: Recv error: %d\n", err);
    if (p != NULL) {
      pbuf_free(p);
    }
    return err;
  }

  if (p == NULL) {
    printf("HTTP: Client requested connection close (FIN)\n");
    return http_close_connection(tpcb, state);
  }

  printf("HTTP: Received %d bytes of data\n", p->tot_len);
  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  if (state == NULL) {
    return ERR_VAL;
  }

  if (!state->response_pending) {
    state->response_pending = true;
    printf("HTTP: Sending Hello World response...\n");
    err_t write_err = tcp_write(tpcb, http_response, sizeof(http_response) - 1, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
      printf("HTTP: tcp_write failed: %d\n", write_err);
      return write_err;
    }
    tcp_output(tpcb);
    state->close_pending = true;
    tcp_sent(tpcb, http_sent);
    tcp_poll(tpcb, http_poll, 2);
  }

  return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  (void)arg;
  if (err != ERR_OK) {
    printf("HTTP: Accept error: %d\n", err);
    return err;
  }

  printf("HTTP: Accepted connection from %s:%d\n",
         ipaddr_ntoa(&newpcb->remote_ip), newpcb->remote_port);

  struct http_state *state = (struct http_state *)mem_malloc(sizeof(struct http_state));
  if (state == NULL) {
    printf("HTTP: Failed to allocate state memory!\n");
    return ERR_MEM;
  }

  state->response_pending = false;
  state->close_pending = false;

  tcp_arg(newpcb, state);
  tcp_recv(newpcb, http_recv);
  tcp_sent(newpcb, http_sent);
  tcp_poll(newpcb, http_poll, 2);
  tcp_err(newpcb, http_err);
  return ERR_OK;
}

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

static inline void audio_pack_s24_le(uint8_t *dst, uint32_t sample_word) {
  int32_t sample_24 = ((int32_t)sample_word) >> 8;
  dst[0] = (uint8_t)(sample_24 & 0xff);
  dst[1] = (uint8_t)((sample_24 >> 8) & 0xff);
  dst[2] = (uint8_t)((sample_24 >> 16) & 0xff);
}

static bool audio_udp_socket_init(void) {
  audio_udp_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
  if (audio_udp_pcb == NULL) {
    printf("AUDIO: udp_new_ip_type() failed\n");
    return false;
  }

  if (udp_bind(audio_udp_pcb, IP_ANY_TYPE, AUDIO_UDP_PORT) != ERR_OK) {
    printf("AUDIO: udp_bind() failed\n");
    udp_remove(audio_udp_pcb);
    audio_udp_pcb = NULL;
    return false;
  }

  ip_set_option(audio_udp_pcb, SOF_BROADCAST);
  udp_recv(audio_udp_pcb, audio_udp_recv_cb, NULL);

  printf("AUDIO: UDP broadcaster ready on port %u\n", AUDIO_UDP_PORT);
  return true;
}

static bool audio_update_broadcast_destination(struct netif *netif) {
  if (netif == NULL || !netif_is_link_up(netif) || !netif_is_up(netif)) {
    return false;
  }

  if (USE_DHCP && !dhcp_supplied_address(netif)) {
    return false;
  }

  uint32_t ip_host = lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(netif)));
  uint32_t mask_host = lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(netif)));

  if (ip_host == 0 || mask_host == 0) {
    return false;
  }

  if (audio_use_unicast) {
    if (to_ms_since_boot(get_absolute_time()) - audio_last_hello_ms > 3000) {
      printf("AUDIO: Client timeout. Stopping unicast stream.\n");
      audio_use_unicast = false;
    }
  }

  if (!audio_use_unicast) {
#if AUDIO_REQUIRE_UNICAST_REQUEST
    return false;
#else
    uint32_t bcast_host = (ip_host & mask_host) | (~mask_host);
    ip4_addr_t bcast_addr;
    ip4_addr_set_u32(&bcast_addr, lwip_htonl(bcast_host));
    ip_addr_copy_from_ip4(audio_udp_destination, bcast_addr);
#endif
  }
  udp_bind_netif(audio_udp_pcb, netif);

  if (audio_last_bcast_addr.addr != audio_udp_destination.addr) {
    audio_last_bcast_addr = audio_udp_destination;
    printf("AUDIO: UDP destination ready on port %u (%s)\n", AUDIO_UDP_PORT, ipaddr_ntoa(&audio_udp_destination));
  }
  return true;
}

static void audio_udp_write_header(void) {
  audio_udp_header_t *hdr = (audio_udp_header_t *)audio_packet_buffer;
  hdr->magic = AUDIO_HEADER_MAGIC;
  hdr->version = 1;
  hdr->channels = AUDIO_CHANNEL_COUNT;
  hdr->bytes_per_sample = AUDIO_BYTES_PER_SAMPLE;
  hdr->flags = 1;  // 1 = signed little-endian PCM
  hdr->sample_rate_hz = AUDIO_SAMPLE_RATE_HZ;
  hdr->sequence = audio_sequence++;
  hdr->timestamp_us = time_us_32();
  hdr->frame_count = AUDIO_FRAMES_PER_PACKET;
  hdr->reserved = 0;
}

static void audio_udp_send_packet(struct netif *netif) {
  if (audio_udp_pcb == NULL || !audio_update_broadcast_destination(netif)) {
    audio_packets_dropped++;
    audio_frames_staged = 0;
    return;
  }

  audio_udp_write_header();

  struct pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, sizeof(audio_packet_buffer), PBUF_POOL);
  if (packet == NULL) {
    audio_packets_dropped++;
    audio_send_errors++;
    audio_last_send_err = ERR_MEM;
    printf("AUDIO: pbuf_alloc failed len=%u\n", (unsigned)sizeof(audio_packet_buffer));
    audio_frames_staged = 0;
    return;
  }

  err_t take_err = pbuf_take(packet, audio_packet_buffer, sizeof(audio_packet_buffer));
  if (take_err != ERR_OK) {
    pbuf_free(packet);
    audio_packets_dropped++;
    audio_send_errors++;
    audio_last_send_err = take_err;
    printf("AUDIO: pbuf_take failed err=%d\n", take_err);
    audio_frames_staged = 0;
    return;
  }

  err_t send_err = udp_sendto_if_src(
      audio_udp_pcb,
      packet,
      &audio_udp_destination,
      AUDIO_UDP_PORT,
      netif,
      netif_ip4_addr(netif));
  pbuf_free(packet);

  if (send_err == ERR_OK) {
    audio_packets_sent++;
  } else {
    audio_packets_dropped++;
    audio_send_errors++;
    audio_last_send_err = send_err;
    printf("AUDIO: udp_sendto_if_src failed err=%d\n", send_err);
  }

  audio_frames_staged = 0;
}

static void audio_stream_step(struct netif *netif) {
  uint32_t write_idx = audio_i2s_get_write_index();
  uint32_t available = (write_idx - audio_read_idx) & (AUDIO_RING_FRAMES - 1);

  if (available > AUDIO_RING_OVERRUN_THRESHOLD) {
    uint32_t recovered = AUDIO_RING_RECOVER_FRAMES;
    if (available > recovered) {
      audio_frames_lost += (available - recovered);
    }
    audio_ring_overruns++;
    audio_read_idx = (write_idx - recovered) & (AUDIO_RING_FRAMES - 1);
    available = recovered;
  }

  while (available > 0) {
    uint32_t needed = AUDIO_FRAMES_PER_PACKET - audio_frames_staged;
    uint32_t take = min_u32(available, needed);
    uint32_t payload_offset = sizeof(audio_udp_header_t) + (audio_frames_staged * 6u);

    for (uint32_t i = 0; i < take; i++) {
      uint32_t frame = (audio_read_idx + i) & (AUDIO_RING_FRAMES - 1);
      uint32_t left = audio_ring_buffer[frame * 2u + 0u];
      uint32_t right = audio_ring_buffer[frame * 2u + 1u];
      uint8_t *dst = &audio_packet_buffer[payload_offset + i * 6u];

      audio_pack_s24_le(&dst[0], left);
      audio_pack_s24_le(&dst[3], right);
    }

    audio_read_idx = (audio_read_idx + take) & (AUDIO_RING_FRAMES - 1);
    audio_frames_staged += (uint16_t)take;
    available -= take;

    if (audio_frames_staged >= AUDIO_FRAMES_PER_PACKET) {
      audio_udp_send_packet(netif);
    }
  }
}

static bool audio_status_cb(repeating_timer_t *rt) {
  (void)rt;
  printf("AUDIO: sent=%lu dropped=%lu send_err=%lu overruns=%lu lost_frames=%lu staged=%u\n",
         audio_packets_sent,
         audio_packets_dropped,
         audio_send_errors,
         audio_ring_overruns,
         audio_frames_lost,
         audio_frames_staged);
  if (audio_send_errors != 0) {
    printf("AUDIO: last_send_err=%ld\n", (long)audio_last_send_err);
  }
  return true;
}

static void core1_net_audio_loop(void) {
  while (1) {
    netif_rmii_ethernet_poll();
    audio_stream_step(status_netif);
  }
}

int main() {
  // LWIP network interface
  struct netif netif;

  // Setup LEDs early to indicate startup
  gpio_init(4);
  gpio_set_dir(4, GPIO_OUT);
  gpio_init(5);
  gpio_set_dir(5, GPIO_OUT);
  static repeating_timer_t led_timer;
  add_repeating_timer_ms(100, led_blink_fast_cb, NULL, &led_timer);

  // Do board specific init
  arch_pico_init();

  // Setup 50MHz reference clock for LAN8720 on GPIO23
  setup_50mhz_clock(23);

  printf("pico rmii ethernet - httpd + udp audio\n");

  // Initilize LWIP in NO_SYS mode
  lwip_init();

  // Initialize the PIO-based RMII Ethernet network interface
  // Run a quick MDIO bit-bang diagnostic before initializing the driver
#ifdef PICO_RMII_ETHERNET_RST_PIN
  // Hard reset the PHY to clear any weird states from previous runs
  gpio_init(PICO_RMII_ETHERNET_RST_PIN);
  gpio_set_dir(PICO_RMII_ETHERNET_RST_PIN, GPIO_OUT);
  
  // Pull reset low for 50ms
  gpio_put(PICO_RMII_ETHERNET_RST_PIN, 0);
  sleep_ms(50);
  
  // Deassert reset pin so the PHY is active during diagnostic checks
  gpio_put(PICO_RMII_ETHERNET_RST_PIN, 1);
  sleep_ms(150); // Give it time to boot and stabilize its PLLs
#endif
  printf("Running MDIO bit-bang diagnostic...\n");
  fflush(stdout);
  run_mdio_test(0);
  // If registers read as 0xffff, scan all addresses to help debug wiring/address
  scan_mdio_addresses();

  if (netif_rmii_ethernet_init(&netif) != ERR_OK) {
    printf("Failed to open ethernet interface\n");
    return -1;
  }

  // Report configuration
  arch_pico_info(&netif);

  // Assign callbacks for link and status
  netif_set_link_callback(&netif, netif_link_callback);
  netif_set_status_callback(&netif, netif_status_callback);

  // Set the default interface and bring it up
  netif_set_default(&netif);
  netif_set_up(&netif);

  // Configure either DHCP or a static IPv4 address.
  configure_networking(&netif);
  status_netif = &netif;

  // Periodic network status report.
  static repeating_timer_t network_status_timer;
  add_repeating_timer_ms(5000, network_status_cb, NULL, &network_status_timer);

  // Initialize I2S ADC capture and UDP audio broadcasting state.
  audio_i2s_init();
  audio_read_idx = audio_i2s_get_write_index();
  if (!audio_udp_socket_init()) {
    printf("AUDIO: continuing without UDP stream due to socket init failure\n");
  }

  static repeating_timer_t audio_status_timer;
  add_repeating_timer_ms(2000, audio_status_cb, NULL, &audio_status_timer);

  // Simple raw-TCP HTTP server using lwIP raw API that serves a Hello World page
  // This runs via callbacks while core1 handles the RMII loop.
  struct tcp_pcb *pcb = tcp_new();
  if (pcb != NULL) {
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    if (pcb != NULL) {
      tcp_accept(pcb, http_accept);
      printf("Raw HTTP server listening on port 80\n");
    }
  }

  // Core 1 runs lwIP poll + UDP audio sender to keep all lwIP calls on one core.
  multicore_launch_core1(core1_net_audio_loop);

  // Startup complete: transition LEDs
  cancel_repeating_timer(&led_timer);
  gpio_put(4, 1); // Solid ON for GPIO4
  add_repeating_timer_ms(500, led_blink_normal_cb, NULL, &led_timer); // Normal blink for GPIO5

  while (1) {
    tight_loop_contents();
  }

  return 0;
}
