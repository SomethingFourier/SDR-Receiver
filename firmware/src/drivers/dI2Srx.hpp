#ifndef dI2Srx_H
#define dI2Srx_H

#include <stdlib.h>
#include <pico/types.h>
#include <hardware/pio.h>
#include <hardware/dma.h>

class dI2Srx {
    
    public:
        dI2Srx();

        // Public API
        void Init(PIO i2s_pio_instance = pio0);
        void Start();
        int * Get_A_Buffer();
        int * Get_B_Buffer();
        
        // DMA Buffer A/B Data Ready Flags
        volatile bool A_buffer_ready = false;
        volatile bool B_buffer_ready = false;

    private:
        PIO pio_instance;
        // PIO state machine index in use (0..3)
        int pio_state_machine;

        // Buffer sizing constants (lab recommended)
        static const int FRAMES_PER_HALF = 192;   // 1 ms at 192 kHz
        static const int CHANNEL_COUNT = 2;       // stereo
        static const int WORDS_PER_HALF = FRAMES_PER_HALF * CHANNEL_COUNT; // 384 words

        // Interleaved stereo half-buffers (persistent storage for DMA)
        int audio_A_buffer[WORDS_PER_HALF];
        int audio_B_buffer[WORDS_PER_HALF];

        // DMA channel indices used for ping-pong buffering
        int audio_A_dma_channel;
        int audio_B_dma_channel;

        // Core 1 entry point runs on the second core and must be callable by
        // `multicore_launch_core1`. Make it static so it can be passed as a
        // function pointer. The DMA IRQ handler needs access to private members
        // so declare it as a friend.
        static void core1_entry();
        friend void dma_irq0_handler();
};

extern dI2Srx g_I2Srx;

#endif  // dI2Srx_H