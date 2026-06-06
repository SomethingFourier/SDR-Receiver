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

#define AUDIO_SAMPLES_PER_BUFFER 384 // 192 frames * 2 channels (stereo)

#define FIR_TAPS 31
#define GROUP_DELAY ((FIR_TAPS - 1) / 2)

const float right_channel_fir[FIR_TAPS] = {
    -0.0000000000f,
    -0.0000850254f,
    0.0003767077f,
    -0.0009618252f,
    0.0019694923f,
    -0.0035658081f,
    0.0059431792f,
    -0.0093064184f,
    0.0138590017f,
    -0.0197942491f,
    0.0272980892f,
    -0.0365736113f,
    0.0479059997f,
    -0.0618090841f,
    0.0793632436f,
    -0.1030959417f,
    0.1398300714f,
    -0.2139528740f,
    0.5077906466f,
    0.7437000505f,
    -0.1712360100f,
    0.0802195400f,
    -0.0436304902f,
    0.0247286202f,
    -0.0139596256f,
    0.0076210718f,
    -0.0039056174f,
    0.0017944558f,
    -0.0006680303f,
    0.0001444412f,
    0.0000000000f,
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
        
        // Grab the IN endpoint FIFO and check how many bytes are backed up.
        // If there's more than 1ms of audio (768 bytes) waiting, drop 1 frame (4 bytes) to prevent overrun.
        if (tu_fifo_count(tud_audio_get_ep_in_ff()) > 1152) {
            bytes_to_write -= 4; 
        }
        // If the FIFO is draining below 0.5ms (384 bytes), add 1 frame (4 bytes) to prevent underrun.
        else if (tu_fifo_count(tud_audio_get_ep_in_ff()) < 384) {
            bytes_to_write += 4;
        }

        if (tu_fifo_remaining(tud_audio_get_ep_in_ff()) >= bytes_to_write) {
            int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER + 2]; // +2 for duplicate frame
            int32_t *in_buffer = (int32_t *)g_I2Srx.Get_A_Buffer();
            
            // Always process the base buffer
            for (int i = 0; i < AUDIO_SAMPLES_PER_BUFFER; i++) {
                out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
            }
            
            // Apply phase correction on EXACTLY the base buffer (384 samples)
            // This ensures the FIR history is perfectly contiguous and never rings!
            apply_phase_correction(out_buffer, AUDIO_SAMPLES_PER_BUFFER);
            
            // If we are dropping a frame, bytes_to_write/2 will be AUDIO_SAMPLES_PER_BUFFER - 2.
            // If we are adding a frame, we duplicate the last filtered frame.
            if (bytes_to_write > AUDIO_SAMPLES_PER_BUFFER * 2) {
                out_buffer[AUDIO_SAMPLES_PER_BUFFER] = out_buffer[AUDIO_SAMPLES_PER_BUFFER - 2];
                out_buffer[AUDIO_SAMPLES_PER_BUFFER + 1] = out_buffer[AUDIO_SAMPLES_PER_BUFFER - 1];
            }

            tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
            g_I2Srx.A_buffer_ready = false;
        }
    }

    if (g_I2Srx.B_buffer_ready) {
        int bytes_to_write = AUDIO_SAMPLES_PER_BUFFER * 2;
        
        // Grab the IN endpoint FIFO and check how many bytes are backed up.
        // If there's more than 1ms of audio (768 bytes) waiting, drop 1 frame (4 bytes) to prevent overrun.
        if (tu_fifo_count(tud_audio_get_ep_in_ff()) > 1152) {
            bytes_to_write -= 4; 
        }
        // If the FIFO is draining below 0.5ms (384 bytes), add 1 frame (4 bytes) to prevent underrun.
        else if (tu_fifo_count(tud_audio_get_ep_in_ff()) < 384) {
            bytes_to_write += 4;
        }

        if (tu_fifo_remaining(tud_audio_get_ep_in_ff()) >= bytes_to_write) {
            int16_t out_buffer[AUDIO_SAMPLES_PER_BUFFER + 2]; // +2 for duplicate frame
            int32_t *in_buffer = (int32_t *)g_I2Srx.Get_B_Buffer();
            
            // Always process the base buffer
            for (int i = 0; i < AUDIO_SAMPLES_PER_BUFFER; i++) {
                out_buffer[i] = (int16_t)(in_buffer[i] >> 16);
            }
            
            // Apply phase correction on EXACTLY the base buffer (384 samples)
            // This ensures the FIR history is perfectly contiguous and never rings!
            apply_phase_correction(out_buffer, AUDIO_SAMPLES_PER_BUFFER);
            
            // If we are dropping a frame, bytes_to_write/2 will be AUDIO_SAMPLES_PER_BUFFER - 2.
            // If we are adding a frame, we duplicate the last filtered frame.
            if (bytes_to_write > AUDIO_SAMPLES_PER_BUFFER * 2) {
                out_buffer[AUDIO_SAMPLES_PER_BUFFER] = out_buffer[AUDIO_SAMPLES_PER_BUFFER - 2];
                out_buffer[AUDIO_SAMPLES_PER_BUFFER + 1] = out_buffer[AUDIO_SAMPLES_PER_BUFFER - 1];
            }

            tud_audio_write((uint8_t *)out_buffer, bytes_to_write);
            g_I2Srx.B_buffer_ready = false;
        }
    }
}