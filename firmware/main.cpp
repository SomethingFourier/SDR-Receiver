// C Libraries
#include <stdio.h>
#include <stdlib.h>

// Native Pico libraries
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/binary_info.h>
#include <hardware/i2c.h>

// Custom Libraries
#include "dSi5351.hpp"
#include "dSSD1306.hpp"
#include "dI2C.hpp"
#include "dI2Srx.hpp"
#include "dGPIO.hpp"

// TinyUSB
#include "USB-cdc_functions.hpp"
#include "USB-audio_functions.hpp"
#include "tusb_config.h"
#include "bsp/board_api.h"
#include "tusb.h"

// driver globals
dI2C g_I2C;
dSi5351 g_Si5351;
dSSD1306 g_SSD1306;
dI2Srx g_I2Srx;

int main (void)
{
    // pico-sdk initializations
    stdio_init_all();
    board_init();

    // driver initializations
    g_I2C.Init();
    g_SSD1306.Init();
    g_I2Srx.Init();
    g_Si5351.Init();
    tusb_init();

    g_I2Srx.Start();

    while (true)
    {
        audio_task();
        tud_task();
        cdc_task();
    }
}
