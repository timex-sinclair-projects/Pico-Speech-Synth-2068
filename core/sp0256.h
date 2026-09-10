/*
 * sp0256.h -- portable SP0256 Narrator Speech Processor emulation core.
 *
 * Derived from MAME's src/devices/sound/sp0256.cpp
 *   license: BSD-3-Clause
 *   copyright-holders: Joseph Zbiciak, Tim Lindner
 * Stripped of the MAME device framework and the SPB640 FIFO so that it
 * runs unchanged on a desktop test harness and on an RP2040 (no malloc,
 * no floating point, no dependencies beyond <stdint.h>).
 *
 * Usage:
 *   sp0256_t sp;
 *   sp0256_init(&sp, rom, rom_len);          // 2 KB SP0256-AL2 image
 *   sp0256_ald(&sp, 27);                     // load allophone HH1
 *   sp0256_render(&sp, buf, n);              // n samples at clock/312 Hz
 *
 * Pin model:
 *   sp0256_lrq_pin()  1 = input buffer full (do not load), 0 = may load.
 *                     Matches the physical /LRQ pin polarity.
 *   sp0256_sby_pin()  1 = sequencer idle (standby), 0 = talking.
 */
#ifndef SP0256_H
#define SP0256_H

#include <stdint.h>

#define SP0256_CLOCK_DIVIDER 312          /* sample rate = clock / 312     */
#define SP0256_ROM_BASE      0x1000       /* AL2 image lives at 0x1000     */

typedef struct {
	int       rpt, cnt;       /* repeat counter, period down-counter    */
	uint32_t  per, rng;       /* period, random number generator        */
	int       amp;            /* amplitude                              */
	int16_t   f_coef[6];      /* F0..F5                                 */
	int16_t   b_coef[6];      /* B0..B5                                 */
	int16_t   z_data[6][2];   /* filter delay line                      */
	uint8_t   r[16];          /* encoded register set                   */
	int       interp;         /* interpolation active                   */
	int       pitch_pct;      /* copy of the pitch control for interp   */
} sp0256_lpc12_t;

typedef struct {
	const uint8_t *rom;       /* ROM image, mapped at SP0256_ROM_BASE   */
	uint32_t       rom_len;

	sp0256_lpc12_t filt;

	int       silent;         /* 1 while the chip is silent             */
	int       lrq;            /* 1 = input buffer empty (may load)      */
	int       ald;            /* pending address (PC units)             */
	int       pc;             /* microsequencer PC, in bits             */
	int       stack;          /* one-deep return stack                  */
	int       halted;         /* 1 when the sequencer is halted         */
	int       sby;            /* standby pin level                      */
	uint32_t  mode;           /* mode register                          */
	uint32_t  page;           /* page set by SETPAGE                    */
	uint32_t  started;        /* count of commands the sequencer began  */

	int       speed_pct;      /* speaking speed, 50..200 (100)          */
	int       pitch_pct;      /* voiced pitch scale, 50..200 (100)      */
} sp0256_t;

void sp0256_init(sp0256_t *sp, const uint8_t *rom, uint32_t rom_len);
void sp0256_reset(sp0256_t *sp);

/* Load an address (allophone number).  Returns 1 if accepted, 0 if the
 * input buffer was full and the write was dropped, as on the real chip. */
int  sp0256_ald(sp0256_t *sp, uint8_t addr);

int  sp0256_lrq_pin(const sp0256_t *sp);
int  sp0256_sby_pin(const sp0256_t *sp);
int  sp0256_halted(const sp0256_t *sp);

/* Generate exactly n samples (signed 16-bit) into out.  Always returns n. */
int  sp0256_render(sp0256_t *sp, int16_t *out, int n);

/* Playback controls, in percent, clamped to SP0256_PCT_MIN..SP0256_PCT_MAX.
 * Speed scales how fast frames go by without changing pitch (200 = twice
 * as fast); pitch scales the voiced pitch without changing duration.
 * 100 = as the chip.
 * A third control, the clock, is not in the core: change the rate at which
 * the caller consumes samples (clock/312), which moves pitch and speed
 * together exactly as a different crystal would on the real part.      */
#define SP0256_PCT_MIN 50
#define SP0256_PCT_MAX 200
void sp0256_set_speed(sp0256_t *sp, int percent);
void sp0256_set_pitch(sp0256_t *sp, int percent);

#endif /* SP0256_H */
