#include "USB-audio_functions.hpp"

// Native Pico libraries
#include <pico/types.h>

#include "dGPIO.hpp"
#include "dI2Srx.hpp"

// TinyUSB
extern "C" {
// #include "tusb_config.h"
// #include "bsp/board_api.h"
// #include "tusb.h"
}

#define FIRMWARE_VERSION "1.0"
#define FIRMWARE_MODE "DIRECT"
#define AUDIO_EP_IN 0x81
#define AUDIO_SAMPLE_RATE_HZ 192000u

// Set to 0 to disable FIR phase correction entirely
#define ENABLE_FIR_FILTER 1

static uint8_t current_mute[3] = {0, 0, 0};

// UAC1 callbacks required by TinyUSB
bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    (void) rhport;
    (void) p_request;
    
    // Clear the stale data out of the TinyUSB FIFO when stream state changes
    tud_audio_clear_ep_in_ff();
    
    // Reset our I2S flags so we don't immediately write stale buffer data
    g_I2Srx.A_buffer_ready = false;
    g_I2Srx.B_buffer_ready = false;

    return true;
}
/*
bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    (void) rhport;
    (void) p_request;
    return true;
}
    */

bool tud_audio_get_itf_close_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    (void) rhport;
    (void) p_request;
    return true;
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request, uint8_t *pBuff) {
    (void) rhport;
    (void) p_request;
    (void) pBuff;
    return true;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    if (TU_U16_LOW(p_request->wIndex) != AUDIO_EP_IN) {
        return false;
    }
    if (p_request->bRequest != AUDIO10_CS_REQ_GET_CUR || TU_U16_HIGH(p_request->wValue) != AUDIO10_EP_CTRL_SAMPLING_FREQ) {
        return false;
    }

    uint8_t sample_rate[3] = {
        (uint8_t) (AUDIO_SAMPLE_RATE_HZ & 0xFFu),
        (uint8_t) ((AUDIO_SAMPLE_RATE_HZ >> 8) & 0xFFu),
        (uint8_t) ((AUDIO_SAMPLE_RATE_HZ >> 16) & 0xFFu),
    };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, sample_rate, sizeof(sample_rate));
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);
    uint8_t control_selector = TU_U16_HIGH(p_request->wValue);

    if (entity_id == 2 && control_selector == AUDIO10_FU_CTRL_MUTE && p_request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        uint8_t channel = TU_U16_LOW(p_request->wValue);
        if (channel > 2u) return false;

        uint8_t control_value = current_mute[channel];
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &control_value, sizeof(control_value));
    }

    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request, uint8_t *pBuff) {
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);
    uint8_t control_selector = TU_U16_HIGH(p_request->wValue);

    if (entity_id == 2 && control_selector == AUDIO10_FU_CTRL_MUTE && p_request->bRequest == AUDIO10_CS_REQ_SET_CUR) {
        uint8_t channel = TU_U16_LOW(p_request->wValue);
        if (channel > 2u) return false;

        current_mute[channel] = pBuff[0];
        return true;
    }

    return false;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting) {
    (void) rhport; (void) itf; (void) ep_in; (void) cur_alt_setting;
    return true; // We manage data feeding manually in audio_task()
}

#define AUDIO_SAMPLES_PER_BUFFER 384 // 192 frames * 2 channels (stereo)

#define FIR_TAPS 31
#define GROUP_DELAY ((FIR_TAPS - 1) / 2)

const float right_channel_fir[FIR_TAPS] = {
    -0.0000006992f,
    0.0000093150f,
    -0.0000483204f,
    0.0001594884f,
    -0.0004224311f,
    0.0009667356f,
    -0.0019844381f,
    0.0037424961f,
    -0.0065991381f,
    0.0110382664f,
    -0.0177613078f,
    0.0279491894f,
    -0.0440787946f,
    0.0730594196f,
    -0.1453025750f,
    0.9440164214f,
    0.2123591743f,
    -0.0881739233f,
    0.0499555018f,
    -0.0306980759f,
    0.0191451991f,
    -0.0117503580f,
    0.0069623657f,
    -0.0039221223f,
    0.0020688758f,
    -0.0010036781f,
    0.0004370807f,
    -0.0001645511f,
    0.0000497345f,
    -0.0000095679f,
    0.0000007169f,
};

static float right_fir_history[FIR_TAPS] = {0};
static int16_t left_delay_history[GROUP_DELAY] = {0};
static int fir_idx = 0;
static int delay_idx = 0;

void apply_phase_correction(int16_t* buffer, int num_samples) {
#if ENABLE_FIR_FILTER
    for (int i = 0; i < num_samples; i += 2) {
        // --- Left Channel (Integer Delay) ---
        int16_t new_left = buffer[i];
        
        // Read the delayed sample and overwrite it with the new sample (circular buffer)
        buffer[i] = left_delay_history[delay_idx];
        left_delay_history[delay_idx] = new_left;
        
        delay_idx++;
        if (delay_idx >= GROUP_DELAY) {
            delay_idx = 0;
        }

        // --- Right Channel (FIR Filter) ---
        right_fir_history[fir_idx] = (float)buffer[i + 1];
        
        float out_r = 0.0f;
        int j = 0;
        
        // Optimize inner loop by splitting it into two contiguous history segments
        // Segment 1: from current index down to 0
        for (int k = fir_idx; k >= 0; k--, j++) {
            out_r += right_fir_history[k] * right_channel_fir[j];
        }
        
        // Segment 2: from end of buffer down to current index + 1
        for (int k = FIR_TAPS - 1; k > fir_idx; k--, j++) {
            out_r += right_fir_history[k] * right_channel_fir[j];
        }
        
        fir_idx++;
        if (fir_idx >= FIR_TAPS) {
            fir_idx = 0;
        }
        
        if (out_r > 32767.0f) out_r = 32767.0f;
        if (out_r < -32768.0f) out_r = -32768.0f;
        
        buffer[i + 1] = (int16_t)out_r;
    }
#else
    (void)buffer;
    (void)num_samples;
#endif
}

void audio_task(void) {
    if (g_I2Srx.A_buffer_ready) {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 2;

        if (tu_fifo_remaining(tud_audio_get_ep_in_ff()) >= bytes_to_write) {
            int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER];
            int32_t *in_buffer = (int32_t *)g_I2Srx.Get_A_Buffer();
            
            // Always process the base buffer
            for (int i = 0; i < AUDIO_SAMPLES_PER_BUFFER; i++) {
                out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
            }
            
            // Apply phase correction on EXACTLY the base buffer (384 samples)
            // This ensures the FIR history is perfectly contiguous and never rings!
            apply_phase_correction(out_buffer, AUDIO_SAMPLES_PER_BUFFER);

            tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
            g_I2Srx.A_buffer_ready = false;
        }
    }

    if (g_I2Srx.B_buffer_ready) {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 2;

        if (tu_fifo_remaining(tud_audio_get_ep_in_ff()) >= bytes_to_write) {
            int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER];
            int32_t *in_buffer = (int32_t *)g_I2Srx.Get_B_Buffer();
            
            // Always process the base buffer
            for (int i = 0; i < AUDIO_SAMPLES_PER_BUFFER; i++) {
                out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
            }
            
            // Apply phase correction on EXACTLY the base buffer (384 samples)
            // This ensures the FIR history is perfectly contiguous and never rings!
            apply_phase_correction(out_buffer, AUDIO_SAMPLES_PER_BUFFER);

            tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
            g_I2Srx.B_buffer_ready = false;
        }
    }
}