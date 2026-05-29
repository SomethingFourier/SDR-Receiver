#include "USB-audio_functions.hpp"

// C Libraries
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Native Pico libraries
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/types.h>

#include "dI2Srx.hpp"

// TinyUSB
extern "C" {
#include "tusb_config.h"
#include "bsp/board_api.h"
#include "tusb.h"
}

#define FIRMWARE_VERSION "1.0"
#define FIRMWARE_MODE "DIRECT"
#define AUDIO_EP_IN 0x81
#define AUDIO_SAMPLE_RATE_HZ 48000u

static uint8_t current_mute[3] = {0, 0, 0};

// UAC1 callbacks required by TinyUSB
bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    (void) rhport;
    (void) p_request;
    return true;
}

bool tud_audio_get_itf_close_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    (void) rhport;
    (void) p_request;
    return true;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request, uint8_t *pBuff)
{
    (void) rhport;
    (void) p_request;
    (void) pBuff;
    return true;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    if (TU_U16_LOW(p_request->wIndex) != AUDIO_EP_IN)
    {
        return false;
    }
    if (p_request->bRequest != AUDIO10_CS_REQ_GET_CUR || TU_U16_HIGH(p_request->wValue) != AUDIO10_EP_CTRL_SAMPLING_FREQ)
    {
        return false;
    }

    uint8_t sample_rate[3] = {
        (uint8_t) (AUDIO_SAMPLE_RATE_HZ & 0xFFu),
        (uint8_t) ((AUDIO_SAMPLE_RATE_HZ >> 8) & 0xFFu),
        (uint8_t) ((AUDIO_SAMPLE_RATE_HZ >> 16) & 0xFFu),
    };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, sample_rate, sizeof(sample_rate));
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request)
{
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);
    uint8_t control_selector = TU_U16_HIGH(p_request->wValue);

    if (entity_id == 2 && control_selector == AUDIO10_FU_CTRL_MUTE && p_request->bRequest == AUDIO10_CS_REQ_GET_CUR)
    {
        uint8_t channel = TU_U16_LOW(p_request->wValue);
        if (channel > 2u) return false;

        uint8_t control_value = current_mute[channel];
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &control_value, sizeof(control_value));
    }

    return false;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void) rhport; (void) itf; (void) ep_in; (void) cur_alt_setting;
    return true; // We manage data feeding manually in audio_task()
}

#define AUDIO_SAMPLES_PER_BUFFER 96 // 48 frames * 2 channels (stereo)

void audio_task(void)
{
    // Ensure we are configured and the host is ready for audio
    if (!tud_ready() || !tud_audio_mounted())
    {
        // Option to flush ready flags here if not connected to avoid stale data
        return;
    }

    if (g_I2Srx.A_buffer_ready)
    {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 4; // 384 bytes
        
        // If free space is getting low, drop 1 frame (8 bytes) to let the Host catch up
        // Note: The exact function name depends on your TinyUSB version.
        if (tud_audio_available() < 384) {
            bytes_to_write -= 8; 
        }
        
        // Write the completely filled DMA buffer to TinyUSB.
        // tud_audio_write expects the size in bytes. 
        // 96 32-bit words * 4 bytes/word = 384 bytes
        uint16_t written = tud_audio_write((uint8_t *)g_I2Srx.Get_A_Buffer(), AUDIO_SAMPLES_PER_BUFFER * 4);
        
        if (written > 0)
        {
            // Un-flag the buffer so DMA can overwrite it again
            g_I2Srx.A_buffer_ready = false; 
        }
    }

    if (g_I2Srx.B_buffer_ready)
    {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 4; // 384 bytes
        
        // If free space is getting low, drop 1 frame (8 bytes) to let the Host catch up
        // Note: The exact function name depends on your TinyUSB version.
        if (tud_audio_available() < 384) {
            bytes_to_write -= 8; 
        }

        uint16_t written = tud_audio_write((uint8_t *)g_I2Srx.Get_B_Buffer(), AUDIO_SAMPLES_PER_BUFFER * 4);
        
        if (written > 0) 
        {
            g_I2Srx.B_buffer_ready = false;
        }
    }
}