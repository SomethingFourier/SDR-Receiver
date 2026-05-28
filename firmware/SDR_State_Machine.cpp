#include "SDR_State_Machine.hpp"
#include "dMUX.hpp"
#include "dSi5351.hpp"
#include "pico/multicore.h"

void sdr_state_machine()
{
	dMUX dmux;
	while (true) 
	{
		uint32_t freq = multicore_fifo_pop_blocking();
		//HF/VHF cutoff 30 MHz

		if (freq <= 30e6) 
		{
			//HF Mode
			dmux.Set_Receiver_Configuration_State(dMUX::receiver_configuration::HF);
			g_Si5351.Set_Frequency_Integer_Quadrature(freq);
		} else 
		{
			//VHF Mode
			dmux.Set_Receiver_Configuration_State(dMUX::receiver_configuration::VHF_CHARLES);
			g_Si5351.Set_Frequency_Integer_Quadrature(25e6);
			g_Si5351.Set_Frequency_Integer_Clk(freq - 25e6, 2);
		}


	}
}