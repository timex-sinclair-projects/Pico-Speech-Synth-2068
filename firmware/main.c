/*
 * main.c -- ZX Voice replica firmware for RP2040.
 *
 * core 0: USB console, Z80 bus capture (PIO + IRQ), text/control channel
 * core 1: SP0256 emulation, audio output, /LRQ and SBY pins
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "config.h"
#include "bus.h"
#include "synth.h"
#include "text.h"
#include "console.h"

int main(void)
{
	stdio_init_all();

	text_init();
	synth_start();          /* core 1 up first: it owns /LRQ and SBY   */
	bus_init();             /* then let the bus in                     */
	console_init();

	printf("\nZX Voice replica " FW_VERSION " (SP0256-AL2 emulation)\n> ");

	for (;;) {
		console_poll();
		text_poll();
		__wfe();
	}
}
