#include "lan8720.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

enum {
    LAN8720_REG_BMCR = 0,
    LAN8720_REG_BMSR = 1,
    LAN8720_REG_PHYID1 = 2,
    LAN8720_REG_PHYID2 = 3,
    LAN8720_REG_ANAR = 4,
};

enum {
    LAN8720_BMCR_RESET = 1u << 15,
    LAN8720_BMCR_AUTO_NEG_ENABLE = 1u << 12,
    LAN8720_BMCR_RESTART_AUTO_NEG = 1u << 9,
};

enum {
    LAN8720_BMSR_LINK_STATUS = 1u << 2,
};

enum {
    LAN8720_ANAR_10_HALF = 1u << 5,
    LAN8720_ANAR_10_FULL = 1u << 6,
    LAN8720_ANAR_100_HALF = 1u << 7,
    LAN8720_ANAR_100_FULL = 1u << 8,
    LAN8720_ANAR_SELECTOR_8023 = 0x0001u,
};

static inline void mdc_low(const lan8720_t *phy) {
    gpio_put(phy->mdc_pin, 0);
}

static inline void mdc_high(const lan8720_t *phy) {
    gpio_put(phy->mdc_pin, 1);
}

static inline void mdio_drive_low(const lan8720_t *phy) {
    gpio_set_dir(phy->mdio_pin, GPIO_OUT);
    gpio_put(phy->mdio_pin, 0);
}

static inline void mdio_release(const lan8720_t *phy) {
    gpio_put(phy->mdio_pin, 1);
    gpio_set_dir(phy->mdio_pin, GPIO_IN);
    gpio_pull_up(phy->mdio_pin);
}

static inline void mdio_write_bit(const lan8720_t *phy, bool bit_value) {
    if (bit_value) {
        mdio_release(phy);
    } else {
        mdio_drive_low(phy);
    }

    sleep_us(1);
    mdc_high(phy);
    sleep_us(1);
    mdc_low(phy);
    sleep_us(1);
}

static inline bool mdio_read_bit(const lan8720_t *phy) {
    mdio_release(phy);
    sleep_us(1);
    mdc_high(phy);
    sleep_us(1);
    bool bit_value = gpio_get(phy->mdio_pin);
    mdc_low(phy);
    sleep_us(1);
    return bit_value;
}

static void mdio_send_preamble(const lan8720_t *phy) {
    for (int i = 0; i < 32; ++i) {
        mdio_write_bit(phy, true);
    }
}

static void mdio_send_bits(const lan8720_t *phy, uint32_t value, int bit_count) {
    for (int bit = bit_count - 1; bit >= 0; --bit) {
        mdio_write_bit(phy, (value >> bit) & 1u);
    }
}

void lan8720_init_pins(const lan8720_t *phy) {
    gpio_init(phy->mdc_pin);
    gpio_set_dir(phy->mdc_pin, GPIO_OUT);
    gpio_put(phy->mdc_pin, 0);

    gpio_init(phy->mdio_pin);
    mdio_release(phy);

    if (phy->reset_pin != UINT32_MAX) {
        gpio_init(phy->reset_pin);
        gpio_set_dir(phy->reset_pin, GPIO_OUT);
        gpio_put(phy->reset_pin, 0);
    }
}

void lan8720_reset(const lan8720_t *phy) {
    if (phy->reset_pin == UINT32_MAX) {
        return;
    }

    gpio_put(phy->reset_pin, 0);
    sleep_ms(10);
    gpio_put(phy->reset_pin, 1);
    sleep_ms(25);
}

uint16_t lan8720_read_register(const lan8720_t *phy, uint8_t reg) {
    mdio_send_preamble(phy);
    mdio_send_bits(phy, 0b01, 2);
    mdio_send_bits(phy, 0b10, 2);
    mdio_send_bits(phy, phy->phy_addr & 0x1fu, 5);
    mdio_send_bits(phy, reg & 0x1fu, 5);

    (void)mdio_read_bit(phy);
    (void)mdio_read_bit(phy);

    uint16_t value = 0;
    for (int bit = 15; bit >= 0; --bit) {
        value = (uint16_t)((value << 1) | (mdio_read_bit(phy) ? 1u : 0u));
    }

    mdio_release(phy);
    return value;
}

void lan8720_write_register(const lan8720_t *phy, uint8_t reg, uint16_t value) {
    mdio_send_preamble(phy);
    mdio_send_bits(phy, 0b01, 2);
    mdio_send_bits(phy, 0b01, 2);
    mdio_send_bits(phy, phy->phy_addr & 0x1fu, 5);
    mdio_send_bits(phy, reg & 0x1fu, 5);

    mdio_write_bit(phy, true);
    mdio_write_bit(phy, false);
    mdio_send_bits(phy, value, 16);
    mdio_release(phy);
}

bool lan8720_probe(lan8720_t *phy) {
    for (uint8_t addr = 0; addr < 32; ++addr) {
        phy->phy_addr = addr;
        uint16_t id1 = lan8720_read_register(phy, LAN8720_REG_PHYID1);
        uint16_t id2 = lan8720_read_register(phy, LAN8720_REG_PHYID2);
        if (id1 != 0xffffu && id1 != 0x0000u && id2 != 0xffffu && id2 != 0x0000u) {
            return true;
        }
    }

    phy->phy_addr = 0xffu;
    return false;
}

bool lan8720_start_autonegotiation(const lan8720_t *phy) {
    uint16_t anar = LAN8720_ANAR_SELECTOR_8023 |
                    LAN8720_ANAR_10_HALF |
                    LAN8720_ANAR_10_FULL |
                    LAN8720_ANAR_100_HALF |
                    LAN8720_ANAR_100_FULL;

    lan8720_write_register(phy, LAN8720_REG_ANAR, anar);
    lan8720_write_register(phy, LAN8720_REG_BMCR,
                           LAN8720_BMCR_AUTO_NEG_ENABLE |
                           LAN8720_BMCR_RESTART_AUTO_NEG);
    return true;
}

bool lan8720_link_up(const lan8720_t *phy) {
    // Read BMSR twice per IEEE 802.3 standard
    // First read may return latched status
    uint16_t status = lan8720_read_register(phy, LAN8720_REG_BMSR);
    status = lan8720_read_register(phy, LAN8720_REG_BMSR);
    
    // Bit 2: Link Status (1 = link established)
    // Bit 5: Auto-Negotiation Complete (1 = complete)
    bool link_up = (status & LAN8720_BMSR_LINK_STATUS) != 0;
    bool autoneg_complete = (status & (1u << 5)) != 0;
    
    // Link is only considered "up" if both link status and autoneg are complete
    // This ensures proper negotiation before considering the link ready
    return link_up && autoneg_complete;
}
