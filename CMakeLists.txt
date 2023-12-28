cmake_minimum_required(VERSION 3.22)

include(/home/jon/src/pico/pico-sdk/pico_sdk_init.cmake)

project(pica)

pico_sdk_init()

set(CMAKE_CXX_STANDARD 17)

#add_definitions(-DPICO_STDIO_ENABLE_CRLF_SUPPORT=0)

add_executable(pica main.c io.c io.h sensor.h sensor.c
        servo.c
        servo.h
        net.c
        net.h
        lwipopts.h
        aqi.c
        aqi.h
        comms.c
        comms.h
        data.c
        data.h
        util.h
        str.h)

target_include_directories(pica PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(pica pico_cyw43_arch_lwip_poll pico_stdlib hardware_pwm hardware_flash pico_lwip_mdns pico_lwip_http)

# create map/bin/hex file etc.
pico_add_extra_outputs(pica)
add_definitions(-DPARAM_ASSERTIONS_ENABLE_ALL=1)

pico_enable_stdio_usb(pica 1)
pico_enable_stdio_uart(pica 0)