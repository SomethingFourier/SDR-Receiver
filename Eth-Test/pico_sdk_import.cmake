if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT PICO_SDK_PATH)
    if(EXISTS "/home/pkcubed/pico-sdk/pico_sdk_init.cmake")
        set(PICO_SDK_PATH /home/pkcubed/pico-sdk)
    endif()
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to a local Raspberry Pi Pico SDK checkout")
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "PICO_SDK_PATH does not point at a valid Pico SDK")
endif()

include("${PICO_SDK_PATH}/pico_sdk_init.cmake")