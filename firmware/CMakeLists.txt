# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(beebKeyNext C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(beebKeyNext beebKeyNext.c ps2keys.c ps2comm.c)

pico_set_program_name(beebKeyNext "beebKeyNext")
pico_set_program_version(beebKeyNext "0.1")

#pico_enable_stdio_uart(beebKeyNext 1)
pico_enable_stdio_usb(beebKeyNext 1)

# Add the standard library to the build
target_link_libraries(beebKeyNext pico_stdlib)

# Add any user requested libraries
target_link_libraries(beebKeyNext
        hardware_pio
        hardware_timer
        pico_multicore 
        )

pico_add_extra_outputs(beebKeyNext)

