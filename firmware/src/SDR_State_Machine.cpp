#include "SDR_State_Machine.hpp"

#include <pico/multicore.h>

#include "dMUX.hpp"
#include "dSi5351.hpp"

void sdr_state_machine() {

	while (true) {
		uint32_t freq = multicore_fifo_pop_blocking();
		// HF/VHF cutoff 30 MHz
		int actual_frequency = 0;

		if (freq <= 30e6) {
			// HF Mode
			g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::HF);
			actual_frequency = g_Si5351.Set_Golden_Frequency_Quadrature(freq);
		}
		else {
			// VHF Mode
			g_MUX.Set_Receiver_Configuration_State(dMUX::receiver_configuration::VHF_CHARLES);
			g_Si5351.Set_Golden_Frequency_Quadrature(25e6);
			actual_frequency = g_Si5351.Set_Frequency_Integer_Clk(freq - 25e6, 2) + 25e6;
			g_Si5351.Set_Actual_Frequency(actual_frequency);
		}

		multicore_fifo_push_blocking((uint32_t)actual_frequency);
		multicore_fifo_push_blocking((uint32_t)g_Si5351.Get_PLLA_Mode());
	}
}