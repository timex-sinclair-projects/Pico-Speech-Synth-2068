/*
 * text.c -- OUT 55: ASCII text and control bytes, and the console's SAY.
 *
 * Control bytes (bit 7 set) act immediately.  Printable bytes accumulate in
 * a buffer that is handed to the text-to-allophone engine at '.', '!', '?',
 * CR, NUL, or when full.  The engine's allophones go into the synth queue,
 * behind anything the Z80 sends on OUT 23.
 *
 *   0x80        stop (flush queue, silence)
 *   0x81        flush text buffer (speak now)
 *   0x90+n      pitch, n = 0..15 -> 50..200 %
 *   0xA0+n      speed, n = 0..15 -> 50..200 %
 *   0xB0+n      clock, n = 0..15 -> 2.5..4.0 MHz (0xB4 = 3.12, 0xB5 = 3.25)
 *   0xC0        echo allophone codes to the console (toggle)
 *   0xC8+n      filter (reserved)
 *   0xD0 / 0xD1 text engine: NRL / CTS256
 *   0xE0+n      volume (I2S board, Rev B)
 *   0xFF        reset
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "config.h"
#include "sp0256_names.h"
#include "tts.h"
#include "text.h"
#include "synth.h"

static volatile uint8_t  tbuf[TEXT_BUFFER];
static volatile int      tlen;
static volatile bool     tflush;
static volatile uint32_t nchars, nctl;
static volatile int      engine = TTS_NRL;
static volatile bool     echo;

static int pct_from_nibble(int n)         /* 0..15 -> 50..200 */
{
	return 50 + n * 10;
}

void text_init(void) { tlen = 0; }
void text_reset(void) { tlen = 0; tflush = false; }

void text_set_engine(int e) { engine = e ? TTS_CTS256 : TTS_NRL; }
const char *text_engine_name(void) { return tts_engine_name((tts_engine_t)engine); }

void __time_critical_func(text_bus_byte)(uint8_t v)
{
	if (v & 0x80) {
		nctl++;
		if      (v == 0x80) synth_stop();
		else if (v == 0x81) tflush = true;
		else if (v == 0xFF) { synth_request_reset(); tlen = 0; }
		else if ((v & 0xF0) == 0x90) synth_set_pitch(pct_from_nibble(v & 15));
		else if ((v & 0xF0) == 0xA0) synth_set_speed(pct_from_nibble(v & 15));
		else if ((v & 0xF0) == 0xB0) synth_set_clock(CLOCK_MIN_HZ + (v & 15) * 100000u);
		else if (v == 0xC0) echo = !echo;
		else if (v == 0xD0 || v == 0xD1) text_set_engine(v & 1);
		return;
	}
	nchars++;
	if (v == 0 || v == '\r' || v == '\n') { tflush = true; return; }
	if (v < 0x20) return;
	if (tlen < TEXT_BUFFER) tbuf[tlen++] = v;
	if (v == '.' || v == '!' || v == '?' || tlen >= TEXT_BUFFER) tflush = true;
}

static void emit_to_synth(void *ctx, uint8_t a)
{
	(void)ctx;
	while (!synth_queue(a))               /* queue full: wait for room */
		sleep_ms(1);
	if (echo) printf("%s ", sp0256_names[a]);
}

void text_say(const char *line)
{
	tts_speak((tts_engine_t)engine, line, emit_to_synth, NULL);
	if (echo) printf("\n");
}

void text_poll(void)
{
	char line[TEXT_BUFFER + 1];
	int n;

	if (!tflush) return;
	tflush = false;
	n = tlen;
	if (n == 0) return;
	memcpy(line, (const void *)tbuf, (size_t)n);
	line[n] = 0;
	tlen = 0;
	text_say(line);
}

uint32_t text_chars(void)    { return nchars; }
uint32_t text_controls(void) { return nctl; }
int      text_pending(void)  { return tlen; }
