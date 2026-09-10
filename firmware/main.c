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
#include "board.h"

int main(void)
{
	stdio_init_all();

	board_init();           /* jumpers and bus clock, before anything talks */
	text_init();
	synth_start();          /* core 1 up first: it owns READY and SBY  */
	bus_init();             /* then let the bus in                     */
	console_init();

	/* On a TS1000 the original ran the chip from the CPU clock. */
	if (board_suggested_sp0256_clock() != DEFAULT_CLOCK_HZ)
		synth_set_clock(board_suggested_sp0256_clock());

	printf("\nZX Voice replica " FW_VERSION " (SP0256-AL2 emulation), board " BOARD_NAME "\n");
#if HAS_BOARD_ID
	if (board_id() != BOARD_ID_EXPECTED)
		printf("warning: board-ID jumpers read %d, this firmware expects %d\n", board_id(), BOARD_ID_EXPECTED);
#endif
	printf("> ");

	for (;;) {
		console_poll();
		text_poll();
		__wfe();
	}
}
