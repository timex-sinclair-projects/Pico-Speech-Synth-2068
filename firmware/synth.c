/*
 * synth.c -- core 1: the SP0256 core, its input buffer, and the LRQ/SBY pins.
 *
 * Buffering follows the chip exactly.  The bus writes into a one-deep buffer
 * (bus_val/bus_full); a write while that buffer or the chip's own address
 * latch is full is dropped.  /LRQ is driven busy the moment a byte is
 * accepted (in the IRQ, on core 0) and released by core 1 once the sequencer
 * has taken the address, which is when the real chip releases it.  Console
 * SPEAK requests sit in a separate queue that is fed to the chip only when
 * the bus buffer is empty, so a host program on the Z80 always wins.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "config.h"
#include "sp0256.h"
#include "rom_al2.h"
#include "audio.h"
#include "synth.h"

static sp0256_t chip;

static volatile uint8_t  bus_val;
static volatile bool     bus_full;
static volatile bool     chip_busy;       /* mirror of sp0256_lrq_pin()   */
static volatile bool     reset_req;
static volatile bool     stop_req;

static volatile uint8_t  hq[HOST_QUEUE];
static volatile uint32_t hq_head, hq_tail;  /* head: producer, tail: consumer */

static volatile uint32_t clock_hz = DEFAULT_CLOCK_HZ;
static volatile uint32_t clock_req;         /* nonzero: apply on core 1     */
static volatile int      speed_pct = 100, pitch_pct = 100;
static volatile bool     ctl_req;

/* ---------------------------------------------------------------------- */
/*  Core 0 side                                                           */
/* ---------------------------------------------------------------------- */
bool __time_critical_func(synth_bus_ald)(uint8_t allophone)
{
	if (bus_full || chip_busy)
		return false;
	bus_val  = allophone;
	bus_full = true;
	gpio_put(PIN_LRQ, 1);
	return true;
}

bool synth_queue(uint8_t allophone)
{
	uint32_t next = (hq_head + 1) % HOST_QUEUE;
	if (next == hq_tail) return false;
	hq[hq_head] = allophone;
	hq_head = next;
	return true;
}

void synth_queue_flush(void) { hq_tail = hq_head; }
int  synth_queue_count(void)  { return (int)((hq_head + HOST_QUEUE - hq_tail) % HOST_QUEUE); }

void synth_request_reset(void) { reset_req = true; }
void synth_stop(void)          { synth_queue_flush(); stop_req = true; }

void synth_set_clock(uint32_t hz)
{
	if (hz < CLOCK_MIN_HZ) hz = CLOCK_MIN_HZ;
	if (hz > CLOCK_MAX_HZ) hz = CLOCK_MAX_HZ;
	clock_req = hz;
}
uint32_t synth_get_clock(void) { return clock_hz; }
void synth_set_speed(int pct)  { speed_pct = pct; ctl_req = true; }
void synth_set_pitch(int pct)  { pitch_pct = pct; ctl_req = true; }
int  synth_get_speed(void)     { return chip.speed_pct; }
int  synth_get_pitch(void)     { return chip.pitch_pct; }

bool     synth_busy(void)   { return bus_full || chip_busy; }
bool     synth_idle(void)   { return sp0256_sby_pin(&chip) && !bus_full; }
uint32_t synth_played(void) { return chip.started; }

/* ---------------------------------------------------------------------- */
/*  Core 1                                                                */
/* ---------------------------------------------------------------------- */
static inline void feed(void)
{
	if (bus_full) {
		if (sp0256_ald(&chip, bus_val))
			bus_full = false;
	} else if (!sp0256_lrq_pin(&chip) && hq_tail != hq_head) {
		sp0256_ald(&chip, hq[hq_tail]);
		hq_tail = (hq_tail + 1) % HOST_QUEUE;
	}
}

static inline void update_pins(void)
{
	chip_busy = sp0256_lrq_pin(&chip) != 0;
	gpio_put(PIN_LRQ, bus_full || chip_busy);
	gpio_put(PIN_SBY, sp0256_sby_pin(&chip));
}

static void core1_main(void)
{
	int16_t s;

	sp0256_init(&chip, sp0256_al2_rom, sizeof sp0256_al2_rom);
	audio_init(clock_hz / SP0256_CLOCK_DIVIDER);
	update_pins();

	for (;;) {
		if (reset_req || stop_req) {
			reset_req = stop_req = false;
			bus_full  = false;
			sp0256_reset(&chip);
			update_pins();
		}
		if (clock_req) {
			clock_hz  = clock_req;
			clock_req = 0;
			audio_set_rate(clock_hz / SP0256_CLOCK_DIVIDER);
		}
		if (ctl_req) {
			ctl_req = false;
			sp0256_set_speed(&chip, speed_pct);
			sp0256_set_pitch(&chip, pitch_pct);
		}

		audio_begin_block();
		for (int i = 0; i < AUDIO_BLOCK; i++) {
			feed();
			sp0256_render(&chip, &s, 1);
			audio_put_sample(s);
			update_pins();
		}
		audio_end_block();
	}
}

void synth_start(void)
{
	gpio_init(PIN_LRQ); gpio_set_dir(PIN_LRQ, GPIO_OUT); gpio_put(PIN_LRQ, 0);
	gpio_init(PIN_SBY); gpio_set_dir(PIN_SBY, GPIO_OUT); gpio_put(PIN_SBY, 1);
	multicore_launch_core1(core1_main);
}
