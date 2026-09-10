/*
 * audio.c -- PWM output through DMA.
 *
 * The PWM slice runs at OVERSAMPLE x the synth sample rate (100 kHz at
 * 10 kHz).  Two DMA channels, chained to each other, stream 32-bit words into
 * the slice's CC register, paced by the slice's DREQ, one word per PWM
 * period.  Each synth sample is written OVERSAMPLE times (zero-order hold),
 * which is what the chip's own per-sample PWM amounted to; the RC filter on
 * the board removes the carrier.
 *
 * Changing the sample rate only changes the PWM wrap value, so the "clock"
 * control moves pitch and speed together exactly as a crystal change would.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "config.h"
#include "audio.h"

#define WORDS_PER_BLOCK (AUDIO_BLOCK * OVERSAMPLE)

static uint32_t          buf[2][WORDS_PER_BLOCK];
static volatile bool     block_free[2];
static volatile uint32_t underruns;
static int               dma_ch[2];
static uint              slice;
static uint32_t          rate;
static uint32_t          wrap;          /* PWM top value              */

static int               fill_idx;      /* block core 1 is filling    */
static int               fill_pos;

static void __isr dma_irq_handler(void)
{
	for (int i = 0; i < 2; i++) {
		if (dma_channel_get_irq0_status(dma_ch[i])) {
			dma_channel_acknowledge_irq0(dma_ch[i]);
			/* Re-arm this channel for its next turn (the other channel's
			 * chain trigger will start it).  If core 1 has not refilled it
			 * by then, the old contents replay: count that as an underrun. */
			if (block_free[i]) underruns++;
			block_free[i] = true;
			dma_channel_set_read_addr(dma_ch[i], buf[i], false);
			dma_channel_set_trans_count(dma_ch[i], WORDS_PER_BLOCK, false);
		}
	}
}

static void set_wrap_for_rate(uint32_t r)
{
	uint32_t sys = clock_get_hz(clk_sys);
	wrap = (sys + (r * OVERSAMPLE) / 2) / (r * OVERSAMPLE) - 1;
	if (wrap < 255) wrap = 255;
	if (wrap > 65535) wrap = 65535;
	pwm_set_wrap(slice, (uint16_t)wrap);
}

void audio_init(uint32_t sample_rate)
{
	rate = sample_rate;

	gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);
	slice = pwm_gpio_to_slice_num(PIN_PWM);
	pwm_set_clkdiv_int_frac(slice, 1, 0);
	set_wrap_for_rate(rate);
	pwm_set_both_levels(slice, (uint16_t)(wrap / 2), (uint16_t)(wrap / 2));
	pwm_set_enabled(slice, true);

	/* Silence in both blocks; both "in use" until the DMA has played them. */
	uint32_t mid = (wrap / 2) | ((wrap / 2) << 16);
	for (int i = 0; i < 2; i++) {
		for (int k = 0; k < WORDS_PER_BLOCK; k++) buf[i][k] = mid;
		block_free[i] = false;
	}

	for (int i = 0; i < 2; i++) dma_ch[i] = dma_claim_unused_channel(true);

	for (int i = 0; i < 2; i++) {
		dma_channel_config c = dma_channel_get_default_config(dma_ch[i]);
		channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
		channel_config_set_read_increment(&c, true);
		channel_config_set_write_increment(&c, false);
		channel_config_set_dreq(&c, pwm_get_dreq(slice));
		channel_config_set_chain_to(&c, dma_ch[i ^ 1]);
		dma_channel_configure(dma_ch[i], &c, &pwm_hw->slice[slice].cc,
		                      buf[i], WORDS_PER_BLOCK, false);
		dma_channel_set_irq0_enabled(dma_ch[i], true);
	}
	irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
	irq_set_enabled(DMA_IRQ_0, true);

	fill_idx = 0;
	fill_pos = 0;
	dma_channel_start(dma_ch[0]);
}

void audio_set_rate(uint32_t sample_rate)
{
	rate = sample_rate;
	set_wrap_for_rate(rate);
}

uint32_t audio_get_rate(void) { return rate; }
uint32_t audio_underruns(void) { return underruns; }

void audio_begin_block(void)
{
	while (!block_free[fill_idx])
		__wfe();
	fill_pos = 0;
}

void audio_put_sample(int16_t s)
{
	/* -32768..32767 -> 0..wrap, same level on both PWM channels. */
	uint32_t level = ((uint32_t)((int32_t)s + 32768) * (wrap + 1)) >> 16;
	uint32_t word  = level | (level << 16);
	uint32_t *p = &buf[fill_idx][fill_pos];
	for (int k = 0; k < OVERSAMPLE; k++) p[k] = word;
	fill_pos += OVERSAMPLE;
}

void audio_end_block(void)
{
	block_free[fill_idx] = false;
	fill_idx ^= 1;
	__sev();
}
