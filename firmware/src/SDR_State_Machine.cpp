#include "SDR_State_Machine.hpp"

#include <pico/multicore.h>
#include <pico/stdlib.h>

#include "dMUX.hpp"
#include "dSi5351.hpp"
#include "dGPIO.hpp"

void sdr_state_machine() {
	while (true) {
		if (g_Si5351.Programming_Request_Exists()) {
			// program the radio
			// HF/VHF cutoff 30 MHz
			int actual_frequency = 0;

			if (g_Si5351.requested_frequency <= 30e6) {
				actual_frequency = g_Si5351.Set_Golden_Frequency_Quadrature(g_Si5351.requested_frequency);
				// HF Mode
				g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::HF);
				// we don't take down the programming_request_exists flag because core 0 still needs to respond to the user on CDC about it.
				g_Si5351.programming_request_exists = false;
				g_Si5351.cdc_programming_response_needed = true;
				gpio_xor_mask(1u << LED_WHITE);
			}
			else {
				// VHF Mode
				g_Si5351.Set_Golden_Frequency_Quadrature(25e6);
				g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::VHF_CHARLES);
				actual_frequency = g_Si5351.Set_Frequency_Integer_Clk(g_Si5351.requested_frequency - 25e6, 2) + 25e6;
				g_Si5351.programming_request_exists = false;
				g_Si5351.cdc_programming_response_needed = true;
				gpio_xor_mask(1u << LED_WHITE);
			}
		}
	}
}