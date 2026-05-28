#ifndef dSi5351_H
#define dSi5351_H

#include <hardware/i2c.h>
#include <pico/types.h>

class dSi5351
{
public:
	dSi5351();

	// Public API
	bool Init();
	bool Start_Outputs();
	bool Stop_Outputs();
	uint32_t Program_With_Exact_Parameters(uint32_t target_frequency, uint32_t phase_offset_divider, uint32_t pll_multiplier_integer, uint32_t pll_multiplier_numerator, uint32_t pll_multiplier_denominator, uint32_t pll_parameter_1, uint32_t pll_parameter_2, uint32_t pll_parameter_3);
	uint32_t Set_Frequency_Integer_Quadrature(uint32_t target_hz);
	uint32_t Get_Actual_Frequency();
	char Get_PLLA_Mode();

private:
	uint32_t actual_frequency;

	static const uint8_t Si5351_ADDR = 0x60;
	static const uint32_t XTAL_HZ = 24576000u;

	bool Reg_Write(uint8_t reg, uint8_t value);
	bool Reg_Read(uint8_t reg, uint8_t *value);
	bool Write_Block(uint8_t start_reg, const uint8_t *data, uint8_t len);
	void Pack_Integer_Params(uint32_t a, uint8_t out[8]);
	
    dSi5351(const dSi5351&);
    void operator=(const dSi5351&);
};

extern dSi5351 g_Si5351;

#endif  // dSi5351_H