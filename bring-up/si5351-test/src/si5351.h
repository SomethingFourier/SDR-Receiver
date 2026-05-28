#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

typedef struct {
    uint32_t requested_hz;
    uint32_t actual_hz;
    uint32_t pll_hz;
    uint32_t reference_hz;
    uint8_t pll_multiplier;
    uint8_t multisynth_divider;
    uint8_t phase_offset;
} si5351_clock_plan_t;

bool si5351_configure_quadrature(i2c_inst_t *i2c,
                                 uint8_t i2c_address,
                                 uint32_t reference_hz,
                                 uint32_t requested_hz,
                                 si5351_clock_plan_t *plan_out);

// Enable or disable all SI5351 outputs. When `enable` is true, outputs
// are enabled; when false, all outputs are disabled.
bool si5351_enable_all_outputs(i2c_inst_t *i2c, uint8_t i2c_address, bool enable);