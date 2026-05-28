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

#include "lwip/init.h"
#include "lwip/ip4_addr.h"

#include "rmii_ethernet/netif.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/mem.h"
#include <string.h>
#include "rmii_ethernet_phy_rx.pio.h"
#include "hardware/clocks.h"

#include "hardware/clocks.h"

// Setup 50MHz clock output on GPIO23 for LAN8720 reference clock
static void setup_50mhz_clock(uint gpio_pin) {
  // Select which clock output to use (GPIO23 uses GPOUT2)
  // System clock is 300MHz by default after arch_pico_init()
  // To get 50MHz: 300MHz / 6 = 50MHz
  
  // Configure GPIO to output GPOUT2 clock
  gpio_init(gpio_pin);
  
  // Set GPIO to output GPOUT2 
  gpio_set_function(gpio_pin, GPIO_FUNC_GPCK);
  
  // Configure GPOUT2 to output sys clock divided by 6
  // GPOUT typically outputs clk_sys/divisor
  // We need 300MHz / 6 = 50MHz
  clock_gpio_init(gpio_pin, CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS, 6);
  
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

static bool led_blink_cb(repeating_timer_t *rt) {
  static bool on = false;
  on = !on;
  gpio_put(4, on);
  return true;
}

// Heartbeat to verify USB serial works
static bool heartbeat_cb(repeating_timer_t *rt) {
  (void)rt;
  printf("HEARTBEAT: system clk %4.2f MHz\n", (float)clock_get_hz(clk_sys)/1e6);
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

int main() {
  // LWIP network interface
  struct netif netif;

  // Do board specific init
  arch_pico_init();

  // Setup 50MHz reference clock for LAN8720 on GPIO23
  setup_50mhz_clock(23);

  printf("pico rmii ethernet - httpd\n");

  // Initilize LWIP in NO_SYS mode
  lwip_init();

  // Initialize the PIO-based RMII Ethernet network interface
  // Run a quick MDIO bit-bang diagnostic before initializing the driver
#ifdef PICO_RMII_ETHERNET_RST_PIN
  // Deassert reset pin so the PHY is active during diagnostic checks
  gpio_put(PICO_RMII_ETHERNET_RST_PIN, 1);
  sleep_ms(100);
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

  // Configure a static IPv4 address
  ip4_addr_t ipaddr;
  ip4_addr_t netmask;
  ip4_addr_t gateway;
  IP4_ADDR(&ipaddr, 192, 168, 1, 100);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gateway, 192, 168, 1, 1);
  netif_set_addr(&netif, &ipaddr, &netmask, &gateway);

  printf("Static IP configured: %s\n", ip4addr_ntoa(netif_ip4_addr(&netif)));

  // Set the default interface and bring it up
  netif_set_default(&netif);
  netif_set_up(&netif);

  // Initialize LED on GPIO 4 and start 1 Hz blink timer
  gpio_init(4);
  gpio_set_dir(4, GPIO_OUT);
  static repeating_timer_t led_timer;
  add_repeating_timer_ms(500, led_blink_cb, NULL, &led_timer);

  // Periodic serial heartbeat to verify USB serial is alive
  static repeating_timer_t heartbeat_timer;
  add_repeating_timer_ms(1000, heartbeat_cb, NULL, &heartbeat_timer);

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

  // Setup core 1 to monitor the RMII ethernet interface
  // This allows core 0 do other things :)
  multicore_launch_core1(netif_rmii_ethernet_loop);

  while (1) {
    tight_loop_contents();
  }

  return 0;
}
