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
#include <string.h>
#include "rmii_ethernet_phy_rx.pio.h"
#include "hardware/clocks.h"

#include "hardware/clocks.h"

// Setup 50MHz clock output on GPIO23 for LAN8720 reference clock
static void setup_50mhz_clock(uint gpio_pin) {
  // Select which clock output to use (GPIO23 uses GPOUT2)
  // System clock is 300MHz by default after arch_pico_init()
  // To get 50MHz: 300MHz / 6 = 50MHz
  
  // Configure GPIO23 to output GPOUT2 clock
  gpio_init(gpio_pin);
  
  // Set GPIO23 to output GPOUT2 
  gpio_set_function(gpio_pin, GPIO_FUNC_GPCK);
  
  // Configure GPOUT2 to output sys clock divided by 6
  // GPOUT typically outputs clk_sys/divisor
  // We need 300MHz / 6 = 50MHz
  clock_gpio_init(gpio_pin, CLOCKS_CLK_GPOUT2_CTRL_AUXSRC_VALUE_CLK_SYS, 6);
  
  printf("50MHz clock configured on GPIO %d\n", gpio_pin);
}

// Simple MDIO bit-bang test to diagnose PHY wiring before driver init
static void mdio_send_bit(bool bit, uint gpio_mdc, uint gpio_mdio) {
  // Data is sampled on rising edge of MDC in PHYs typically, so
  // set data then toggle clock high then low
  gpio_put(gpio_mdio, bit);
  sleep_us(1);
  gpio_put(gpio_mdc, 1);
  sleep_us(1);
  gpio_put(gpio_mdc, 0);
  sleep_us(1);
}

static int mdio_read_register_bitbang(int phy_addr, int reg_addr) {
  const uint gpio_mdio = PICO_RMII_ETHERNET_MDIO_PIN;
  const uint gpio_mdc = PICO_RMII_ETHERNET_MDC_PIN;
  // Ensure pins are GPIO and start idle low
  gpio_init(gpio_mdc);
  gpio_set_dir(gpio_mdc, GPIO_OUT);
  gpio_put(gpio_mdc, 0);

  gpio_init(gpio_mdio);
  // Drive MDIO for preamble
  gpio_set_dir(gpio_mdio, GPIO_OUT);

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
    sleep_us(1);
    gpio_put(gpio_mdc, 1);
    sleep_us(1);
    gpio_put(gpio_mdc, 0);
  }

  // Read 16 bits
  uint16_t val = 0;
  for (int i = 15; i >= 0; i--) {
    sleep_us(1);
    gpio_put(gpio_mdc, 1);
    sleep_us(1);
    int b = gpio_get(gpio_mdio);
    val |= (b & 1) << i;
    gpio_put(gpio_mdc, 0);
    sleep_us(1);
  }

  return val;
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
  tcp_arg(tpcb, NULL);
  tcp_recv(tpcb, NULL);
  tcp_sent(tpcb, NULL);
  tcp_poll(tpcb, NULL, 0);
  if (state != NULL) {
    state->close_pending = false;
    state->response_pending = false;
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
  (void)arg;
  (void)err;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  struct http_state *state = (struct http_state *)arg;
  if (err != ERR_OK) {
    if (p != NULL) {
      pbuf_free(p);
    }
    return err;
  }

  if (p == NULL) {
    if (state != NULL && state->close_pending) {
      return http_close_connection(tpcb, state);
    }
    return ERR_OK;
  }

  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  if (state == NULL) {
    return ERR_VAL;
  }

  if (!state->response_pending) {
    state->response_pending = true;
    err_t write_err = tcp_write(tpcb, http_response, sizeof(http_response) - 1, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
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
  static struct http_state http_state;
  (void)arg;
  if (err != ERR_OK) {
    return err;
  }

  http_state.response_pending = false;
  http_state.close_pending = false;

  tcp_arg(newpcb, &http_state);
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
  printf("Running MDIO bit-bang diagnostic...\n");
  int mdio_val = mdio_read_register_bitbang(0, 0);
  printf("MDIO read (phy 0 reg 0): 0x%04x\n", mdio_val & 0xffff);

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

  // Setup core 1 to monitor the RMII ethernet interface
  // This allows core 0 do other things :)
  multicore_launch_core1(netif_rmii_ethernet_loop);

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

  while (1) {
    tight_loop_contents();
  }

  return 0;
}
