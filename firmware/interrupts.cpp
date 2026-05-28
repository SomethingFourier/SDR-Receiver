#include "interrupts.hpp"

#include <hardware/irq.h>
#include "dI2Srx.hpp"


// DMA IRQ handler (runs on Core 1). Friend of dI2Srx so it can access members.
void dma_irq0_handler()
{
    // Read interrupt status for channels
    uint32_t interrupt_status = dma_hw->ints0;

    // Channel left finished
    if (interrupt_status & (1u << g_I2Srx.audio_A_dma_channel))
    {
        // Clear the interrupt flag
        dma_hw->ints0 = (1u << g_I2Srx.audio_A_dma_channel);
        // Restore write address and transfer count so chaining will work next time
        dma_channel_set_write_addr(g_I2Srx.audio_A_dma_channel, g_I2Srx.audio_A_buffer, false);
        g_I2Srx.B_buffer_ready = false;
        g_I2Srx.A_buffer_ready = true;
    }

    // Channel right finished
    if (interrupt_status & (1u << g_I2Srx.audio_B_dma_channel))
    {
        // Clear the interrupt flag
        dma_hw->ints0 = (1u << g_I2Srx.audio_B_dma_channel);
        dma_channel_set_write_addr(g_I2Srx.audio_B_dma_channel, g_I2Srx.audio_B_buffer, false);
        g_I2Srx.A_buffer_ready = false;
        g_I2Srx.B_buffer_ready = true;
    }
}