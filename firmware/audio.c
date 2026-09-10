/* audio.c -- back-end dispatcher. */
#include "pico/stdlib.h"
#include "config.h"
#include "audio.h"

static uint32_t         rate;
static audio_render_fn  render;
static int              backend = AUDIO_PWM;
static bool             running;

static void start(int be)
{
	if (running) {
		if (backend == AUDIO_PWM) audio_pwm_stop();
#if HAS_I2S
		else audio_i2s_stop();
#endif
	}
	backend = be;
#if HAS_I2S
	if (backend == AUDIO_I2S) audio_i2s_init(rate); else
#endif
	audio_pwm_init(rate);
	running = true;
}

void audio_init(uint32_t synth_rate, audio_render_fn fn)
{
	rate   = synth_rate;
	render = fn;
	start(HAS_I2S ? AUDIO_I2S : AUDIO_PWM);
}

void audio_set_rate(uint32_t synth_rate)
{
	rate = synth_rate;
#if HAS_I2S
	if (backend == AUDIO_I2S) { audio_i2s_set_rate(rate); return; }
#endif
	audio_pwm_set_rate(rate);
}

uint32_t audio_get_rate(void) { return rate; }

void audio_process(void)
{
#if HAS_I2S
	if (backend == AUDIO_I2S) { audio_i2s_process(render); return; }
#endif
	audio_pwm_process(render);
}

uint32_t audio_underruns(void)
{
#if HAS_I2S
	if (backend == AUDIO_I2S) return audio_i2s_underruns();
#endif
	return audio_pwm_underruns();
}

void audio_select(int be)
{
#if !HAS_I2S
	be = AUDIO_PWM;
#endif
	if (be != backend) start(be);
}

int audio_backend(void) { return backend; }
const char *audio_backend_name(void) { return backend == AUDIO_I2S ? "I2S speaker" : "PWM line-out"; }
