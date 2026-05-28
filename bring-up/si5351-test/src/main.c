#include <stdbool.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "si5351.h"

#define LED_PIN 4
#define STATUS_LED_PIN 5
#define BUTTON_FOR_GPIO26 2
#define BUTTON_FOR_GPIO29 3

#define LED_GPIO26 26
#define LED_GPIO29 29

#define MIRROR_GPIO29_TO 8
#define MIRROR_GPIO26_TO 9

#define BUTTON_DEBOUNCE_MS 30
#define I2C_PORT i2c0
#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1
#define I2C_BAUDRATE_HZ 100000

#define SI5351_I2C_ADDR 0x60
#define SI5351_REFERENCE_HZ 24576000u
#define SI5351_TARGET_HZ 25000000u

static void update_led_outputs(bool led26_on, bool led29_on) {
    gpio_put(LED_GPIO26, led26_on);
    gpio_put(LED_GPIO29, led29_on);
    gpio_put(MIRROR_GPIO26_TO, led26_on);
    gpio_put(MIRROR_GPIO29_TO, led29_on);
}

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 0);

    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    i2c_init(I2C_PORT, I2C_BAUDRATE_HZ);

    gpio_init(LED_GPIO26);
    gpio_set_dir(LED_GPIO26, GPIO_OUT);

    gpio_init(LED_GPIO29);
    gpio_set_dir(LED_GPIO29, GPIO_OUT);

    gpio_init(MIRROR_GPIO26_TO);
    gpio_set_dir(MIRROR_GPIO26_TO, GPIO_OUT);

    gpio_init(MIRROR_GPIO29_TO);
    gpio_set_dir(MIRROR_GPIO29_TO, GPIO_OUT);

    gpio_init(BUTTON_FOR_GPIO26);
    gpio_set_dir(BUTTON_FOR_GPIO26, GPIO_IN);
    gpio_pull_up(BUTTON_FOR_GPIO26);

    gpio_init(BUTTON_FOR_GPIO29);
    gpio_set_dir(BUTTON_FOR_GPIO29, GPIO_IN);
    gpio_pull_up(BUTTON_FOR_GPIO29);

    printf("==============================================\n");
    printf("RP2350 SI5351 Quadrature Bring-Up\n");
    printf("==============================================\n");
    printf("I2C: SDA=GPIO%d SCL=GPIO%d address=0x%02X\n", I2C_SDA_PIN, I2C_SCL_PIN, SI5351_I2C_ADDR);
    printf("Reference clock: %u Hz\n", SI5351_REFERENCE_HZ);
    printf("Requested output: %u Hz in quadrature on CLK0/CLK1\n", SI5351_TARGET_HZ);
    printf("Mode: strict integer-mode, no fractional jitter\n");

    si5351_clock_plan_t plan;
    bool configured = si5351_configure_quadrature(I2C_PORT,
                                                  SI5351_I2C_ADDR,
                                                  SI5351_REFERENCE_HZ,
                                                  SI5351_TARGET_HZ,
                                                  &plan);

    if (!configured) {
        printf("SI5351 configuration failed\n");
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
    }

    printf("SI5351 configured successfully\n");
    printf("Chosen output: %u Hz\n", plan.actual_hz);
    printf("PLL frequency: %u Hz\n", plan.pll_hz);
    printf("Multisynth divider: %u\n", plan.multisynth_divider);
    printf("Quadrature phase offset: %u\n", plan.phase_offset);
    printf("Button GPIO%d toggles GPIO%d (mirrored on GPIO%d)\n", BUTTON_FOR_GPIO26, LED_GPIO26, MIRROR_GPIO26_TO);
    printf("Button GPIO%d toggles GPIO%d (mirrored on GPIO%d)\n", BUTTON_FOR_GPIO29, LED_GPIO29, MIRROR_GPIO29_TO);

    printf("Press both buttons together to toggle SI5351 outputs on/off. SI5351 status shown on GPIO%d.\n", STATUS_LED_PIN);

    bool led26_on = false;
    bool led29_on = false;
    update_led_outputs(led26_on, led29_on);

    bool si5351_on = true;
    // Ensure the SI5351 outputs are enabled initially (full-on). The
    // configure_quadrature() above sets up PLLs and multisynths; this
    // call ensures outputs are globally enabled.
    si5351_enable_all_outputs(I2C_PORT, SI5351_I2C_ADDR, si5351_on);
    gpio_put(STATUS_LED_PIN, si5351_on);

    bool prev_button_26 = gpio_get(BUTTON_FOR_GPIO26);
    bool prev_button_29 = gpio_get(BUTTON_FOR_GPIO29);
    bool prev_both_pressed = false;
    absolute_time_t last_toggle_26 = get_absolute_time();
    absolute_time_t last_toggle_29 = get_absolute_time();
    absolute_time_t last_both_toggle = get_absolute_time();
    absolute_time_t last_heartbeat = get_absolute_time();
    absolute_time_t last_freq_print = get_absolute_time();

    while (true) {
        absolute_time_t now = get_absolute_time();

        bool button_26 = gpio_get(BUTTON_FOR_GPIO26);
        bool button_29 = gpio_get(BUTTON_FOR_GPIO29);

        if (prev_button_26 && !button_26 &&
            absolute_time_diff_us(last_toggle_26, now) > (BUTTON_DEBOUNCE_MS * 1000)) {
            led26_on = !led26_on;
            last_toggle_26 = now;
        }

        if (prev_button_29 && !button_29 &&
            absolute_time_diff_us(last_toggle_29, now) > (BUTTON_DEBOUNCE_MS * 1000)) {
            led29_on = !led29_on;
            last_toggle_29 = now;
        }

        prev_button_26 = button_26;
        prev_button_29 = button_29;

        bool both_pressed = (!button_26 && !button_29);
        if (both_pressed && !prev_both_pressed &&
            absolute_time_diff_us(last_both_toggle, now) > (BUTTON_DEBOUNCE_MS * 1000)) {
            si5351_on = !si5351_on;
            si5351_enable_all_outputs(I2C_PORT, SI5351_I2C_ADDR, si5351_on);
            gpio_put(STATUS_LED_PIN, si5351_on);
            last_both_toggle = now;
        }
        prev_both_pressed = both_pressed;

        update_led_outputs(led26_on, led29_on);

        if (absolute_time_diff_us(last_heartbeat, now) > 500000) {
            gpio_xor_mask(1u << LED_PIN);
            last_heartbeat = now;
        }

        if (absolute_time_diff_us(last_freq_print, now) > 1000000) {
            uint32_t current_hz = si5351_on ? plan.actual_hz : 0u;
            printf("SI5351 %s, CLK0/CLK1 frequency: %u Hz\n", si5351_on ? "ON" : "OFF", current_hz);
            last_freq_print = now;
        }

        sleep_ms(5);
    }

    return 0;
}
