#include "audio_i2s.h"
#include "hardware/pio.h"
#include "audio_i2s.pio.h"
#include "hardware/gpio.h"
#include <string.h>

// Hardware pins
#define I2S_BCLK 19
#define I2S_WS 20
#define I2S_SD 21

// ADC configuration pins
#define ADC_RST 22
#define ADC_MD0 6
#define ADC_MD1 7

// The DMA ring buffer must be aligned to its size in bytes (128 * 4 = 512)
uint32_t audio_ring_buffer[AUDIO_RING_FRAMES] __attribute__((aligned(512)));

static int dma_channel_i2s;
static pio_hw_t *pio_hw = pio0;
static uint sm = 0;

uint32_t audio_i2s_get_write_index(void) {
    uint32_t write_addr = dma_hw->ch[dma_channel_i2s].write_addr;
    return (write_addr - (uint32_t)audio_ring_buffer) / 4;
}

void audio_i2s_init(void) {
    // Setup ADC config pins
    gpio_init(ADC_MD0);
    gpio_init(ADC_MD1);
    gpio_init(ADC_RST);

    gpio_set_dir(ADC_MD0, GPIO_OUT);
    gpio_set_dir(ADC_MD1, GPIO_OUT);
    gpio_set_dir(ADC_RST, GPIO_OUT);

    // Reset ADC
    gpio_put(ADC_RST, 0);
    for(volatile int i=0; i<10000; i++);
    gpio_put(ADC_RST, 1);
    
    // Set ADC to Master Mode
    gpio_put(ADC_MD0, 0);
    gpio_put(ADC_MD1, 0);

    // PIO Init
    uint offset = pio_add_program(pio_hw, &audio_i2s_program);
    audio_i2s_program_init(pio_hw, sm, offset, I2S_SD, I2S_BCLK, I2S_WS);

    // DMA Init
    dma_channel_i2s = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_channel_i2s);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio_hw, sm, false));
    channel_config_set_ring(&c, true, 9); // 1<<9 = 512 bytes

    dma_channel_configure(
        dma_channel_i2s,
        &c,
        audio_ring_buffer,       // Destination pointer
        &pio_hw->rxf[sm],        // Source pointer
        0xFFFFFFFF,              // Infinite transfers
        true                     // Start immediately
    );

    // Start PIO state machine
    pio_sm_set_enabled(pio_hw, sm, true);
}
