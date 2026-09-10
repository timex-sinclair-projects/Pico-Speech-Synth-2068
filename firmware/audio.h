/*
 * audio.h -- audio output, pull model.
 *
 * The back end owns the DMA buffers and asks the synthesizer for samples
 * as it needs them, so back ends with different frame rates (PWM at the
 * chip's native rate, I2S at 32 kHz with interpolation) share one synth.
 *
 *   audio_init(rate, render)   once, on core 1
 *   audio_process()            core 1 loop: waits for a free half-buffer,
 *                              fills it through render(), submits it
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*audio_render_fn)(int16_t *out, int n);   /* n synth samples */

enum { AUDIO_PWM = 0, AUDIO_I2S = 1 };

void      audio_init(uint32_t synth_rate, audio_render_fn render);
void      audio_set_rate(uint32_t synth_rate);
uint32_t  audio_get_rate(void);
void      audio_process(void);
uint32_t  audio_underruns(void);

/* Rev B only: switch between the PWM line-out and the I2S amplifier. */
void      audio_select(int backend);
int       audio_backend(void);
const char *audio_backend_name(void);

/* Back ends (internal). */
void audio_pwm_init(uint32_t synth_rate);
void audio_pwm_set_rate(uint32_t synth_rate);
void audio_pwm_process(audio_render_fn render);
void audio_pwm_stop(void);
uint32_t audio_pwm_underruns(void);

void audio_i2s_init(uint32_t synth_rate);
void audio_i2s_set_rate(uint32_t synth_rate);
void audio_i2s_process(audio_render_fn render);
void audio_i2s_stop(void);
uint32_t audio_i2s_underruns(void);
