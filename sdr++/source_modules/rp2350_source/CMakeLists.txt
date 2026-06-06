cmake_minimum_required(VERSION 3.13)
project(rp2350_source)

file(GLOB SRC "src/*.cpp")

include(${SDRPP_MODULE_CMAKE})

if (MSVC)
    target_link_directories(rp2350_source PRIVATE "C:/Program Files (x86)/RtAudio/lib")
    target_include_directories(rp2350_source PRIVATE "C:/Program Files (x86)/RtAudio/include/rtaudio")
    target_link_libraries(rp2350_source PRIVATE rtaudio)
else (MSVC)
    find_package(PkgConfig)
    pkg_check_modules(RTAUDIO REQUIRED rtaudio)
    target_include_directories(rp2350_source PRIVATE ${RTAUDIO_INCLUDE_DIRS})
    target_link_directories(rp2350_source PRIVATE ${RTAUDIO_LIBRARY_DIRS})
    target_link_libraries(rp2350_source PRIVATE ${RTAUDIO_LIBRARIES})
endif ()
