#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t mdio_pin;
    uint32_t mdc_pin;
    uint32_t reset_pin;
    uint8_t phy_addr;
} lan8720_t;

void lan8720_init_pins(const lan8720_t *phy);
void lan8720_reset(const lan8720_t *phy);
bool lan8720_probe(lan8720_t *phy);
uint16_t lan8720_read_register(const lan8720_t *phy, uint8_t reg);
void lan8720_write_register(const lan8720_t *phy, uint8_t reg, uint16_t value);
bool lan8720_start_autonegotiation(const lan8720_t *phy);
bool lan8720_link_up(const lan8720_t *phy);