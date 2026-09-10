/*
 * say.c -- desktop text-to-speech tool.
 *
 *   say [-e nrl|cts] [-r ROM] [-o out.wav] [-c CLOCK] [-t SPEED] [-p PITCH] "text"
 *
 * Prints the allophone sequence and, with -r, renders it through the core.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/sp0256.h"
#include "../core/sp0256_names.h"
#include "../tts/tts.h"
#include "wav.h"

static uint8_t seq[4096];
static int     nseq;

static void collect(void *ctx, uint8_t a)
{
	(void)ctx;
	if (nseq < (int)sizeof seq) seq[nseq++] = a;
}

int main(int argc, char **argv)
{
	tts_engine_t engine = TTS_NRL;
	const char *rom_path = NULL, *out = NULL, *text = NULL;
	uint32_t clock = 3120000;
	int speed = 100, pitch = 100, i, quiet = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-e") && i + 1 < argc) {
			i++;
			engine = (argv[i][0] == 'c' || argv[i][0] == 'C') ? TTS_CTS256 : TTS_NRL;
		}
		else if (!strcmp(argv[i], "-r") && i + 1 < argc) rom_path = argv[++i];
		else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) clock = (uint32_t)atol(argv[++i]);
		else if (!strcmp(argv[i], "-t") && i + 1 < argc) speed = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-p") && i + 1 < argc) pitch = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-q")) quiet = 1;
		else text = argv[i];
	}
	if (!text) {
		fprintf(stderr, "usage: say [-e nrl|cts] [-r ROM -o out.wav] [-c CLOCK] [-t SPEED] [-p PITCH] \"text\"\n");
		return 2;
	}

	tts_speak(engine, text, collect, NULL);

	if (!quiet) {
		printf("%s: ", tts_engine_name(engine));
		for (i = 0; i < nseq; i++) printf("%s ", sp0256_names[seq[i]]);
		printf("(%d)\n", nseq);
	}

	if (rom_path && out) {
		uint32_t rom_len, rate = clock / SP0256_CLOCK_DIVIDER;
		uint8_t *rom = wav_load_rom(rom_path, &rom_len);
		sp0256_t sp;
		int16_t *pcm;
		uint32_t cap = (uint32_t)nseq * rate + rate, pos = 0;
		int k = 0;

		if (!rom) return 1;
		pcm = calloc(cap, sizeof(int16_t));
		sp0256_init(&sp, rom, rom_len);
		sp0256_set_speed(&sp, speed);
		sp0256_set_pitch(&sp, pitch);
		while (k < nseq || !sp0256_sby_pin(&sp)) {
			if (k < nseq && !sp0256_lrq_pin(&sp)) { sp0256_ald(&sp, seq[k++]); continue; }
			if (pos >= cap) break;
			sp0256_render(&sp, pcm + pos, 1);
			pos++;
		}
		wav_write(out, pcm, pos, rate);
		if (!quiet) printf("%.2f s -> %s\n", pos / (double)rate, out);
		free(pcm); free(rom);
	}
	return 0;
}
