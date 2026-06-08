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
static const uint8_t SI5351_REG_MS2_BASE = 58;
static const uint8_t SI5351_REG_CLK2_CTRL = 18;
static const uint8_t SI5351_REG_CLK0_PHOFF = 165;
static const uint8_t SI5351_REG_CLK1_PHOFF = 166;
static const uint8_t SI5351_REG_CLK2_PHOFF = 167;
static const uint8_t SI5351_REG_PLL_RESET = 177;
static const uint8_t SI5351_REG_XTAL_CL = 183;

static uint32_t Round_Divide(uint64_t numerator, uint32_t denominator) {
    return (uint32_t)((numerator + (uint64_t)(denominator / 2u)) / (uint64_t)denominator);
}

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
    out[2] = ((p1 >> 16) & 0x03u); // R_DIV = 000, DIVBY4 = 0
    out[3] = ((p1 >> 8) & 0xFFu);
    out[4] = (p1 & 0xFFu);
    out[5] = (((p3 >> 12) & 0xF0u) | ((p2 >> 16) & 0x0Fu));
    out[6] = ((p2 >> 8) & 0xFFu);
    out[7] = (p2 & 0xFFu);
} // Pack_Integer_Params()

void dSi5351::Pack_Fractional_Params(uint32_t a, uint32_t b, uint32_t c, uint8_t out[8]) {
    uint32_t p1 = 128u * a + (uint32_t)(128ULL * b / c) - 512u;
    uint32_t p2 = 128u * b - c * (uint32_t)(128ULL * b / c);
    uint32_t p3 = c;

    out[0] = ((p3 >> 8) & 0xFFu);
    out[1] = (p3 & 0xFFu);
    out[2] = ((p1 >> 16) & 0x03u); // R_DIV = 000, DIVBY4 = 0
    out[3] = ((p1 >> 8) & 0xFFu);
    out[4] = (p1 & 0xFFu);
    out[5] = (((p3 >> 12) & 0xF0u) | ((p2 >> 16) & 0x0Fu));
    out[6] = ((p2 >> 8) & 0xFFu);
    out[7] = (p2 & 0xFFu);
} // Pack_Fractional_Params()

bool dSi5351::Start_Outputs() {

    uint8_t output_enable = 0;

    if (!Reg_Read(SI5351_REG_OUTPUT_ENABLE_CTRL, &output_enable)) {
        return false;
    }

    // Enable CLK0 + CLK1 + CLK2 by clearing bits 0, 1, and 2.
    output_enable &= (uint8_t)(~0x07u);
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
    uint32_t best_ms_b = 0;
    uint32_t best_ms_c = 1;
    uint32_t best_frequency = 0;
    uint32_t best_error = UINT_MAX;

    if (target_frequency == 0u) {
        return 0;
    }

#if SI5351_CLK0_CLK1_INTEGER_MODE
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
#else
    for (uint32_t n = 25; n <= 36; ++n) {
        uint32_t vco_hz = XTAL_HZ * n;
        if (vco_hz < 600000000u || vco_hz > 900000000u) {
            continue;
        }

        double ms_div = (double)vco_hz / target_frequency;
        if (ms_div < 8.0 || ms_div > 900.0) continue;

        uint32_t a = vco_hz / target_frequency;
        uint32_t remainder = vco_hz % target_frequency;
        uint32_t b = (uint32_t)(((uint64_t)remainder * 1048575ULL) / target_frequency);
        uint32_t c = 1048575;

        best_error = 0;
        best_multiplier = n;
        best_multisynth_divider = a;
        best_ms_b = b;
        best_ms_c = c;
        best_frequency = target_frequency;
    }
#endif

    if (best_multiplier == 0u || best_multisynth_divider == 0u) {
        return 0;
    }

    uint8_t pll_data[8];
    uint8_t ms0_data[8];
    uint8_t ms1_data[8];
    Pack_Integer_Params(best_multiplier, pll_data);
#if SI5351_CLK0_CLK1_INTEGER_MODE
    Pack_Integer_Params(best_multisynth_divider, ms0_data);
    Pack_Integer_Params(best_multisynth_divider, ms1_data);
#else
    Pack_Fractional_Params(best_multisynth_divider, best_ms_b, best_ms_c, ms0_data);
    Pack_Fractional_Params(best_multisynth_divider, best_ms_b, best_ms_c, ms1_data);
#endif

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

#if SI5351_CLK0_CLK1_INTEGER_MODE
    // Integer mode on PLLA for both clock outputs.
    if (!Reg_Write(SI5351_REG_CLK0_CTRL, 0x4Fu)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_CTRL, 0x4Fu)) {
        return 0;
    }
#else
    // Fractional mode on PLLA for both clock outputs.
    if (!Reg_Write(SI5351_REG_CLK0_CTRL, 0x0Fu)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_CTRL, 0x0Fu)) {
        return 0;
    }
#endif

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
    actual_clk2_frequency = 0;
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
            uint32_t f_hz = Round_Divide((uint64_t)vco_hz, m);
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

    if (clk == 2u) {
        return actual_clk2_frequency = best_frequency;
    } else {
        return actual_quadrature_frequency = best_frequency;
    }
} // Set_Frequency_Integer_Clk()

int dSi5351::Set_VHF_Quadrature_Frequency(uint32_t target_frequency, uint32_t if_center_frequency, uint32_t if_span_hz) {

    if (target_frequency == 0u || if_center_frequency == 0u) {
        return 0;
    }

    uint32_t if_low = (if_center_frequency > if_span_hz) ? (if_center_frequency - if_span_hz) : 0u;
    uint32_t if_high = if_center_frequency + if_span_hz;

    uint32_t best_multiplier = 0;
    uint32_t best_if_divider = 0;
    uint32_t best_if_b = 0;
    uint32_t best_if_c = 1;
    uint32_t best_clk_divider = 0;
    uint32_t best_clk_b = 0;
    uint32_t best_clk_c = 1;
    uint32_t best_if_frequency = 0;
    uint32_t best_clk_frequency = 0;
    uint32_t best_total_frequency = 0;
    uint32_t best_error = UINT_MAX;
    uint32_t best_if_center_error = UINT_MAX;

    for (uint32_t n = 25; n <= 36; ++n) {
        uint32_t vco_hz = XTAL_HZ * n;
        if (vco_hz < 600000000u || vco_hz > 900000000u) {
            continue;
        }

        for (uint32_t if_divider = 8; if_divider <= 126; if_divider += 2) {
#if SI5351_CLK0_CLK1_INTEGER_MODE
            if ((vco_hz % if_divider) != 0u) {
                continue;
            }

            uint32_t if_frequency = vco_hz / if_divider;
            if (if_frequency < if_low || if_frequency > if_high) {
                continue;
            }
#else
            if (if_divider > 8) break; // only need to run once per VCO frequency for fractional
            
            uint32_t if_frequency = if_center_frequency;
            double ms_div = (double)vco_hz / if_frequency;
            if (ms_div < 8.0 || ms_div > 900.0) continue;
            uint32_t if_a = vco_hz / if_frequency;
            uint32_t if_rem = vco_hz % if_frequency;
            uint32_t if_b_val = (uint32_t)(((uint64_t)if_rem * 1048575ULL) / if_frequency);
            uint32_t if_c_val = 1048575;
#endif

#if SI5351_CLK2_INTEGER_MODE
            for (uint32_t clk_divider = 6; clk_divider <= 254; clk_divider += 2) {
                uint32_t clk_frequency = Round_Divide((uint64_t)vco_hz, clk_divider);
                uint32_t total_frequency = if_frequency + clk_frequency;
                uint32_t error = (total_frequency > target_frequency) ? (total_frequency - target_frequency) : (target_frequency - total_frequency);
                uint32_t if_center_error = (if_frequency > if_center_frequency) ? (if_frequency - if_center_frequency) : (if_center_frequency - if_frequency);

                if (error < best_error || (error == best_error && if_center_error < best_if_center_error)) {
                    best_error = error;
                    best_if_center_error = if_center_error;
                    best_multiplier = n;
#if SI5351_CLK0_CLK1_INTEGER_MODE
                    best_if_divider = if_divider;
#else
                    best_if_divider = if_a;
                    best_if_b = if_b_val;
                    best_if_c = if_c_val;
#endif
                    best_clk_divider = clk_divider;
                    best_if_frequency = if_frequency;
                    best_clk_frequency = clk_frequency;
                    best_total_frequency = total_frequency;
                }
            }
#else
            uint32_t clk_frequency = (target_frequency > if_frequency) ? (target_frequency - if_frequency) : 0;
            if (clk_frequency == 0) continue;
            double clk_div = (double)vco_hz / clk_frequency;
            if (clk_div < 6.0 || clk_div > 900.0) continue;
            
            uint32_t clk_a = vco_hz / clk_frequency;
            uint32_t clk_rem = vco_hz % clk_frequency;
            uint32_t clk_b_val = (uint32_t)(((uint64_t)clk_rem * 1048575ULL) / clk_frequency);
            uint32_t clk_c_val = 1048575;
            
            uint32_t total_frequency = if_frequency + clk_frequency;
            uint32_t error = (total_frequency > target_frequency) ? (total_frequency - target_frequency) : (target_frequency - total_frequency);
            uint32_t if_center_error = (if_frequency > if_center_frequency) ? (if_frequency - if_center_frequency) : (if_center_frequency - if_frequency);

            if (error < best_error || (error == best_error && if_center_error < best_if_center_error)) {
                best_error = error;
                best_if_center_error = if_center_error;
                best_multiplier = n;
#if SI5351_CLK0_CLK1_INTEGER_MODE
                best_if_divider = if_divider;
#else
                best_if_divider = if_a;
                best_if_b = if_b_val;
                best_if_c = if_c_val;
#endif
                best_clk_divider = clk_a;
                best_clk_b = clk_b_val;
                best_clk_c = clk_c_val;
                best_if_frequency = if_frequency;
                best_clk_frequency = clk_frequency;
                best_total_frequency = total_frequency;
            }
#endif
        }
    }

    if (best_multiplier == 0u || best_if_divider == 0u || best_clk_divider == 0u) {
        return 0;
    }

    uint8_t pll_data[8];
    uint8_t if_data[8];
    uint8_t clk_data[8];
    Pack_Integer_Params(best_multiplier, pll_data);
#if SI5351_CLK0_CLK1_INTEGER_MODE
    Pack_Integer_Params(best_if_divider, if_data);
#else
    Pack_Fractional_Params(best_if_divider, best_if_b, best_if_c, if_data);
#endif

#if SI5351_CLK2_INTEGER_MODE
    Pack_Integer_Params(best_clk_divider, clk_data);
#else
    Pack_Fractional_Params(best_clk_divider, best_clk_b, best_clk_c, clk_data);
#endif

    if (!Stop_Outputs()) {
        return 0;
    }

    if (!Write_Block(SI5351_REG_PLLA_BASE, pll_data, 8)) {
        return 0;
    }
    if (!Write_Block(SI5351_REG_MS0_BASE, if_data, 8)) {
        return 0;
    }
    if (!Write_Block(SI5351_REG_MS1_BASE, if_data, 8)) {
        return 0;
    }
    if (!Write_Block(SI5351_REG_MS2_BASE, clk_data, 8)) {
        return 0;
    }

#if SI5351_CLK0_CLK1_INTEGER_MODE
    if (!Reg_Write(SI5351_REG_CLK0_CTRL, 0x4Fu)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_CTRL, 0x4Fu)) {
        return 0;
    }
#else
    if (!Reg_Write(SI5351_REG_CLK0_CTRL, 0x0Fu)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_CTRL, 0x0Fu)) {
        return 0;
    }
#endif

#if SI5351_CLK2_INTEGER_MODE
    if (!Reg_Write(SI5351_REG_CLK2_CTRL, 0x4Fu)) {
        return 0;
    }
#else
    if (!Reg_Write(SI5351_REG_CLK2_CTRL, 0x0Fu)) {
        return 0;
    }
#endif

    if (!Reg_Write(SI5351_REG_CLK0_PHOFF, 0x00u)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK1_PHOFF, (uint8_t)best_if_divider)) {
        return 0;
    }
    if (!Reg_Write(SI5351_REG_CLK2_PHOFF, 0x00u)) {
        return 0;
    }

    if (!Reg_Write(SI5351_REG_PLL_RESET, 0x20u)) {
        return 0;
    }

    if (!Start_Outputs()) {
        return 0;
    }

    requested_quadrature_frequency = (int)target_frequency;
    frequency_offset = (int)(target_frequency - best_total_frequency);
    actual_quadrature_frequency = (int)best_if_frequency;
    actual_clk2_frequency = (int)best_clk_frequency;
    return (int)best_total_frequency;
} // Set_VHF_Quadrature_Frequency()

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

