#include "interrupts.hpp"

#include <pico/multicore.h>
#include <hardware/irq.h>
#include "dAudio.hpp"


// DMA IRQ handler (runs on Core 1). Friend of dAudio so it can access members.
void dma_irq0_handler()
{
    // Read interrupt status for channels
    uint32_t interrupt_status = dma_hw->ints0;

    // Channel left finished
    if (interrupt_status & (1u << g_Audio.audio_A_dma_channel))
    {
        // Clear the interrupt flag
        dma_hw->ints0 = (1u << g_Audio.audio_A_dma_channel);
        // Restore write address and transfer count so chaining will work next time
        dma_channel_set_write_addr(g_Audio.audio_A_dma_channel, g_Audio.audio_A_buffer, false);
        dma_channel_set_trans_count(g_Audio.audio_A_dma_channel, dAudio::WORDS_PER_HALF, false);
        // Notify Core 0 with buffer pointer (non-blocking)
        multicore_fifo_push_timeout_us((uintptr_t)g_Audio.audio_A_buffer, 0);
        g_Audio.B_buffer_ready = false;
        g_Audio.A_buffer_ready = true;
    }

    // Channel right finished
    if (interrupt_status & (1u << g_Audio.audio_B_dma_channel))
    {
        // Clear the interrupt flag
        dma_hw->ints0 = (1u << g_Audio.audio_B_dma_channel);
        dma_channel_set_write_addr(g_Audio.audio_B_dma_channel, g_Audio.audio_B_buffer, false);
        dma_channel_set_trans_count(g_Audio.audio_B_dma_channel, dAudio::WORDS_PER_HALF, false);
        multicore_fifo_push_timeout_us((uintptr_t)g_Audio.audio_B_buffer, 0);
        g_Audio.A_buffer_ready = false;
        g_Audio.B_buffer_ready = true;
    }
}