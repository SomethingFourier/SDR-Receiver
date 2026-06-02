#include "dSi5351.hpp"

#include <stdio.h>
#include <limits.h>
#include <pico/types.h>
#include <pico/stdlib.h>
#include <hardware/i2c.h>

#include "dI2C.hpp"

static const uint8_t SI5351_REG_DEVICE_STATUS = 0;
static const uint8_t SI5351_REG_OUTPUT_ENABLE_CTRL = 3;
static const uint8_t SI5351_REG_CLK0_CTRL = 16;
static const uint8_t SI5351_REG_CLK1_CTRL = 17;
static const uint8_t SI5351_REG_MSNA_INT = 22;
static const uint8_t SI5351_REG_MSNB_INT = 23;
static const uint8_t SI5351_REG_PLLA_BASE = 26;
static const uint8_t SI5351_REG_MS0_BASE = 42;
static const uint8_t SI5351_REG_MS1_BASE = 50;
static const uint8_t SI5351_REG_CLK0_PHOFF = 165;
static const uint8_t SI5351_REG_CLK1_PHOFF = 166;
static const uint8_t SI5351_REG_PLL_RESET = 177;
static const uint8_t SI5351_REG_XTAL_CL = 183;

dSi5351::dSi5351() {
    actual_quadrature_frequency = 0;
    requested_quadrature_frequency = 0;
    frequency_offset = 0;
    programming_request_exists = false;
    cdc_programming_response_needed = false;
} // constructor


bool dSi5351::Init() {
   uint8_t status = 0x80u;

    // Disable all outputs while configuring.
    if (!Reg_Write(SI5351_REG_OUTPUT_ENABLE_CTRL, 0xFFu))
    {
        return false;
    }

    // Set crystal load capacitance to 10 pF.
    if (!Reg_Write(SI5351_REG_XTAL_CL, 0xD2u))
    {
        return false;
    }

    // Wait for SYS_INIT (bit 7) to clear.
    for (int i = 0; i < 200; ++i)
    {
        if (!Reg_Read(SI5351_REG_DEVICE_STATUS, &status))
        {
            return false;
        }
        if ((status & 0x80u) == 0u)
        {
            return true;
        }
        sleep_ms(5);
    }
    
    return false;
} // Init()

bool dSi5351::Reg_Write(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    // Use timeout version to avoid bus hangs
    return i2c_write_timeout_us(g_I2C.master_i2c_instance, Si5351_ADDR, buffer, 2, false, 100000) == 2;
}

bool dSi5351::Reg_Read(uint8_t reg, uint8_t *value) {

    if (value == NULL) {
        return false;
    }

    // Use timeout version to avoid bus hangs
    if (i2c_write_timeout_us(g_I2C.master_i2c_instance, Si5351_ADDR, &reg, 1, true, 100000) != 1) {
        return false;
    }

    return i2c_read_timeout_us(g_I2C.master_i2c_instance, Si5351_ADDR, value, 1, false, 100000) == 1;
}

bool dSi5351::Write_Block(uint8_t start_reg, const uint8_t *data, uint8_t len) {

    if (data == NULL || len < 1) {
        return false;
    }

    uint8_t buffer[256];
    buffer[0] = start_reg;
    for (uint8_t i = 0; i < len; ++i) {
        buffer[1 + i] = data[i];
    }

    return i2c_write_timeout_us(g_I2C.master_i2c_instance, Si5351_ADDR, buffer, (size_t)(len + 1), false, 100000) == (len + 1);
}

void dSi5351::Pack_Integer_Params(uint32_t a, uint8_t out[8]) {

    uint32_t p1 = 128u * a - 512u;
    uint32_t p2 = 0u;
    uint32_t p3 = 1u;

    out[0] = ((p3 >> 8) & 0xFFu);
    out[1] = (p3 & 0xFFu);
    out[2] = (((p3 >> 12) & 0xF0u) | ((p1 >> 16) & 0x03u));
    out[3] = ((p1 >> 8) & 0xFFu);
    out[4] = (p1 & 0xFFu);
    out[5] = ((p2 >> 16) & 0x0Fu);
    out[6] = ((p2 >> 8) & 0xFFu);
    out[7] = (p2 & 0xFFu);
}

bool dSi5351::Start_Outputs() {

    uint8_t output_enable = 0;

    if (!Reg_Read(SI5351_REG_OUTPUT_ENABLE_CTRL, &output_enable)) {
        return false;
    }

    // Enable CLK0 + CLK1 by clearing bits 0 and 1.
    output_enable &= (uint8_t)(~0x03u);
    return Reg_Write(SI5351_REG_OUTPUT_ENABLE_CTRL, output_enable);
} // Start_Outputs()

bool dSi5351::Stop_Outputs() {
    return Reg_Write(SI5351_REG_OUTPUT_ENABLE_CTRL, 0xFFu);
} // Stop_Outputs()

void dSi5351::Request_Frequency_Programming(uint32_t target_frequency) {
    requested_quadrature_frequency = target_frequency;
    programming_request_exists = true;
} // Request_Frequency_Programming


int dSi5351::Set_Golden_Quadrature_Frequency(uint32_t target_frequency) {
    uint32_t best_multiplier = 0;
    uint32_t best_multisynth_divider = 0;
    uint32_t best_frequency = 0;
    uint32_t best_error = UINT_MAX;

    if (target_frequency == 0u) {
        return 0;
    }

    // Integer-mode search for N and even M in valid ranges.
    for (uint32_t n = 25; n <= 36; ++n) {
        uint32_t vco_hz = XTAL_HZ * n;
        if (vco_hz < 600000000u || vco_hz > 900000000u) {
            continue;
        }

        for (uint32_t m = 8; m <= 127; m += 2) {
            // FIX: Reject any combination that leaves a decimal remainder
            if (vco_hz % m != 0u) {
                continue;
            }

            uint32_t f_hz = vco_hz / m;
            uint32_t error = (f_hz > target_frequency) ? (f_hz - target_frequency) : (target_frequency - f_hz);

            if (error < best_error) {
                best_error = error;
                best_multiplier = n;
                best_multisynth_divider = m;
                best_frequency = f_hz;
            }
        }
    }

    if (best_multiplier == 0u || best_multisynth_divider == 0u) {
        return 0;
    }

    uint8_t pll_data[8];
    uint8_t ms0_data[8];
    uint8_t ms1_data[8];
    Pack_Integer_Params(best_multiplier, pll_data);
    Pack_Integer_Params(best_multisynth_divider, ms0_data);
    Pack_Integer_Params(best_multisynth_divider, ms1_data);

    if (!Stop_Outputs()) {
        return 0;
    }

    // Program PLLA and both multisynth outputs.
    if (!Write_Block(SI5351_REG_PLLA_BASE, pll_data, 8)) {
        return 0;
    }
    if (!Write_Block(SI5351_REG_MS0_BASE, ms0_data, 8)) {
        return 0;
    }
    if (!Write_Block(SI5351_REG_MS1_BASE, ms1_data, 8)) {
        return 0;
    }

    // Integer mode on PLLA for both clock outputs.
    if (!Reg_Write(SI5351_REG_CLK0_CTRL, 0x4Fu)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_CTRL, 0x4Fu)) {
        return 0;
    }

    // 90-degree phase shift for Q channel in integer mode.
    if (!Reg_Write(SI5351_REG_CLK0_PHOFF, 0x00u)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_PHOFF, (uint8_t)best_multisynth_divider)) {
        return 0;
    }

    if (!Reg_Write(SI5351_REG_PLL_RESET, 0x20u)) {
        return 0;
    }

    if (!Start_Outputs()) {
        return 0;
    }

    frequency_offset = target_frequency - best_frequency;
    return actual_quadrature_frequency = best_frequency;
} // Set_Golden_Frequency_Quadrature()

int dSi5351::Set_Frequency_Integer_Clk(uint32_t target_frequency, uint32_t clk) {

    if (target_frequency == 0u || clk > 2u) {
        return 0;
    }

    uint32_t best_multiplier = 0;
    uint32_t best_multisynth_divider = 0;
    uint32_t best_frequency = 0;
    uint32_t best_error = UINT_MAX;

    // Integer-mode search: N in valid PLL range, even M for integer multisynth mode.
    for (uint32_t n = 25; n <= 36; ++n) {
        uint32_t vco_hz = XTAL_HZ * n;
        if (vco_hz < 600000000u || vco_hz > 900000000u) {
            continue;
        }

        for (uint32_t m = 6; m <= 254; m += 2) {
            uint32_t f_hz = vco_hz / m;
            uint32_t error = (f_hz > target_frequency) ? (f_hz - target_frequency) : (target_frequency - f_hz);
            if (error < best_error) {
                best_error = error;
                best_multiplier = n;
                best_multisynth_divider = m;
                best_frequency = f_hz;
            }
        }
    }

    if (best_multiplier == 0u || best_multisynth_divider == 0u) {
        return 0;
    }

    uint8_t pll_data[8];
    uint8_t ms_data[8];
    Pack_Integer_Params(best_multiplier, pll_data);
    Pack_Integer_Params(best_multisynth_divider, ms_data);

    if (!Stop_Outputs()) {
        return 0;
    }

    // Program PLLA
    if (!Write_Block(SI5351_REG_PLLA_BASE, pll_data, 8)) {
        return 0;
    }

    // MS0 base = 42, MS1 base = 50, MS2 base = 58 (8 bytes each)
    uint8_t ms_base = SI5351_REG_MS0_BASE + (uint8_t)(clk * 8u);
    if (!Write_Block(ms_base, ms_data, 8)) {
        return 0;
    }

    // CLK0 ctrl = 16, CLK1 ctrl = 17, CLK2 ctrl = 18
    // 0x4F: not powered down, integer mode, PLLA source, MSn source, 8 mA drive
    uint8_t clk_ctrl_reg = SI5351_REG_CLK0_CTRL + (uint8_t)clk;
    if (!Reg_Write(clk_ctrl_reg, 0x4Fu)) {
        return 0;
    }

    // CLK0_PHOFF = 165, CLK1_PHOFF = 166, CLK2_PHOFF = 167; set to 0 (no phase shift)
    uint8_t phoff_reg = SI5351_REG_CLK0_PHOFF + (uint8_t)clk;
    if (!Reg_Write(phoff_reg, 0x00u)) {
        return 0;
    }

    // Reset PLLA (bit 5)
    if (!Reg_Write(SI5351_REG_PLL_RESET, 0x20u)) {
        return 0;
    }

    if (!Start_Outputs()) {
        return 0;
    }

    return actual_quadrature_frequency = best_frequency;
} // Set_Frequency_Integer_Clk()

int dSi5351::Get_Actual_Quadrature_Frequency() {
    return actual_quadrature_frequency;
} // Get_Actual_Frequency

int dSi5351::Get_Desired_Quadrature_Frequency() {
    return requested_quadrature_frequency;
} // Get_Desired_Frequency

int dSi5351::Get_Frequency_Offset() {
    return frequency_offset;
} // Get_Frequency_Offset

bool dSi5351::Programming_Request_Exists() {
    return programming_request_exists;
} // Programming_Request_Exists

bool dSi5351::CDC_Programming_Response_Needed() {
    return cdc_programming_response_needed;
} // Programming_Is_Complete

