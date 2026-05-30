#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include "hardware/dma.h"

// Ring buffer containing exactly 128 stereo frames of 24-bit audio (in 32-bit words)
// (Each frame is 2 words: Left then Right)
#define AUDIO_RING_FRAMES 128
#define AUDIO_RING_WORDS (AUDIO_RING_FRAMES * 2)
extern uint32_t audio_ring_buffer[AUDIO_RING_WORDS];

// Get the current write index of the DMA in the ring buffer (0 to 127 frames)
uint32_t audio_i2s_get_write_index(void);

// Initialize PIO, DMA, and ADC
void audio_i2s_init(void);

#endif
