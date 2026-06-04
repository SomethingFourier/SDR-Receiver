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
#define AUDIO_SAMPLE_RATE_HZ 48000u

static uint8_t current_mute[3] = {0, 0, 0};

// UAC1 callbacks required by TinyUSB
bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request) {
    (void) rhport;
    (void) p_request;
    return true;
}

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

#define AUDIO_SAMPLES_PER_BUFFER 96 // 48 frames * 2 channels (stereo)

#define FIR_TAPS 31
#define GROUP_DELAY ((FIR_TAPS - 1) / 2)

const float right_channel_fir[FIR_TAPS] = {
    0.0000000000f,
    0.0000252163f,
    -0.0001129608f,
    0.0002920706f,
    -0.0006068017f,
    0.0011173534f,
    -0.0018998195f,
    0.0030468560f,
    -0.0046715548f,
    0.0069193986f,
    -0.0099988508f,
    0.0142569993f,
    -0.0203791073f,
    0.0300061320f,
    -0.0482966219f,
    0.1034327164f,
    0.9824491082f,
    -0.0797343920f,
    0.0380217824f,
    -0.0226100376f,
    0.0144260497f,
    -0.0093816375f,
    0.0060611098f,
    -0.0038253847f,
    0.0023252815f,
    -0.0013400664f,
    0.0007157679f,
    -0.0003397913f,
    0.0001298911f,
    -0.0000287070f,
    -0.0000000000f,
};

static float right_fir_history[FIR_TAPS] = {0};
static int16_t left_delay_history[GROUP_DELAY] = {0};

void apply_phase_correction(int16_t* buffer, int num_samples) {
    for (int i = 0; i < num_samples; i += 2) {
        // --- Left Channel (Integer Delay) ---
        int16_t new_left = buffer[i];
        buffer[i] = left_delay_history[GROUP_DELAY - 1];
        
        for (int j = GROUP_DELAY - 1; j > 0; j--) {
            left_delay_history[j] = left_delay_history[j - 1];
        }
        left_delay_history[0] = new_left;

        // --- Right Channel (FIR Filter) ---
        // Shift history
        for (int j = FIR_TAPS - 1; j > 0; j--) {
            right_fir_history[j] = right_fir_history[j - 1];
        }
        right_fir_history[0] = (float)buffer[i + 1];
        
        float out_r = 0.0f;
        for (int j = 0; j < FIR_TAPS; j++) {
            out_r += right_fir_history[j] * right_channel_fir[j];
        }
        
        if (out_r > 32767.0f) out_r = 32767.0f;
        if (out_r < -32768.0f) out_r = -32768.0f;
        
        buffer[i + 1] = (int16_t)out_r;
    }
}

void audio_task(void) {
    if (g_I2Srx.A_buffer_ready) {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 2;
        
        // Grab the IN endpoint FIFO and check how many bytes are backed up.
        // If there's more than 1ms of audio (192 bytes) waiting, drop 1 frame (4 bytes).
        if (tu_fifo_count(tud_audio_get_ep_in_ff()) > 192) {
            bytes_to_write -= 4; 
        }

        int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER];
        int32_t *in_buffer = (int32_t *)g_I2Srx.Get_A_Buffer();
        for (int i = 0; i < (bytes_to_write / 2); i++) {
            out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
        }

        apply_phase_correction(out_buffer, bytes_to_write / 2);

        uint16_t written = tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
        if (written > 0) {
            g_I2Srx.A_buffer_ready = false;
        }
    }

    if (g_I2Srx.B_buffer_ready) {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 2;
        
        if (tu_fifo_count(tud_audio_get_ep_in_ff()) > 192) {
            bytes_to_write -= 4; 
        }

        int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER];
        int32_t *in_buffer = (int32_t *)g_I2Srx.Get_B_Buffer();
        for (int i = 0; i < (bytes_to_write / 2); i++) {
            out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
        }

        apply_phase_correction(out_buffer, bytes_to_write / 2);

        uint16_t written = tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
        if (written > 0) {
            g_I2Srx.B_buffer_ready = false;
        }
    }
}