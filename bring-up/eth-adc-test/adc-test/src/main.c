#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "audio_i2s.h"

#define LED_PIN 4

int main() {
    stdio_init_all();
    board_init();
    tusb_init();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    audio_i2s_init();

    uint32_t read_idx = 0;

    while (1) {
        tud_task();

        uint32_t write_idx = audio_i2s_get_write_index();
        uint32_t available = (write_idx - read_idx) & (AUDIO_RING_FRAMES - 1);

        if (available > 64) {
            // Buffer is overflowing, USB is disconnected or too slow.
            // Drop old samples to catch up to the latest data.
            read_idx = (write_idx - 32) & (AUDIO_RING_FRAMES - 1);
            available = 32;
        }

        if (available > 0) {
            gpio_put(LED_PIN, 1);
            
            // Check space in USB FIFO (in bytes)
            uint32_t max_bytes = tu_fifo_remaining(tud_audio_get_ep_in_ff());
            uint32_t max_frames = max_bytes / 8; // 32-bit stereo in 4-byte subframes = 8 bytes per frame

            uint32_t frames_to_write = available;
            if (frames_to_write > max_frames) {
                frames_to_write = max_frames;
            }

            // We may need to write in two chunks if we wrap around the ring buffer end
            if (frames_to_write > 0) {
                uint32_t chunk1 = frames_to_write;
                if (read_idx + chunk1 > AUDIO_RING_FRAMES) {
                    chunk1 = AUDIO_RING_FRAMES - read_idx;
                }

                uint16_t written1 = tud_audio_write((uint8_t*)&audio_ring_buffer[read_idx * 2], chunk1 * 8);
                read_idx = (read_idx + (written1 / 8)) & (AUDIO_RING_FRAMES - 1);

                if (written1 == chunk1 * 8 && frames_to_write > chunk1) {
                    uint32_t chunk2 = frames_to_write - chunk1;
                    uint16_t written2 = tud_audio_write((uint8_t*)&audio_ring_buffer[read_idx * 2], chunk2 * 8);
                    read_idx = (read_idx + (written2 / 8)) & (AUDIO_RING_FRAMES - 1);
                }
            }
            gpio_put(LED_PIN, 0);
        }
    }
    return 0;
}

// ---- TinyUSB Audio Callbacks ---- //
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request) { return true; }
bool tud_audio_get_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request) { return true; }

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff) {
    (void) rhport; (void) p_request; (void) pBuff;
    return true; // Accept all set requests
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const * p_request) {
    if (TU_U16_LOW(p_request->wIndex) != 0x81) return false;
    if (p_request->bRequest != AUDIO10_CS_REQ_GET_CUR || TU_U16_HIGH(p_request->wValue) != AUDIO10_EP_CTRL_SAMPLING_FREQ) return false;
    uint8_t sample_rate[3] = { (uint8_t)(48000 & 0xFF), (uint8_t)((48000 >> 8) & 0xFF), (uint8_t)((48000 >> 16) & 0xFF) };
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, sample_rate, sizeof(sample_rate));
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff) { return true; }

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff) {
    (void) rhport; (void) p_request; (void) pBuff;
    return true;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request) {
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);
    uint8_t control_selector = TU_U16_HIGH(p_request->wValue);
    if (entity_id == 2 && control_selector == AUDIO10_FU_CTRL_MUTE && p_request->bRequest == AUDIO10_CS_REQ_GET_CUR) {
        uint8_t control_value = 0;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &control_value, sizeof(control_value));
    }
    return false;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting) { return true; }

// ---- Dummy CDC Callbacks ---- //
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) { (void) itf; (void) dtr; (void) rts; }
void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;
    tud_cdc_n_read_flush(itf);
}
