#include "dMUX.hpp"

// Native Pico libraries
#include <pico/stdlib.h>

#include "dGPIO.hpp"

#define VHF_FILTER_SELECT 8
#define BAND_SELECT 9


dMUX::dMUX() {
    gpio_init(VHF_FILTER_SELECT);
    gpio_init(BAND_SELECT);
    gpio_set_dir(VHF_FILTER_SELECT, GPIO_OUT);
    gpio_set_dir(BAND_SELECT, GPIO_OUT);
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 1);
}

int dMUX::Get_Receiver_Configuration_State() {
    return state;
} // Get_Receiver_Configuration_State

void dMUX::Set_Receiver_Configuration_State(enum receiver_configuration configuration_number) {
    state = configuration_number;
} // Set_Receiver_Configuration_State

void dMUX::Configure_For_VHF_Charles() {
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 0);
    gpio_put(LED_GREEN, 1);
    gpio_put(LED_YELLOW, 0);
    gpio_put(LED_WHITE, 0);
} // Configure_For_VHF_Charles

void dMUX::Configure_For_VHF_External() {
    gpio_put(VHF_FILTER_SELECT, 1);
    gpio_put(BAND_SELECT, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_YELLOW, 1);
    gpio_put(LED_WHITE, 0);
} // Configure_For_VHF_External

void dMUX::Configure_For_HF() {
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 1);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_YELLOW, 0);
    gpio_put(LED_WHITE, 1);
} // Configure_For_HF