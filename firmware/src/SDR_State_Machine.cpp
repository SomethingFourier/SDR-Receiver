#include "SDR_State_Machine.hpp"

#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <stdio.h>

#include "dMUX.hpp"
#include "dSi5351.hpp"
#include "dGPIO.hpp"
#include "dSSD1306.hpp"

void sdr_state_machine() {
	while (true) {
		if (g_Si5351.Programming_Request_Exists()) {
			// program the radio
			// HF/VHF cutoff 30 MHz

			if (g_Si5351.requested_quadrature_frequency <= 30e6) {
				g_Si5351.Set_Golden_Quadrature_Frequency(g_Si5351.requested_quadrature_frequency);
				// HF Mode
				g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::HF);
				// we don't take down the programming_request_exists flag because core 0 still needs to respond to the user on CDC about it.
				g_Si5351.programming_request_exists = false;
				g_Si5351.cdc_programming_response_needed = true;
			}
			else {
				// VHF Mode
				g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::VHF_CHARLES);
				g_Si5351.Set_VHF_Quadrature_Frequency(g_Si5351.requested_quadrature_frequency, 25e6, 5e6);
				g_Si5351.programming_request_exists = false;
				g_Si5351.cdc_programming_response_needed = true;
			}

			char requested_freq_str[20];
			char actual_freq_str[20];

			char tayloe_freq_str[20];
			char diode_ring_freq_str[20];

			snprintf(requested_freq_str, sizeof(requested_freq_str), "Requested: %lu", (unsigned long)g_Si5351.requested_quadrature_frequency);
			snprintf(actual_freq_str, sizeof(actual_freq_str), "Actual   : %lu", (unsigned long)(g_Si5351.Get_Actual_Quadrature_Frequency() + g_Si5351.actual_clk2_frequency));

			snprintf(tayloe_freq_str, sizeof(tayloe_freq_str), "Tayloe: %lu", (unsigned long)(g_Si5351.Get_Actual_Quadrature_Frequency()));
			snprintf(diode_ring_freq_str, sizeof(diode_ring_freq_str), "D-Ring: %lu", (unsigned long)(g_Si5351.actual_clk2_frequency));

			g_SSD1306.Draw_Text(3, requested_freq_str);
			g_SSD1306.Draw_Text(4, actual_freq_str);

			g_SSD1306.Draw_Text(5, tayloe_freq_str);
			g_SSD1306.Draw_Text(6, diode_ring_freq_str);

			g_SSD1306.Update();
		}
	}
}