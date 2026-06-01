#ifndef dI2C_H
#define dI2C_H

#include <pico/types.h>
#include <hardware/i2c.h>

class dI2C {
public:
	dI2C();

	// Public API
	void Init(i2c_inst_t *i2c_instance = i2c0);
	void Scan();

	i2c_inst_t *master_i2c_instance; // Pointer to the active i2c instance

private:
	bool Reserved_Address(uint8_t address);
	
    dI2C(const dI2C&);
    void operator=(const dI2C&);
};

extern dI2C g_I2C;

#endif  // dI2C_H