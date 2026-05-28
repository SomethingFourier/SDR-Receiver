#include "dI2Srx.hpp"

#include <string.h>
#include <pico/stdlib.h>
#include <pico/types.h>
#include <pico/multicore.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <hardware/irq.h>

#include "dSSD1306.hpp"

#include "interrupts.hpp"
#include "i2s_receiver.pio.h"

#define MIN_I2S_PIN     14
#define I2S_PIN_COUNT   3
#define I2S_SD          14
#define I2S_BCLK        15
#define I2S_WS          16

dI2Srx::dI2Srx(PIO i2s_pio_instance)
{
    // DMA configuration
    dma_channel_0 = dma_claim_unused_channel(true);
    dma_channel_1 = dma_claim_unused_channel(true);
    
    dma_channel_config_t dma_channel_0_config = dma_channel_get_default_config(dma_channel_0);
    dma_channel_config_t dma_channel_1_config = dma_channel_get_default_config(dma_channel_1);

    channel_config_set_transfer_data_size(&dma_channel_0_config, DMA_SIZE_32);
    channel_config_set_transfer_data_size(&dma_channel_1_config, DMA_SIZE_32);

    channel_config_set_read_increment(&dma_channel_0_config, false);  // Don't increment read address because  
    channel_config_set_read_increment(&dma_channel_1_config, false);  // the PIO RX FIFO never changes location.

    channel_config_set_write_increment(&dma_channel_0_config, true);  // the buffer array address (index) needs to increment
    channel_config_set_write_increment(&dma_channel_1_config, true);  // the buffer array address (index) needs to increment

    // PIO I2S receiver setup
    uint sm;
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2s_receiver_program, &i2s_pio_instance, &sm, &offset, MIN_I2S_PIN, I2S_PIN_COUNT, true);
    if (success)
    {
        i2s_receiver_program_init(i2s_pio_instance, sm, offset, I2S_SD, I2S_BCLK, I2S_WS);
        pio_sm_set_enabled(i2s_pio_instance, sm, true);
        g_SSD1306.Draw_Text(0, "PIO SM Claim success!");
        g_SSD1306.Update();
    }
    else
    {
        g_SSD1306.Draw_Text(0, "PIO SM Claim failed!");
        g_SSD1306.Update();
    }

    if (success)
    {
        // DREQ must match the PIO RX FIFO for the chosen SM
        uint dreq = pio_get_dreq(i2s_pio_instance, sm, false);
        channel_config_set_dreq(&dma_channel_0_config, dreq);
        channel_config_set_dreq(&dma_channel_1_config, dreq);

        // Chain A -> B, B -> A
        channel_config_set_chain_to(&dma_channel_0_config, dma_channel_1);
        channel_config_set_chain_to(&dma_channel_1_config, dma_channel_0);

        // Configure DMA channel A
        dma_channel_configure
        (
            dma_channel_0,
            &dma_channel_0_config,
            audio_A_buffer,                 // write address (RAM)
            &i2s_pio_instance->rxf[sm],     // read address (PIO RX FIFO)
            WORDS_PER_HALF,                 // transfer count (32-bit words)
            false                           // don't start yet
        );

        // Configure DMA channel B
        dma_channel_configure
        (
            dma_channel_1,
            &dma_channel_1_config,
            audio_B_buffer,
            &i2s_pio_instance->rxf[sm],
            WORDS_PER_HALF,
            false
        );

        // Do not start DMA or enable IRQs here; Core 1 will do that after launch
        multicore_launch_core1(dI2Srx::core1_entry);
    }
}

int * dI2Srx::Get_A_Buffer()
{
    return audio_A_buffer;
} // Get_A_Buffer

int * dI2Srx::Get_B_Buffer()
{
    return audio_B_buffer;
} // Get_B_Buffer

void dI2Srx::Fix_A_Samples()
{
    for (int i = 0; i < 96; i++)
    {
        // Shift left by 1 to move the sign bit from bit 30 to bit 31
        audio_A_buffer[i] <<= 1; 
    }
} // Fix_A_Samples

void dI2Srx::Fix_B_Samples()
{
    for (int i = 0; i < 96; i++)
    {
        audio_B_buffer[i] <<= 1;
    }
} // Fix_B_Samples

void dI2Srx::core1_entry()
{
    // Running on Core 1
    multicore_fifo_drain();

    // Enable IRQ signalling for both channels (they will assert DMA_IRQ_0)
    dma_channel_set_irq0_enabled(g_I2Srx.dma_channel_0, true);
    dma_channel_set_irq0_enabled(g_I2Srx.dma_channel_1, true);

    // Register IRQ handler on Core 1 and enable the interrupt
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Start the first channel; chaining will trigger the partner automatically
    dma_channel_start(g_I2Srx.dma_channel_0);

    // Remain on Core 1; DMA IRQ will do the work
    for (;;) tight_loop_contents();
}
