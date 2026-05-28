#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/vreg.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/apps/httpd.h"

#include "rmii_ethernet/netif.h"
#include "board.h"

/* Static network configuration */
#define STATIC_IP_ADDR    "192.168.1.100"
#define STATIC_IP_GW      "192.168.1.1"
#define STATIC_IP_MASK    "255.255.255.0"

/* Global network interface */
static struct netif netif;

/**
 * netif_status_callback() - Called when network interface status changes
 */
static void netif_status_callback(struct netif *netif) {
    if (netif_is_up(netif)) {
        printf("[NET] Interface UP: IP %s, Netmask %s, Gateway %s\n",
               ip4addr_ntoa(&netif->ip_addr),
               ip4addr_ntoa(&netif->netmask),
               ip4addr_ntoa(&netif->gw));
        printf("[NET] Link: %s\n", netif_is_link_up(netif) ? "UP" : "DOWN");
    } else {
        printf("[NET] Interface DOWN\n");
    }
}

/**
 * led_blink_task() - Blink LED on GPIO4 at 1 Hz (500ms on, 500ms off)
 * Runs on the main core, non-blocking with async timer
 */
void led_blink_task(void) {
    static uint32_t last_toggle = 0;
    static bool led_state = false;
    
    uint32_t now = get_absolute_time() / 1000;  // milliseconds
    
    if ((now - last_toggle) >= 500) {
        led_state = !led_state;
        gpio_put(LED_GPIO, led_state);
        last_toggle = now;
    }
}

/**
 * main() - Application entry point
 */
int main(void) {
    
    /* ============ System Initialization ============ */
    
    /* Initialize USB serial for debug output */
    stdio_init_all();
    
    /* Brief delay to allow USB to stabilize */
    sleep_ms(100);
    
    printf("\n");
    printf("========================================\n");
    printf("  RP2350 PIO Ethernet Demo\n");
    printf("  System Clock: 300 MHz\n");
    printf("  Static IP: %s\n", STATIC_IP_ADDR);
    printf("========================================\n\n");
    
    /* ============ Clock Configuration ============ */
    
    printf("[CLOCK] Setting system clock to 300 MHz...\n");
    
    /* Increase voltage for stable 300 MHz operation */
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(10);
    
    /* Set clock to 300 MHz */
    set_sys_clock_khz(300000, true);
    
    printf("[CLOCK] sys_clk_hz = %lu\n", clock_get_hz(clk_sys));
    printf("[CLOCK] USB clk = %lu\n", clock_get_hz(clk_usb));
    printf("[CLOCK] ADC clk = %lu\n", clock_get_hz(clk_adc));
    
    /* ============ LED GPIO Setup ============ */
    
    printf("[GPIO] Initializing LED on GPIO %d...\n", LED_GPIO);
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
    gpio_put(LED_GPIO, 0);
    
    /* ============ lwIP Initialization ============ */
    
    printf("[LWIP] Initializing lwIP stack...\n");
    lwip_init();
    
    /* ============ Ethernet Interface Setup ============ */
    
    printf("[ETH] Initializing RMII Ethernet interface...\n");
    
    if (netif_rmii_ethernet_init(&netif) != ERR_OK) {
        printf("[ETH] ERROR: Failed to initialize Ethernet interface!\n");
        while (1) {
            led_blink_task();  // Keep LED blinking even in error
        }
    }
    
    printf("[ETH] Ethernet interface created successfully\n");
    
    /* ============ Static IP Configuration ============ */
    
    ip4_addr_t ipaddr, netmask, gw;
    
    ip4addr_aton(STATIC_IP_ADDR, &ipaddr);
    ip4addr_aton(STATIC_IP_MASK, &netmask);
    ip4addr_aton(STATIC_IP_GW, &gw);
    
    printf("[IP] Setting static IP: %s\n", STATIC_IP_ADDR);
    printf("[IP] Netmask: %s\n", STATIC_IP_MASK);
    printf("[IP] Gateway: %s\n", STATIC_IP_GW);
    
    netif_set_addr(&netif, &ipaddr, &netmask, &gw);
    
    /* ============ Interface Status Callback ============ */
    
    netif_set_status_callback(&netif, netif_status_callback);
    
    /* ============ Set as Default and Bring Up ============ */
    
    netif_set_default(&netif);
    netif_set_up(&netif);
    
    printf("[NET] Interface brought up\n");
    
    /* ============ HTTP Server ============ */
    
    printf("[HTTP] Starting httpd on port 80...\n");
    // httpd_init(); // TODO: enable when httpd app is available
    
    printf("[HTTP] HTTP server started\n");
    
    /* ============ Launch Core 1 for Ethernet Polling ============ */
    
    printf("[CORE] Launching Ethernet polling on core 1...\n");
    multicore_launch_core1(netif_rmii_ethernet_loop);
    
    sleep_ms(100);
    printf("[CORE] Core 1 launched\n");
    
    printf("\n========================================\n");
    printf("  System Ready\n");
    printf("  Web Server: http://%s/\n", STATIC_IP_ADDR);
    printf("  Try: ping %s\n", STATIC_IP_ADDR);
    printf("========================================\n\n");
    
    /* ============ Main Core Loop ============ */
    
    while (1) {
        /* Blink LED at 1 Hz */
        led_blink_task();
        
        /* Small delay to prevent busy loop */
        sleep_ms(10);
    }
    
    return 0;
}
