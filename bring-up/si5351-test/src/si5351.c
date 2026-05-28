#include "si5351.h"

#include <stddef.h>
#include <stdint.h>

#define SI5351_OUTPUT_ENABLE_CONTROL 3
#define SI5351_PLL_INPUT_SOURCE 15
#define SI5351_CLK0_CONTROL 16
#define SI5351_CLK1_CONTROL 17
#define SI5351_PLLA_PARAMETERS 26
#define SI5351_PLLB_PARAMETERS 34
#define SI5351_MS0_PARAMETERS 42
#define SI5351_MS1_PARAMETERS 50
#define SI5351_CLK0_PHASE_OFFSET 165
#define SI5351_CLK1_PHASE_OFFSET 166
#define SI5351_PLL_RESET 177
#define SI5351_CRYSTAL_LOAD 183

#define SI5351_MIN_PLL_HZ 600000000u
#define SI5351_MAX_PLL_HZ 900000000u
#define SI5351_MIN_MULTISYNTH_DIVIDER 8u
#define SI5351_MAX_MULTISYNTH_DIVIDER 1800u

#define SI5351_DEFAULT_CRYSTAL_LOAD 0xD2
#define SI5351_OUTPUT_CONTROL_INT_MODE 0x4F

typedef struct {
    uint8_t pll_multiplier;
    uint8_t multisynth_divider;
    uint8_t phase_offset;
    uint32_t pll_hz;
    uint32_t actual_hz;
    uint32_t error_hz;
    bool valid;
} si5351_candidate_t;

static bool si5351_write_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_write_blocking(i2c, addr, buffer, sizeof(buffer), false) == (int)sizeof(buffer);
}

static bool si5351_write_regs(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, const uint8_t *values, size_t length) {
    uint8_t buffer[9];

    if (length > sizeof(buffer) - 1u) {
        return false;
    }

    buffer[0] = reg;
    for (size_t index = 0; index < length; ++index) {
        buffer[index + 1] = values[index];
    }

    return i2c_write_blocking(i2c, addr, buffer, length + 1u, false) == (int)(length + 1u);
}

static void si5351_pack_divider(uint32_t divider, uint32_t numerator, uint32_t denominator, uint8_t output[8]) {
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;

    if (numerator == 0u) {
        p1 = 128u * divider - 512u;
        p2 = 0u;
        p3 = 1u;
    } else {
        uint32_t quotient = (128u * numerator) / denominator;
        p1 = 128u * divider + quotient - 512u;
        p2 = 128u * numerator - denominator * quotient;
        p3 = denominator;
    }

    output[0] = (uint8_t)((p3 >> 8) & 0xFFu);
    output[1] = (uint8_t)(p3 & 0xFFu);
    output[2] = (uint8_t)(((p1 >> 16) & 0x03u) | ((p3 >> 12) & 0xF0u));
    output[3] = (uint8_t)((p1 >> 8) & 0xFFu);
    output[4] = (uint8_t)(p1 & 0xFFu);
    output[5] = (uint8_t)(((p3 >> 16) & 0x0Fu) | ((p2 >> 12) & 0xF0u));
    output[6] = (uint8_t)((p2 >> 8) & 0xFFu);
    output[7] = (uint8_t)(p2 & 0xFFu);
}

static bool si5351_program_pll(i2c_inst_t *i2c, uint8_t addr, uint8_t base_reg, uint8_t multiplier) {
    uint8_t regs[8];

    if (multiplier < 15u || multiplier > 90u) {
        return false;
    }

    si5351_pack_divider(multiplier, 0u, 1u, regs);
    return si5351_write_regs(i2c, addr, base_reg, regs, sizeof(regs));
}

static bool si5351_program_multisynth(i2c_inst_t *i2c, uint8_t addr, uint8_t base_reg, uint8_t divider) {
    uint8_t regs[8];

    if (divider < SI5351_MIN_MULTISYNTH_DIVIDER || divider > SI5351_MAX_MULTISYNTH_DIVIDER) {
        return false;
    }

    si5351_pack_divider(divider, 0u, 1u, regs);
    return si5351_write_regs(i2c, addr, base_reg, regs, sizeof(regs));
}

static bool si5351_find_quadrature_plan(uint32_t reference_hz,
                                        uint32_t requested_hz,
                                        si5351_candidate_t *candidate_out) {
    si5351_candidate_t best = {0};
    best.error_hz = UINT32_MAX;

    for (uint32_t multiplier = 15u; multiplier <= 90u; ++multiplier) {
        uint64_t pll_hz = (uint64_t)reference_hz * multiplier;

        if (pll_hz < SI5351_MIN_PLL_HZ || pll_hz > SI5351_MAX_PLL_HZ) {
            continue;
        }

        for (uint32_t divider = SI5351_MIN_MULTISYNTH_DIVIDER; divider <= SI5351_MAX_MULTISYNTH_DIVIDER; divider += 4u) {
            // SI5351 phase register units are VCO/4 clocks. For a 90-degree
            // shift on an output divider of N, phase offset must be N.
            uint32_t phase_offset = divider;

            if (phase_offset > 127u) {
                break;
            }

            uint32_t actual_hz = (uint32_t)(pll_hz / divider);
            uint32_t error_hz = (actual_hz > requested_hz) ? (actual_hz - requested_hz) : (requested_hz - actual_hz);

            if (!best.valid || error_hz < best.error_hz || (error_hz == best.error_hz && actual_hz < best.actual_hz)) {
                best.valid = true;
                best.pll_multiplier = (uint8_t)multiplier;
                best.multisynth_divider = (uint8_t)divider;
                best.phase_offset = (uint8_t)phase_offset;
                best.pll_hz = (uint32_t)pll_hz;
                best.actual_hz = actual_hz;
                best.error_hz = error_hz;
            }
        }
    }

    if (!best.valid) {
        return false;
    }

    *candidate_out = best;
    return true;
}

bool si5351_configure_quadrature(i2c_inst_t *i2c,
                                 uint8_t i2c_address,
                                 uint32_t reference_hz,
                                 uint32_t requested_hz,
                                 si5351_clock_plan_t *plan_out) {
    si5351_candidate_t plan;

    if (i2c == NULL) {
        return false;
    }

    if (!si5351_find_quadrature_plan(reference_hz, requested_hz, &plan)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_OUTPUT_ENABLE_CONTROL, 0xFFu)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_PLL_INPUT_SOURCE, 0x00u)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_CRYSTAL_LOAD, SI5351_DEFAULT_CRYSTAL_LOAD)) {
        return false;
    }

    if (!si5351_program_pll(i2c, i2c_address, SI5351_PLLA_PARAMETERS, plan.pll_multiplier)) {
        return false;
    }

    if (!si5351_program_pll(i2c, i2c_address, SI5351_PLLB_PARAMETERS, plan.pll_multiplier)) {
        return false;
    }

    if (!si5351_program_multisynth(i2c, i2c_address, SI5351_MS0_PARAMETERS, plan.multisynth_divider)) {
        return false;
    }

    if (!si5351_program_multisynth(i2c, i2c_address, SI5351_MS1_PARAMETERS, plan.multisynth_divider)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_CLK0_CONTROL, SI5351_OUTPUT_CONTROL_INT_MODE)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_CLK1_CONTROL, SI5351_OUTPUT_CONTROL_INT_MODE)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_CLK0_PHASE_OFFSET, 0x00u)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_CLK1_PHASE_OFFSET, plan.phase_offset)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_PLL_RESET, 0xA0u)) {
        return false;
    }

    if (!si5351_write_reg(i2c, i2c_address, SI5351_OUTPUT_ENABLE_CONTROL, 0xFCu)) {
        return false;
    }

    if (plan_out != NULL) {
        plan_out->requested_hz = requested_hz;
        plan_out->actual_hz = plan.actual_hz;
        plan_out->pll_hz = plan.pll_hz;
        plan_out->reference_hz = reference_hz;
        plan_out->pll_multiplier = plan.pll_multiplier;
        plan_out->multisynth_divider = plan.multisynth_divider;
        plan_out->phase_offset = plan.phase_offset;
    }

    return true;
}

bool si5351_enable_all_outputs(i2c_inst_t *i2c, uint8_t i2c_address, bool enable) {
    if (i2c == NULL) return false;

    // Output Enable Control (reg 3): write 0xFF to disable all outputs,
    // write 0x00 to enable all outputs. Existing code used 0xFC to enable
    // only CLK0/CLK1; here we expose full-on/full-off behavior for toggling.
    uint8_t val = enable ? 0x00u : 0xFFu;
    return si5351_write_reg(i2c, i2c_address, SI5351_OUTPUT_ENABLE_CONTROL, val);
}