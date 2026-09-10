/* wav.h -- tiny WAV writer and ROM loader shared by the desktop tools. */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void wav_w32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void wav_w16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static int wav_write(const char *path, const int16_t *pcm, uint32_t n, uint32_t rate)
{
	FILE *f = fopen(path, "wb");
	uint32_t i;
	if (!f) { perror(path); return -1; }
	fwrite("RIFF", 1, 4, f); wav_w32(f, 36 + n * 2); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); wav_w32(f, 16); wav_w16(f, 1); wav_w16(f, 1);
	wav_w32(f, rate); wav_w32(f, rate * 2); wav_w16(f, 2); wav_w16(f, 16);
	fwrite("data", 1, 4, f); wav_w32(f, n * 2);
	for (i = 0; i < n; i++) wav_w16(f, (uint16_t)pcm[i]);
	fclose(f);
	return 0;
}

static uint8_t wav_bitrev8(uint8_t v)
{
	v = (uint8_t)(((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
	v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
	v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
	return v;
}

/* Load an SP0256-AL2 image in either bit order; returns MAME order. */
static uint8_t *wav_load_rom(const char *path, uint32_t *len)
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
	if (((buf[0] >> 4) & 15) != 0xE && ((wav_bitrev8(buf[0]) >> 4) & 15) == 0xE)
		for (i = 0; i < *len; i++) buf[i] = wav_bitrev8(buf[i]);
	return buf;
}
