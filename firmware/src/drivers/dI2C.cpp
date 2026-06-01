#include "dI2C.hpp"

#include <stdio.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/types.h>

#define SDA_PIN 0
#define SCL_PIN 1

dI2C::dI2C() {} // constructor


void dI2C::Init(i2c_inst_t *i2c_instance) {
    
    master_i2c_instance = i2c_instance; // this is i2c0 by default
	i2c_init(i2c_instance, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
} // Init()

void dI2C::Scan() {
    // This was copied from the example in the pico-sdk docs 
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
 
    for (uint8_t address = 0; address < (1 << 7); ++address) {
        if (address % 16 == 0) {
            printf("%02x ", address);
        }

        // I would make this variable more readable, but I bascially copy-pasted it from the official pico-sdk I2C example and i have no idea what it is meant to be
        int ret;
        uint8_t rxdata;

        if (Reserved_Address(address)) {
            ret = PICO_ERROR_GENERIC;
        }
        else {
            // use timeout version to prevent the bus from hanging indefinitely
            ret = i2c_read_timeout_us(master_i2c_instance, address, &rxdata, 1, false, 100000); // 100ms timeout
        }
            
        printf(ret < 0 ? "." : "@");
        printf(address % 16 == 15 ? "\n" : "  ");
    }

    printf("Done.\n");
} // Scan()

bool dI2C::Reserved_Address(uint8_t address) {
    return (address & 0x78) == 0 || (address & 0x78) == 0x78;
} // Reserved_Address()
