/*
 * console.c -- USB serial command interface (core 0).
 *
 * The same commands as the 2023 MicroPython firmware, plus the playback
 * controls.  Lines are terminated by CR or LF; commands are case-insensitive.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "config.h"
#include "sp0256_names.h"
#include "console.h"
#include "synth.h"
#include "bus.h"
#include "text.h"
#include "audio.h"
#include "tts.h"
#include "board.h"

static char line[256];
static int  llen;

static void help(void)
{
	printf(
	"ZX Voice replica " FW_VERSION "\n"
	"  SPEAK <allophones>   names or numbers, e.g. SPEAK HH1 EH LL AX OW PA5\n"
	"  SAY <text>           English text through the current engine\n"
	"  ENGINE NRL|CTS       choose the text engine\n"
	"  HELLO                \"hello world\"\n"
	"  LIST                 allophone table\n"
	"  STATUS               pins, counters, settings\n"
	"  STOP                 flush the queue and go silent\n"
	"  RESET                reset the chip (as /RESET)\n"
	"  SPEED <50..200>      speaking speed, percent\n"
	"  PITCH <50..200>      pitch, percent\n"
	"  CLOCK <2500000..4000000>  emulated crystal, Hz (3250000 = TS1000)\n"
	"  OUTPUT SPEAKER|LINE  audio path (Rev B)\n"
	"  HELP\n");
}

static void speak(char *args)
{
	int n = 0, bad = 0;
	for (char *tok = strtok(args, " ,\t"); tok; tok = strtok(NULL, " ,\t")) {
		int v = sp0256_lookup(tok);
		if (v < 0) { printf("? unknown allophone '%s'\n", tok); bad++; continue; }
		if (!synth_queue((uint8_t)v)) { printf("? queue full\n"); break; }
		n++;
	}
	if (n && !bad) printf("queued %d\n", n);
}

static void list(void)
{
	for (int i = 0; i < 64; i++)
		printf("%2d %-4s %3d ms%s", i, sp0256_names[i], sp0256_table6_ms[i], (i % 4 == 3) ? "\n" : "   ");
}

static void status(void)
{
	printf("board    : %s (id %d), host %s, bus clock %lu Hz\n", BOARD_NAME, board_id(), board_host_name(), (unsigned long)board_bus_clock_hz());
	printf("chip     : %s, %s, %lu allophones started\n",
	       synth_busy() ? "LRQ busy" : "LRQ ready", synth_idle() ? "SBY idle" : "talking",
	       (unsigned long)synth_played());
	printf("bus      : OUT23 %lu (dropped %lu), OUT55 %lu, IN39 %lu, resets %lu\n",
	       (unsigned long)bus_stats.ald_writes, (unsigned long)bus_stats.ald_dropped,
	       (unsigned long)bus_stats.txt_writes, (unsigned long)bus_stats.status_reads, (unsigned long)bus_stats.resets);
	printf("text     : %lu chars, %lu controls, %d pending\n",
	       (unsigned long)text_chars(), (unsigned long)text_controls(), text_pending());
	printf("queue    : %d waiting\n", synth_queue_count());
	printf("engine   : %s\n", text_engine_name());
	printf("clock    : %lu Hz -> %lu samples/s\n", (unsigned long)synth_get_clock(), (unsigned long)audio_get_rate());
	printf("speed    : %d %%   pitch: %d %%\n", synth_get_speed(), synth_get_pitch());
	printf("audio    : %s, %lu underruns\n", audio_backend_name(), (unsigned long)audio_underruns());
}

static void run(char *cmd)
{
	char *args = cmd;
	while (*args && !isspace((unsigned char)*args)) { *args = (char)toupper((unsigned char)*args); args++; }
	if (*args) *args++ = 0;
	while (isspace((unsigned char)*args)) args++;

	if (!*cmd) return;
	else if (!strcmp(cmd, "HELP") || !strcmp(cmd, "?")) help();
	else if (!strcmp(cmd, "SPEAK")) speak(args);
	else if (!strcmp(cmd, "SAY")) text_say(args);
	else if (!strcmp(cmd, "ENGINE")) { text_set_engine((args[0] == 'C' || args[0] == 'c') ? 1 : 0); printf("engine %s\n", text_engine_name()); }
	else if (!strcmp(cmd, "HELLO")) { char h[] = "HH1 EH LL AX OW PA5 WW ER1 LL DD1 PA5"; speak(h); }
	else if (!strcmp(cmd, "LIST")) list();
	else if (!strcmp(cmd, "STATUS")) status();
	else if (!strcmp(cmd, "STOP")) { synth_stop(); printf("stopped\n"); }
	else if (!strcmp(cmd, "RESET")) { synth_request_reset(); text_reset(); printf("reset\n"); }
	else if (!strcmp(cmd, "SPEED")) { synth_set_speed(atoi(args)); printf("speed %d %%\n", synth_get_speed()); }
	else if (!strcmp(cmd, "PITCH")) { synth_set_pitch(atoi(args)); printf("pitch %d %%\n", synth_get_pitch()); }
	else if (!strcmp(cmd, "CLOCK")) { synth_set_clock((uint32_t)atol(args)); printf("clock requested %s Hz\n", args); }
	else if (!strcmp(cmd, "OUTPUT")) { audio_select((args[0] == 'S' || args[0] == 's') ? AUDIO_I2S : AUDIO_PWM); printf("output %s\n", audio_backend_name()); }
	else printf("? unknown command '%s' (HELP)\n", cmd);
}

void console_init(void)
{
	llen = 0;
}

void console_poll(void)
{
	int c;
	while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
		if (c == '\r' || c == '\n') {
			putchar('\n');
			line[llen] = 0;
			run(line);
			llen = 0;
			printf("> ");
		} else if (c == 8 || c == 127) {
			if (llen) { llen--; printf("\b \b"); }
		} else if (c >= 32 && llen < (int)sizeof line - 1) {
			line[llen++] = (char)c;
			putchar(c);
		}
	}
}
