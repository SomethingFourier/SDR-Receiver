#ifndef dSi5351_H
#define dSi5351_H

#include <hardware/i2c.h>
#include <pico/types.h>

class dSi5351 {
	
	public:
		dSi5351();

		// Public API
		bool Init();
		bool Start_Outputs();
		bool Stop_Outputs();
		void Request_Frequency_Programming(uint32_t target_frequency);
		int Set_Golden_Quadrature_Frequency(uint32_t target_frequency);
		int Set_Frequency_Integer_Clk(uint32_t target_frequency, uint32_t clk);
		int Get_Actual_Quadrature_Frequency();
		int Get_Desired_Quadrature_Frequency();
		int Get_Frequency_Offset();
		bool Programming_Request_Exists();
		bool CDC_Programming_Response_Needed();

	private:
		bool programming_request_exists;
		bool cdc_programming_response_needed;

		int actual_quadrature_frequency;
		int requested_quadrature_frequency;
		int frequency_offset;

		static const uint8_t Si5351_ADDR = 0x60;
		static const uint32_t XTAL_HZ = 24576000u;

		bool Reg_Write(uint8_t reg, uint8_t value);
		bool Reg_Read(uint8_t reg, uint8_t *value);
		bool Write_Block(uint8_t start_reg, const uint8_t *data, uint8_t len);
		void Pack_Integer_Params(uint32_t a, uint8_t out[8]);

		friend void sdr_state_machine();
		friend void cdc_task(void);
		
		// dSi5351(const dSi5351&);
		// void operator=(const dSi5351&);
};

extern dSi5351 g_Si5351;

#endif  // dSi5351_H