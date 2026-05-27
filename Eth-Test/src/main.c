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
    uint16_t bmsr = lan8720_read_register(p, 1);

    bool link = (bmsr & (1u << 2)) != 0;
    bool autoneg = (bmcr & (1u << 12)) != 0;

    printf("PHY addr=%u id1=0x%04x id2=0x%04x BMCR=0x%04x BMSR=0x%04x link=%d autoneg=%d\n",
           p->phy_addr, phyid1, phyid2, bmcr, bmsr, link, autoneg);

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
    rmii_refclk_start_dual(pio0, 0, BOARD_ETH_REFCLK_PIN, 1, BOARD_EXTRA_GPIO_22_PIN, (float)clock_get_hz(clk_sys));
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
        
        // Step 2: Configure advertisement register (ANAR)
        uint16_t anar = (1u<<5) | (1u<<6) | (1u<<7) | (1u<<8) | 0x0001u;
        printf("Setting ANAR to 0x%04x (10/100 half/full + selector)\n", anar);
        lan8720_write_register(&phy, 4, anar);
        
        // Verify write
        uint16_t anar_read = lan8720_read_register(&phy, 4);
        printf("ANAR read back as 0x%04x\n", anar_read);
        
        // Step 3: Enable and restart autonegotiation
        uint16_t bmcr_autoneg = (1u<<12) | (1u<<9);
        printf("Writing BMCR with autoneg enable (0x%04x)\n", bmcr_autoneg);
        lan8720_write_register(&phy, 0, bmcr_autoneg);
        
        // Verify BMCR write
        uint16_t bmcr_read = lan8720_read_register(&phy, 0);
        printf("BMCR read back as 0x%04x\n", bmcr_read);
        
        // Step 4: Wait for autoneg to complete (poll BMSR bit 5 = autoneg complete)
        printf("Waiting for autonegotiation to complete (max 10s)...\n");
        for (int i = 0; i < 100; i++) {
            uint16_t bmsr = lan8720_read_register(&phy, 1);
            bmsr = lan8720_read_register(&phy, 1);  // Read twice as per standard
            bool link = (bmsr & (1u<<2)) != 0;
            bool autoneg_done = (bmsr & (1u<<5)) != 0;
            
            if (autoneg_done || link) {
                printf("Autoneg done at %d s: BMSR=0x%04x link=%d\n", i/10, bmsr, link);
                break;
            }
            if (i % 10 == 0) {
                printf("  ...waiting (%d/%d): BMSR=0x%04x\n", i, 100, bmsr);
            }
            sleep_ms(100);
        }
        
        // Final status
        uint16_t final_bmcr = lan8720_read_register(&phy, 0);
        uint16_t final_bmsr = lan8720_read_register(&phy, 1);
        printf("Final PHY state: BMCR=0x%04x BMSR=0x%04x\n", final_bmcr, final_bmsr);
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