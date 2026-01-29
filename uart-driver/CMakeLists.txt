cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(uart-driver_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(uart-driver
    uart-driver.c
)

target_link_libraries(
    uart-driver
    pico_stdlib    
    hardware_uart    
    hardware_gpio    
    hardware_timer    
)

# Enable or disable stdio (not required if you’re writing raw UART)
pico_enable_stdio_usb(uart-driver 0)
pico_enable_stdio_uart(uart-driver 0)

# Generate .elf, .uf2, etc.
pico_add_extra_outputs(uart-driver)

#add debug info
target_compile_definitions(uart-driver PRIVATE PICO_DEBUG=1)

#export(PICO_TOOLCHAIN_PATH="/Applications/arm-gnu-toolchain-14.3.Rel1-darwin-arm64-arm-none-eabi")