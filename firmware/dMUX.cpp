#include "dMUX.hpp"

// Native Pico libraries
#include <pico/stdlib.h>

#define VHF_FILTER_SELECT 8
#define BAND_SELECT 9


dMUX::dMUX()
{
    gpio_init(VHF_FILTER_SELECT);
    gpio_init(BAND_SELECT);
    gpio_set_dir(VHF_FILTER_SELECT, GPIO_OUT);
    gpio_set_dir(BAND_SELECT, GPIO_OUT);
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 1);
}

int dMUX::Get_Receiver_Configuration_State()
{
    return state;
} // Get_Receiver_Configuration_State

bool dMUX::Set_Receiver_Configuration_State(int configuration_number)
{
    if (!configuration_number) 
    {
        state = dMUX::receiver_configuration::HF;
        return true;
    }
    else if (configuration_number == 1)
    {
        state = dMUX::receiver_configuration::VHF_CHARLES;
        return true;
    }
    else if (configuration_number == 2)
    {
        state = dMUX::receiver_configuration::VHF_EXTERNAL;
        return true;
    }
    else return false;
} // Set_Receiver_Configuration_State

void dMUX::Configure_For_VHF_Charles()
{
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 0);
} // Configure_For_VHF_Charles

void dMUX::Configure_For_VHF_External()
{
    gpio_put(VHF_FILTER_SELECT, 1);
    gpio_put(BAND_SELECT, 0);
} // Configure_For_VHF_External

void dMUX::Configure_For_HF()
{
    gpio_put(VHF_FILTER_SELECT, 0);
    gpio_put(BAND_SELECT, 1);
} // Configure_For_HF