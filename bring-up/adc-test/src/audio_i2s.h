#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include "hardware/dma.h"

// Ring buffer containing exactly 128 stereo frames of 16-bit audio
// (Stored packed as 32-bit words: Left channel in LSB, Right channel in MSB)
#define AUDIO_RING_FRAMES 128
extern uint32_t audio_ring_buffer[AUDIO_RING_FRAMES];

// Get the current write index of the DMA in the ring buffer (0 to 127)
uint32_t audio_i2s_get_write_index(void);

// Initialize PIO, DMA, and ADC
void audio_i2s_init(void);

#endif
