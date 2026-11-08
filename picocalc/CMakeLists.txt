# A raylib game on the ClockworkPi PicoCalc (Pico 1 / RP2040), with the Pico SDK.
#
# Once (see README.md for the SDK and compiler):
#     export PICO_SDK_PATH=~/pico-sdk
#
# Build a game (from the project folder):
#     cmake -S picocalc -B build/wasd -DGAME_DIR=examples/wasd
#     cmake --build build/wasd -j
#
# That makes build/wasd/wasd.uf2, to flash to the Pico (see README.md).
# GAME_DIR is any folder with a game's .c files and its shim_assets.c.

cmake_minimum_required(VERSION 3.13)

# The PicoCalc kit comes with a Pico 1. For a Pico 2: cmake ... -DPICO_BOARD=pico2
if(NOT PICO_BOARD)
	set(PICO_BOARD pico)
endif()
include(pico_sdk_import.cmake)

project(raylib_picocalc C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Which game: its folder, relative to the project
set(GAME_DIR "examples/wasd" CACHE PATH "The game's folder, relative to the project")
get_filename_component(GAME_DIR_FULL ${GAME_DIR} ABSOLUTE BASE_DIR ${ROOT})
get_filename_component(GAME_NAME ${GAME_DIR_FULL} NAME)
file(GLOB GAME_SOURCES ${GAME_DIR_FULL}/*.c)
if(NOT GAME_SOURCES)
	message(FATAL_ERROR "No .c files in ${GAME_DIR_FULL}")
endif()
if(NOT EXISTS ${GAME_DIR_FULL}/shim_assets.c)
	message(FATAL_ERROR "No shim_assets.c in ${GAME_DIR}: run  python3 tools/convert_assets.py ${GAME_DIR}")
endif()
message(STATUS "PicoCalc game: ${GAME_NAME} (${GAME_DIR_FULL})")

file(GLOB SHIM_SOURCES ${ROOT}/shim/src/*.c)

add_executable(${GAME_NAME}
	${GAME_SOURCES} # Its shim_assets.c comes along too
	${SHIM_SOURCES}
	${CMAKE_CURRENT_LIST_DIR}/platform_picocalc.c
)

# The shim's raylib.h comes first, so #include <raylib.h> finds it
target_include_directories(${GAME_NAME} PRIVATE
	${ROOT}/shim/include
	${ROOT}/shim/src
	${CMAKE_CURRENT_LIST_DIR}
	${GAME_DIR_FULL}
)

target_compile_definitions(${GAME_NAME} PRIVATE
	PICO_STACK_SIZE=0x1000             # 4 KB (all of SCRATCH_Y): 2 KB is tight for TraceLog's line
	PICO_USE_FASTEST_SUPPORTED_CLOCK=1 # 200 MHz, not 125: the fastest Raspberry Pi support (SDK 2.1 or later)
	PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=1000 # Wait up to a second at start-up for the log to connect, so its first lines aren't lost
)

target_compile_options(${GAME_NAME} PRIVATE -Wall -Wno-unused-parameter)

target_link_libraries(${GAME_NAME}
	pico_stdlib
	pico_multicore
	pico_rand
	hardware_spi
	hardware_i2c
	hardware_pwm
	hardware_dma
)

# TraceLog() goes out over the Pico's micro-USB (/dev/ttyACM0 on Linux), and
# its UART0 (GP0/GP1, 115200 baud) for anyone with a USB-serial lead
pico_enable_stdio_usb(${GAME_NAME} 1)
pico_enable_stdio_uart(${GAME_NAME} 1)

pico_add_extra_outputs(${GAME_NAME}) # .uf2 for drag and drop
