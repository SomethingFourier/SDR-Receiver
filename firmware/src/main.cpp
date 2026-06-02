// Native Pico libraries
#include <hardware/i2c.h>
#include <pico/binary_info.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>

// Custom Libraries
#include "dGPIO.hpp"
#include "dI2C.hpp"
#include "dI2Srx.hpp"
#include "dMUX.hpp"
#include "dSSD1306.hpp"
#include "dSi5351.hpp"

// TinyUSB
#include "USB-audio_functions.hpp"
#include "USB-cdc_functions.hpp"
#include <bsp/board_api.h>
#include <tusb.h>
#include <tusb_config.h>

// driver globals
dI2C g_I2C;
dSi5351 g_Si5351;
dSSD1306 g_SSD1306;
dI2Srx g_I2Srx;
dMUX g_MUX;

int loop_timer = 0;
int loop_duration = 0;
int max_loop_duration = 0;

void debug_task() {
  // printf("Loop duration: %d ms (max: %d ms)\n", loop_duration,
  // max_loop_duration);
}

int main(void) {
  // LED config
  gpio_init(LED_RED);
  gpio_init(LED_GREEN);
  gpio_init(LED_WHITE);
  gpio_init(LED_YELLOW);
  gpio_set_dir(LED_RED, GPIO_OUT);
  gpio_set_dir(LED_GREEN, GPIO_OUT);
  gpio_set_dir(LED_WHITE, GPIO_OUT);
  gpio_set_dir(LED_YELLOW, GPIO_OUT);
  gpio_put(LED_RED, 0);
  gpio_put(LED_GREEN, 0);
  gpio_put(LED_WHITE, 0);
  gpio_put(LED_YELLOW, 0);

  // pico-sdk initializations
  stdio_init_all();
  board_init();

  // driver initializations
  g_I2C.Init();
  g_SSD1306.Init();
  g_I2Srx.Init();
  if (g_Si5351.Init()) {
    g_SSD1306.Draw_Text(4, "clk init yay");
  }
  else {
    g_SSD1306.Draw_Text(4, "clk no init :(");
  }
  g_SSD1306.Update();
  tusb_init();

  g_I2Srx.Start();

  while (true) {
    loop_timer = to_ms_since_boot(get_absolute_time()); // Start a stopwatch to measure loop duration

    debug_task();
    audio_task();
    tud_task();
    cdc_task();

    loop_duration = to_ms_since_boot(get_absolute_time()) - loop_timer; // Read stopwatch value to get loop duration
    if (loop_duration > max_loop_duration) {
      max_loop_duration = loop_duration; // Update max loop duration if current loop is longer
    }
  }
}
