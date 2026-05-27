#pragma once

#include <stdbool.h>

#include "hardware/pio.h"

bool rmii_refclk_start(PIO pio, uint sm, uint pin, float sys_clk_hz);
bool rmii_refclk_start_dual(PIO pio, uint sm0, uint pin0, uint sm1, uint pin1, float sys_clk_hz);
void rmii_refclk_stop(PIO pio, uint sm);