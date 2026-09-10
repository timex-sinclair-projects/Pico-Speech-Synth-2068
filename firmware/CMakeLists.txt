cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(zxvoice C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../core)
set(TTS_DIR  ${CMAKE_CURRENT_LIST_DIR}/../tts)
set(ROM_BIN  ${CMAKE_CURRENT_LIST_DIR}/../rom/sp0256-al2.bin)
set(ROM_H    ${CMAKE_CURRENT_BINARY_DIR}/rom_al2.h)

add_custom_command(
	OUTPUT ${ROM_H}
	COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/../tools/bin2c.py ${ROM_BIN} ${ROM_H} sp0256_al2_rom
	DEPENDS ${ROM_BIN} ${CMAKE_CURRENT_LIST_DIR}/../tools/bin2c.py
	COMMENT "Embedding SP0256-AL2 ROM")

add_executable(zxvoice
	main.c
	bus.c
	audio.c
	synth.c
	text.c
	console.c
	${CORE_DIR}/sp0256.c
	${TTS_DIR}/tts.c
	${TTS_DIR}/rules.c
	${TTS_DIR}/rules_nrl.c
	${TTS_DIR}/rules_cts.c
	${TTS_DIR}/phon2allo.c
	${TTS_DIR}/nrl_numbers.c
	${TTS_DIR}/lexicon.c
	${ROM_H})

pico_generate_pio_header(zxvoice ${CMAKE_CURRENT_LIST_DIR}/bus.pio)

target_include_directories(zxvoice PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CORE_DIR}
	${TTS_DIR})

target_compile_options(zxvoice PRIVATE -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(zxvoice
	pico_stdlib
	pico_multicore
	hardware_pio
	hardware_dma
	hardware_pwm
	hardware_irq
	hardware_clocks)

pico_enable_stdio_usb(zxvoice 1)
pico_enable_stdio_uart(zxvoice 0)

pico_add_extra_outputs(zxvoice)
