/* audio.h -- PWM audio output fed by DMA, double buffered. */
#pragma once
#include <stdint.h>

void      audio_init(uint32_t sample_rate);
void      audio_set_rate(uint32_t sample_rate);   /* synth sample rate, Hz */
uint32_t  audio_get_rate(void);

/* Core 1 side.  Get the next free half-buffer (blocks until one is free),
 * fill it with AUDIO_BLOCK synth samples via audio_put_sample(), submit. */
void      audio_begin_block(void);
void      audio_put_sample(int16_t s);
void      audio_end_block(void);

uint32_t  audio_underruns(void);
