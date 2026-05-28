#include <stdio.h>

#include "board_pins.h"
#include "lan8720.h"
#include "rmii_refclk.h"
#include "web_server.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"

enum {
    ETH_SYS_CLOCK_HZ = 200000000,
};

static lan8720_t phy = {
    .mdio_pin = BOARD_LAN8720_MDIO_PIN,
    .mdc_pin = BOARD_LAN8720_MDC_PIN,
    .reset_pin = BOARD_LAN8720_RESET_PIN,
    .phy_addr = 0xffu,
};

static void core1_main(void) {
    while (true) {
        bool link_up = lan8720_link_up(&phy);
        gpio_put(BOARD_GREEN_LED_PIN, link_up ? 1 : 0);
        sleep_ms(500);
    }
}

static bool heartbeat_cb(struct repeating_timer *t) {
    (void)t;
    static bool on = false;
    on = !on;
    gpio_put(BOARD_RED_LED_PIN, on ? 1 : 0);
    return true;
}

static const lan8720_t *g_phy_ptr_for_timer = NULL;
static bool phy_status_cb(struct repeating_timer *t) {
    (void)t;
    if (g_phy_ptr_for_timer == NULL) return true;

    const lan8720_t *p = g_phy_ptr_for_timer;
    uint16_t phyid1 = lan8720_read_register(p, 2);
    uint16_t phyid2 = lan8720_read_register(p, 3);
    uint16_t bmcr = lan8720_read_register(p, 0);
    
    // Read BMSR twice per IEEE 802.3 standard
    uint16_t bmsr = lan8720_read_register(p, 1);
    bmsr = lan8720_read_register(p, 1);

    bool link = (bmsr & (1u << 2)) != 0;
    bool autoneg_complete = (bmsr & (1u << 5)) != 0;
    bool autoneg_enable = (bmcr & (1u << 12)) != 0;
    bool speed_100 = (bmcr & (1u << 13)) != 0;
    bool duplex_full = (bmcr & (1u << 8)) != 0;

    printf("PHY Status: addr=%u id1=0x%04x id2=0x%04x\n", p->phy_addr, phyid1, phyid2);
    printf("  BMCR=0x%04x: speed=%s autoneg=%s duplex=%s\n",
           bmcr,
           speed_100 ? "100M" : "10M",
           autoneg_enable ? "ON" : "OFF",
           duplex_full ? "Full" : "Half");
    printf("  BMSR=0x%04x: link=%d autoneg_complete=%d\n", bmsr, link, autoneg_complete);

    return true;
}

int main(void) {
    set_sys_clock_khz(ETH_SYS_CLOCK_HZ / 1000, true);

    /* Early heartbeat blink before stdio is initialized so we can
     * confirm firmware execution even if USB/CDC isn't enumerated. */
    gpio_init(BOARD_RED_LED_PIN);
    gpio_set_dir(BOARD_RED_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; ++i) {
        gpio_put(BOARD_RED_LED_PIN, (i & 1) ? 1 : 0);
        sleep_ms(100);
    }
    gpio_put(BOARD_RED_LED_PIN, 0);

    stdio_init_all();
    printf("\n--- Firmware boot: stdio initialized ---\n");

    gpio_init(BOARD_RED_LED_PIN);
    gpio_set_dir(BOARD_RED_LED_PIN, GPIO_OUT);
    gpio_init(BOARD_GREEN_LED_PIN);
    gpio_set_dir(BOARD_GREEN_LED_PIN, GPIO_OUT);

    lan8720_init_pins(&phy);
    
    // CRITICAL: Start RMII clock BEFORE reset. LAN8720A requires clock running during reset
    printf("Starting RMII clock on GPIO %u...\n", BOARD_ETH_REFCLK_PIN);
    rmii_refclk_start_dual(pio0, 0, BOARD_ETH_REFCLK_PIN, 1, BOARD_EXTRA_GPIO_22_PIN, (float)clock_get_hz(clk_sys));
    
    // Now perform reset with clock running
    sleep_ms(1);  // Let clock stabilize
    printf("Resetting LAN8720A with clock running...\n");
    lan8720_reset(&phy);

    sleep_ms(100);
    if (lan8720_probe(&phy)) {
        printf("LAN8720 detected at MDIO address %u\n", phy.phy_addr);
        
        // Robust PHY bring-up sequence
        printf("Starting PHY bring-up...\n");
        
        // Step 1: Soft reset and wait for completion
        printf("Soft resetting PHY...\n");
        lan8720_write_register(&phy, 0, 0x8000);
        for (int retry = 0; retry < 100; retry++) {
            uint16_t bmcr = lan8720_read_register(&phy, 0);
            if ((bmcr & 0x8000) == 0) {
                printf("Reset complete after %d ms\n", retry * 10);
                break;
            }
            sleep_ms(10);
        }
        sleep_ms(50);
        
        // Step 2: Configure advertisement register (ANAR) for 10/100 Mbps
        uint16_t anar = (1u<<5) | (1u<<6) | (1u<<7) | (1u<<8) | 0x0001u;
        printf("Setting ANAR to 0x%04x (10/100 half/full + selector)\n", anar);
        lan8720_write_register(&phy, 4, anar);
        
        sleep_ms(10);
        
        // Step 3: Enable and restart autonegotiation
        uint16_t bmcr_autoneg = (1u<<12) | (1u<<9);
        printf("Writing BMCR with autoneg enable and restart (0x%04x)\n", bmcr_autoneg);
        lan8720_write_register(&phy, 0, bmcr_autoneg);
        
        sleep_ms(10);
        
        // Step 4: Wait for autonegotiation to complete
        printf("Waiting for autonegotiation to complete (max 5000ms)...\n");
        bool autoneg_complete = false;
        for (int i = 0; i < 500; i++) {
            uint16_t bmsr = lan8720_read_register(&phy, 1);
            // Double read BMSR as per standard (first read may be cached)
            bmsr = lan8720_read_register(&phy, 1);
            
            bool link = (bmsr & (1u<<2)) != 0;
            bool autoneg_done = (bmsr & (1u<<5)) != 0;
            
            if (i % 50 == 0 || autoneg_done || link) {
                printf("  [%dms] BMSR=0x%04x link=%d autoneg_done=%d\n", 
                       i*10, bmsr, link, autoneg_done);
            }
            
            if (autoneg_done && link) {
                autoneg_complete = true;
                printf("✓ Autonegotiation complete and link up at %dms\n", i*10);
                break;
            }
            
            sleep_ms(10);
        }
        
        if (!autoneg_complete) {
            printf("⚠ WARNING: Autonegotiation did not complete\n");
        }
        
        // Final status
        uint16_t final_bmcr = lan8720_read_register(&phy, 0);
        uint16_t final_bmsr = lan8720_read_register(&phy, 1);
        uint16_t phyid1 = lan8720_read_register(&phy, 2);
        uint16_t phyid2 = lan8720_read_register(&phy, 3);
        printf("Final PHY state:\n");
        printf("  PHYID1=0x%04x PHYID2=0x%04x\n", phyid1, phyid2);
        printf("  BMCR=0x%04x BMSR=0x%04x\n", final_bmcr, final_bmsr);
    } else {
        printf("LAN8720 not detected\n");
    }

    web_server_set_phy(&phy);
    web_server_start();

    // Start heartbeat timer (1 Hz blink on BOARD_RED_LED_PIN)
    static struct repeating_timer hb_timer;
    add_repeating_timer_ms(500, heartbeat_cb, NULL, &hb_timer);

    // Disable periodic PHY status printer to keep serial output clean.
    // Use the `status` or `reg31` commands over serial to query the PHY on demand.
    g_phy_ptr_for_timer = NULL;
    // To re-enable periodic status printing, uncomment below:
    // static struct repeating_timer phy_timer;
    // add_repeating_timer_ms(2000, phy_status_cb, NULL, &phy_timer);

    multicore_launch_core1(core1_main);

    while (true) {
        tight_loop_contents();
    }

    return 0;
}