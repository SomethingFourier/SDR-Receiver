#include "USB-cdc_functions.hpp"

// C Libraries
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Native Pico libraries
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/types.h>
#include <pico/multicore.h>

// TinyUSB
extern "C" {
#include "tusb_config.h"
#include "bsp/board_api.h"
#include "tusb.h"
}

// Custom Libraries
#include "dSi5351.hpp"
#include "dI2C.hpp"

// respond depending on command
static void respond_serial_port(uint8_t cdc_buffer[], uint32_t count) {
    // Null-terminate the buffer for string functions
    char string_buffer[65];
    uint32_t length = count < 64 ? count : 64;
    memcpy(string_buffer, cdc_buffer, length);
    string_buffer[length] = '\0';

    if (length == 1 && string_buffer[0] == '\x04') {
        const char *response = "SDR ready\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    else if (length == 1 && string_buffer[0] == '\x03') {
        // just eat the ctrl-c, do not respond
    }
    else if (strncmp(string_buffer, "VER", 3) == 0) {
        // the VER command 
        const char* response = "VER,0.1\nOK\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    else if (strncmp(string_buffer, "XTAL", 4) == 0) {
        // the XTAL command 
        const char* response = "XTAL,24576000\nOK\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    else if (strncmp(string_buffer, "MODE", 4) == 0) {
        // the MODE command 
        const char* response = "MODE,DIRECT\nOK\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    else if (strncmp(string_buffer, "RATE,", 5) == 0) {
        // the RATE command
        const char* response = "OK\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    else if (strncmp(string_buffer, "FREQ,", 5) == 0) {
        if (length < 10) { // return current frequency; 12 was chosen for a reason, trust
            // the FREQ command
            char response[64];
            int chars_written = snprintf(response, sizeof(response), "%d\nOK,%c,0\n", g_Si5351.Get_Actual_Frequency(), g_Si5351.Get_PLLA_Mode());

            if (chars_written > 0) tud_cdc_n_write(0, response, strlen(response));
        }
        else if (length < 25) { // program frequency
            // program the si5351a
            int requested_frequency;
            int parameter_count = sscanf(string_buffer, "FREQ,%d", &requested_frequency);

            if (parameter_count == 0) {
                const char* response = "INVALID ARGUMENT\r\n";
                tud_cdc_n_write(0, response, strlen(response));
            }
            else if (parameter_count == 1) {
                multicore_fifo_push_blocking((uint32_t)requested_frequency);

                int actual_frequency = g_Si5351.Set_Golden_Frequency_Quadrature(requested_frequency);

                char pll_mode = g_Si5351.Get_PLLA_Mode();
                int frequency_offset = actual_frequency - requested_frequency;
                if (frequency_offset < 0) {
                    frequency_offset = -1*frequency_offset;
                }

                char response[64];
                int chars_written = snprintf(response, sizeof(response), "%d\nOK,%c,%d\n", requested_frequency, pll_mode, frequency_offset);
                if (chars_written > 0) {
                    tud_cdc_n_write(0, response, strlen(response));
                }
            }
        }
        else {
            const char* response = "ARGUMENT TOO LONG\r\n";
            tud_cdc_n_write(0, response, strlen(response));
        }
    }
    else {
        const char* response = "ERR\r\n";
        tud_cdc_n_write(0, response, strlen(response));
    }
    tud_cdc_n_write_flush(0);
} // respond_serial_port

void cdc_task(void) {
    if (tud_cdc_n_available(0)) {
        uint8_t message_buffer[64];

        uint32_t count = tud_cdc_n_read(0, message_buffer, sizeof(message_buffer));

        // respond to command
        respond_serial_port(message_buffer, count);
    }
}