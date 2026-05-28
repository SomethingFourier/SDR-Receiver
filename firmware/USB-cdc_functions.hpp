// CDC specific functions to be used for TinyUSB
#include <pico/types.h>

void cdc_task(void);

static void respond_serial_port(uint8_t cdc_buffer[], uint32_t count);