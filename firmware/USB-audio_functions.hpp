// C Libraries
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Native Pico libraries
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/types.h>

// TinyUSB
extern "C" {
#include "tusb_config.h"
#include "bsp/board_api.h"
#include "tusb.h"
}

bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *p_request);

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request);

bool tud_audio_set_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request, uint8_t *pBuff);

bool tud_audio_get_req_ep_cb(uint8_t rhport, const tusb_control_request_t *p_request);

bool tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *p_request);

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting);


void audio_task(void);