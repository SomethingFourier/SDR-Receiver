#include "dI2Srx.hpp"

#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/types.h>
#include <string.h>

#include "SDR_State_Machine.hpp"
#include "dGPIO.hpp"
#include "dSSD1306.hpp"

#include "i2s_receiver.pio.h"
#include "interrupts.hpp"

#define MIN_I2S_PIN 19
#define I2S_PIN_COUNT 3
#define I2S_SD 21
#define I2S_BCLK 19
#define I2S_WS 20

dI2Srx::dI2Srx() {}

void dI2Srx::Init(PIO i2s_pio_instance) {
  // ADC configuration pins
  gpio_init(ADC_M0);
  gpio_init(ADC_M1);
  gpio_init(ADC_RST);
  gpio_set_dir(ADC_M0, GPIO_OUT);
  gpio_set_dir(ADC_M1, GPIO_OUT);
  gpio_set_dir(ADC_RST, GPIO_OUT);
  // Ethernet
  /*
  gpio_put(ADC_MD0, 0);
  gpio_put(ADC_MD1, 1);
  */
  // USB
  ///*
  gpio_put(ADC_M0, 0);
  gpio_put(ADC_M1, 1);
  //*/
  gpio_put(ADC_RST, 0);

  // DMA configuration
  audio_A_dma_channel = dma_claim_unused_channel(true);
  audio_B_dma_channel = dma_claim_unused_channel(true);

  dma_channel_config_t dma_channel_A_config = dma_channel_get_default_config(audio_A_dma_channel);
  dma_channel_config_t dma_channel_B_config = dma_channel_get_default_config(audio_B_dma_channel);

  channel_config_set_transfer_data_size(&dma_channel_A_config, DMA_SIZE_32);
  channel_config_set_transfer_data_size(&dma_channel_B_config, DMA_SIZE_32);

  channel_config_set_read_increment(&dma_channel_A_config, false); // Don't increment read address because
  channel_config_set_read_increment(&dma_channel_B_config, false); // the PIO RX FIFO never changes location.

  channel_config_set_write_increment(&dma_channel_A_config, true); // the buffer array address (index) needs to increment
  channel_config_set_write_increment(&dma_channel_B_config, true); // the buffer array address (index) needs to increment

  // PIO I2S receiver setup
  uint sm;
  uint offset;
  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2s_receiver_program, &i2s_pio_instance, &sm, &offset, MIN_I2S_PIN, I2S_PIN_COUNT, true);
  if (success) {
    i2s_receiver_program_init(i2s_pio_instance, sm, offset, I2S_SD, I2S_BCLK, I2S_WS);
    // Don't enable PIO here, wait until DMA is started in Core 1!
    g_SSD1306.Draw_Text(0, "PIO SM Claim success!");
    g_SSD1306.Update();
  }
  else {
    g_SSD1306.Draw_Text(0, "PIO SM Claim failed!");
    g_SSD1306.Update();
  }

  if (success) {
    this->pio_instance = i2s_pio_instance;
    // DREQ must match the PIO RX FIFO for the chosen SM
    this->pio_state_machine = sm;
    uint dreq = pio_get_dreq(i2s_pio_instance, sm, false);
    channel_config_set_dreq(&dma_channel_A_config, dreq);
    channel_config_set_dreq(&dma_channel_B_config, dreq);

    // Chain A -> B, B -> A
    channel_config_set_chain_to(&dma_channel_A_config, audio_B_dma_channel);
    channel_config_set_chain_to(&dma_channel_B_config, audio_A_dma_channel);

    // Configure DMA channel A
    dma_channel_configure(
        audio_A_dma_channel, &dma_channel_A_config,
        audio_A_buffer,             // write address (RAM)
        &i2s_pio_instance->rxf[sm], // read address (PIO RX FIFO)
        WORDS_PER_HALF,             // transfer count (32-bit words)
        false                       // don't start yet
    );

    // Configure DMA channel B
    dma_channel_configure(audio_B_dma_channel, &dma_channel_B_config,
                          audio_B_buffer, &i2s_pio_instance->rxf[sm],
                          WORDS_PER_HALF, false);

    // Do not start DMA or enable IRQs here; Core 1 will do that after launch
    multicore_launch_core1(dI2Srx::core1_entry);
  }
}

void dI2Srx::Start() {
  gpio_put(ADC_RST, 1);
}

int *dI2Srx::Get_A_Buffer() {
  return audio_A_buffer;
} // Get_A_Buffer

int *dI2Srx::Get_B_Buffer() {
  return audio_B_buffer;
} // Get_B_Buffer

void dI2Srx::core1_entry() {
  // Running on Core 1
  multicore_fifo_drain();

  // Enable IRQ signalling for both channels (they will assert DMA_IRQ_0)
  dma_channel_set_irq0_enabled(g_I2Srx.audio_A_dma_channel, true);
  dma_channel_set_irq0_enabled(g_I2Srx.audio_B_dma_channel, true);

  // Register IRQ handler on Core 1 and enable the interrupt
  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  // Start the first channel; chaining will trigger the partner automatically
  dma_channel_start(g_I2Srx.audio_A_dma_channel);

  // NOW enable the PIO. It will wait for LRCK, ensuring perfect L/R sync and avoiding FIFO overflow
  pio_sm_set_enabled(g_I2Srx.pio_instance, g_I2Srx.pio_state_machine, true);

  // Remain on Core 1; DMA IRQ will do the work
  sdr_state_machine();
}
