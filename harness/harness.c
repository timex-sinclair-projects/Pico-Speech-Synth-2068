/*
 * harness.c -- desktop test harness for the portable SP0256 core.
 *
 *   harness -r rom/sp0256-al2.bin -d              durations of all 64 entries vs Table 6
 *   harness -r rom/sp0256-al2.bin -a out/         same, plus one WAV per entry
 *   harness -r rom/sp0256-al2.bin -S sampler.wav  all 64 entries in one WAV, PA4 between
 *   harness -r rom/sp0256-al2.bin -s "HH1 EH LL AX OW PA5" -o hello.wav
 *   harness -c 3250000 ...                        emulate the TS1000's 3.25 MHz CPU clock
 *
 * WAV files are mono, signed 16-bit, at clock/312 Hz (10000 Hz at 3.12 MHz).
 *
 * Duration is measured from loading the entry to the moment the sequencer
 * picks up the PA1 queued behind it.  That is the entry's own program length.
 * The chip keeps sounding its last frame until the next command arrives, so
 * a WAV of an entry on its own would never end; every render here is closed
 * with a pause exactly as the datasheet instructs.
 *
 * Either bit order of the AL2 ROM is accepted: Joe Zbiciak's al2.bin
 * (CRC32 df8de0b0) or the MAME image sp0256-al2.bin (CRC32 b504ac15).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "../core/sp0256.h"

static const char *names[64] = {
	"PA1","PA2","PA3","PA4","PA5","OY","AY","EH","KK3","PP","JH","NN1","IH","TT2","RR1","AX",
	"MM","TT1","DH1","IY","EY","DD1","UW1","AO","AA","YY2","AE","HH1","BB1","TH","UH","UW2",
	"AW","DD2","GG3","VV","GG1","SH","ZH","RR2","FF","KK2","KK1","ZZ","NG","LL","WW","XR",
	"WH","YY1","CH","ER1","ER2","OW","DH2","SS","NN2","HH2","OR","AR","YR","GG2","EL","BB2"
};

/* Datasheet Table 6 durations, ms. */
static const int table6_ms[64] = {
	 10, 30, 50,100,200,420,260, 70,120,210,140,140, 70,140,170, 70,
	180,100,290,250,280, 70,100,100,100,180,120,130, 80,180,100,260,
	370,160,140,190, 80,160,190,120,150,190,160,210,220,110,180,360,
	200,130,190,160,300,240,240, 90,190,180,330,290,350, 40,190, 50
};

static int lookup(const char *tok)
{
	static const struct { const char *a; int n; } alias[] = {
		{"NN",11},{"RR",14},{"TT",17},{"DH",18},{"DD",21},{"UW",22},{"HH",27},
		{"GG",36},{"KK",42},{"YY",49},{"ER",51},{"BB",28}
	};
	int i;
	char up[8];
	for (i = 0; tok[i] && i < 7; i++) up[i] = (char)toupper((unsigned char)tok[i]);
	up[i] = 0;
	if (isdigit((unsigned char)up[0])) {
		int v = atoi(up);
		return (v >= 0 && v < 64) ? v : -1;
	}
	for (i = 0; i < 64; i++) if (!strcmp(up, names[i])) return i;
	for (i = 0; i < (int)(sizeof alias / sizeof alias[0]); i++)
		if (!strcmp(up, alias[i].a)) return alias[i].n;
	return -1;
}

/* ---------------------------------------------------------------------- */
/*  WAV output                                                            */
/* ---------------------------------------------------------------------- */
static void wr32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void wr16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static int write_wav(const char *path, const int16_t *pcm, uint32_t n, uint32_t rate)
{
	FILE *f = fopen(path, "wb");
	uint32_t i;
	if (!f) { perror(path); return -1; }
	fwrite("RIFF", 1, 4, f); wr32(f, 36 + n * 2); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); wr32(f, 16); wr16(f, 1); wr16(f, 1);
	wr32(f, rate); wr32(f, rate * 2); wr16(f, 2); wr16(f, 16);
	fwrite("data", 1, 4, f); wr32(f, n * 2);
	for (i = 0; i < n; i++) wr16(f, (uint16_t)pcm[i]);
	fclose(f);
	return 0;
}

/* ---------------------------------------------------------------------- */
/*  ROM loading, with bit-order detection                                 */
/* ---------------------------------------------------------------------- */
static uint32_t crc32_of(const uint8_t *p, uint32_t n)
{
	uint32_t c = 0xffffffffu, i;
	int k;
	for (i = 0; i < n; i++) {
		c ^= p[i];
		for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return ~c;
}

static uint8_t bitrev8(uint8_t v)
{
	v = (uint8_t)(((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
	v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
	v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
	return v;
}

static uint8_t *load_rom(const char *path, uint32_t *len)
{
	FILE *f = fopen(path, "rb");
	uint8_t *buf;
	long sz;
	uint32_t i;
	if (!f) { perror(path); return NULL; }
	fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
	buf = malloc((size_t)sz);
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
	fclose(f);
	*len = (uint32_t)sz;

	/* Entry 0 of the jump table must decode as a JMP.  The sequencer reads
	 * the low nibble first (immed4), then the high nibble (opcode 0xE).
	 * In Zbiciak's bit order every byte is reversed, so the same test
	 * passes only after reversing.                                        */
	{
		int op_mame = (buf[0] >> 4) & 15;
		int op_rev  = (bitrev8(buf[0]) >> 4) & 15;
		if (op_mame != 0xE && op_rev == 0xE) {
			for (i = 0; i < *len; i++) buf[i] = bitrev8(buf[i]);
			fprintf(stderr, "note: ROM was in Zbiciak bit order, reversed to MAME order\n");
		} else if (op_mame != 0xE) {
			fprintf(stderr, "warning: entry 0 is not a JMP (opcode %X); is this an SP0256 ROM?\n", op_mame);
		}
	}
	fprintf(stderr, "ROM %s: %u bytes, CRC32 %08x (MAME sp0256-al2.bin is b504ac15)\n",
	        path, *len, crc32_of(buf, *len));
	return buf;
}

/* ---------------------------------------------------------------------- */
/*  Rendering                                                             */
/* ---------------------------------------------------------------------- */
typedef struct {
	int16_t  *pcm;
	uint32_t  len, cap;
} pcmbuf_t;

static void pcm_reserve(pcmbuf_t *b, uint32_t extra)
{
	if (b->len + extra > b->cap) {
		b->cap = (b->len + extra) * 2 + 4096;
		b->pcm = realloc(b->pcm, b->cap * sizeof(int16_t));
	}
}

/* Feed a list of commands as a host would (load whenever /LRQ is low),
 * rendering into b.  Returns the sample index at which command `mark`
 * (0-based) was picked up by the sequencer, or 0 if mark < 0.           */
static uint32_t play(sp0256_t *sp, const int *list, int count, pcmbuf_t *b, uint32_t rate, int mark)
{
	int i = 0;
	uint32_t mark_at = 0, base = sp->started, limit = b->len + (uint32_t)count * rate * 2 + rate;

	while (i < count || !sp0256_sby_pin(sp))
	{
		if (i < count && !sp0256_lrq_pin(sp)) {
			sp0256_ald(sp, (uint8_t)list[i++]);
			continue;
		}
		if (b->len >= limit) break;                 /* runaway guard */
		pcm_reserve(b, 1);
		sp0256_render(sp, b->pcm + b->len, 1);
		b->len++;
		if (mark >= 0 && !mark_at && sp->started >= base + (uint32_t)mark + 1)
			mark_at = b->len;
	}
	return mark_at;
}

static int cmd_all(sp0256_t *sp, const char *outdir, const char *sampler, uint32_t rate)
{
	pcmbuf_t one = {0}, all = {0};
	int n, worst = 0;
	long sum_abs = 0;
	int silence = (int)(rate / 4);

	printf("%-4s %-4s %8s %9s %6s\n", "id", "name", "table6", "sequencer", "delta");
	for (n = 0; n < 64; n++)
	{
		int seq[2], ms, delta;
		uint32_t end_at;
		char path[512];

		seq[0] = n; seq[1] = 0;                     /* entry, then PA1 */
		one.len = 0;
		sp0256_reset(sp);
		end_at = play(sp, seq, 2, &one, rate, 1);
		ms = (int)((end_at * 1000ull + rate / 2) / rate);
		delta = ms - table6_ms[n];
		if (abs(delta) > abs(worst)) worst = delta;
		sum_abs += abs(delta);
		printf("%-4d %-4s %6d ms %7d ms %+5d\n", n, names[n], table6_ms[n], ms, delta);

		if (outdir) {
			snprintf(path, sizeof path, "%s/%02d_%s.wav", outdir, n, names[n]);
			write_wav(path, one.pcm, one.len, rate);
		}
		if (sampler) {
			pcm_reserve(&all, one.len + (uint32_t)silence);
			memcpy(all.pcm + all.len, one.pcm, one.len * sizeof(int16_t));
			all.len += one.len;
			memset(all.pcm + all.len, 0, (size_t)silence * sizeof(int16_t));
			all.len += (uint32_t)silence;
		}
	}
	printf("mean |delta| = %.1f ms, worst = %+d ms\n", sum_abs / 64.0, worst);
	if (sampler) {
		write_wav(sampler, all.pcm, all.len, rate);
		printf("wrote %s (%.1f s)\n", sampler, all.len / (double)rate);
	}
	free(one.pcm); free(all.pcm);
	return 0;
}

static int cmd_say(sp0256_t *sp, const char *seq, const char *out, uint32_t rate, int pad_pause)
{
	int list[2048], count = 0;
	char *copy = strdup(seq), *tok;
	pcmbuf_t b = {0};

	for (tok = strtok(copy, " ,\t\n"); tok; tok = strtok(NULL, " ,\t\n"))
	{
		int v = lookup(tok);
		if (v < 0) { fprintf(stderr, "unknown allophone '%s'\n", tok); free(copy); return 1; }
		if (count < 2047) list[count++] = v;
	}
	free(copy);
	if (pad_pause && (count == 0 || list[count - 1] > 4)) list[count++] = 4; /* PA5 */

	sp0256_reset(sp);
	play(sp, list, count, &b, rate, -1);
	printf("%d allophones, %u samples, %.3f s -> %s\n", count, b.len, b.len / (double)rate, out);
	{
		int r = write_wav(out, b.pcm, b.len, rate);
		free(b.pcm);
		return r;
	}
}

static void usage(void)
{
	fprintf(stderr,
		"usage: harness -r ROM [-c CLOCK] (-d | -a OUTDIR | -S FILE.wav | -s \"SEQ\" -o FILE.wav)\n"
		"  -r ROM      SP0256-AL2 ROM image (2048 bytes, either bit order)\n"
		"  -c CLOCK    oscillator Hz (default 3120000; TS1000 = 3250000): pitch and speed together\n"
		"  -t PCT      speed 50..200 (default 100): speed only, 200 = twice as fast\n"
		"  -p PCT      pitch 50..200 (default 100): pitch only\n"
		"  -d          durations of all 64 entries vs Table 6\n"
		"  -a OUTDIR   as -d, plus one WAV per entry (each closed with PA1)\n"
		"  -S FILE     as -d, plus all 64 entries in one WAV\n"
		"  -s SEQ      allophone names or numbers, e.g. \"HH1 EH LL AX OW PA5\"\n"
		"  -o FILE     output WAV for -s (default out.wav)\n"
		"  -n          do not append a trailing PA5 to -s sequences\n");
}

int main(int argc, char **argv)
{
	const char *rom_path = NULL, *outdir = NULL, *sampler = NULL, *seq = NULL, *out = "out.wav";
	uint32_t clock = 3120000, rom_len;
	int durations_only = 0, pad = 1, i, speed = 100, pitch = 100;
	uint8_t *rom;
	sp0256_t sp;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-r") && i + 1 < argc) rom_path = argv[++i];
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) clock = (uint32_t)atol(argv[++i]);
		else if (!strcmp(argv[i], "-a") && i + 1 < argc) outdir = argv[++i];
		else if (!strcmp(argv[i], "-S") && i + 1 < argc) sampler = argv[++i];
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) seq = argv[++i];
		else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-t") && i + 1 < argc) speed = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-p") && i + 1 < argc) pitch = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-d")) durations_only = 1;
		else if (!strcmp(argv[i], "-n")) pad = 0;
		else { usage(); return 2; }
	}
	if (!rom_path || (!outdir && !sampler && !seq && !durations_only)) { usage(); return 2; }

	rom = load_rom(rom_path, &rom_len);
	if (!rom) return 1;
	if (rom_len != 2048)
		fprintf(stderr, "warning: ROM is %u bytes, expected 2048\n", rom_len);

	sp0256_init(&sp, rom, rom_len);
	sp0256_set_speed(&sp, speed);
	sp0256_set_pitch(&sp, pitch);
	{
		uint32_t rate = clock / SP0256_CLOCK_DIVIDER;
		int r;
		printf("clock %u Hz -> sample rate %u Hz, speed %d%%, pitch %d%%\n", clock, rate, sp.speed_pct, sp.pitch_pct);
		if (seq) r = cmd_say(&sp, seq, out, rate, pad);
		else     r = cmd_all(&sp, outdir, sampler, rate);
		free(rom);
		return r;
	}
}
