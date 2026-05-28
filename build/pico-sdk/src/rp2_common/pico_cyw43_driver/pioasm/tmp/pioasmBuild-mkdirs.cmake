# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/neidna/opt/pico-sdk/tools/pioasm")
  file(MAKE_DIRECTORY "/home/neidna/opt/pico-sdk/tools/pioasm")
endif()
file(MAKE_DIRECTORY
  "/home/neidna/SDR-Receiver/build/pioasm"
  "/home/neidna/SDR-Receiver/build/pioasm-install"
  "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/neidna/SDR-Receiver/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
