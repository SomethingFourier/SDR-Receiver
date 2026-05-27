#include "rmii_refclk.h"

#include "hardware/gpio.h"

#include "pico/stdlib.h"

#include "rmii_refclk.pio.h"

bool rmii_refclk_start(PIO pio, uint sm, uint pin, float sys_clk_hz) {
    return rmii_refclk_start_dual(pio, sm, pin, 255u, 0u, sys_clk_hz);
}

bool rmii_refclk_start_dual(PIO pio, uint sm0, uint pin0, uint sm1, uint pin1, float sys_clk_hz) {
    uint offset = pio_add_program(pio, &rmii_refclk_program);
    pio_sm_config config = rmii_refclk_program_get_default_config(offset);

    sm_config_set_sideset_pins(&config, pin0);
    sm_config_set_clkdiv(&config, sys_clk_hz / 100000000.0f);

    gpio_init(pin0);
    pio_gpio_init(pio, pin0);
    pio_sm_set_consecutive_pindirs(pio, sm0, pin0, 1, true);

    pio_sm_init(pio, sm0, offset, &config);
    pio_sm_set_enabled(pio, sm0, true);

    if (sm1 < 4u) {
        pio_sm_config mirror_config = rmii_refclk_program_get_default_config(offset);
        sm_config_set_sideset_pins(&mirror_config, pin1);
        sm_config_set_clkdiv(&mirror_config, sys_clk_hz / 100000000.0f);

        gpio_init(pin1);
        pio_gpio_init(pio, pin1);
        pio_sm_set_consecutive_pindirs(pio, sm1, pin1, 1, true);

        pio_sm_init(pio, sm1, offset, &mirror_config);
        pio_sm_set_enabled(pio, sm1, true);
    }

    return true;
}

void rmii_refclk_stop(PIO pio, uint sm) {
    pio_sm_set_enabled(pio, sm, false);
}
