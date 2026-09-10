/*
 * audio_i2s.c -- I2S output for the MAX98357A at 32 kHz.
 *
 * The amplifier accepts LRCLK only at 8, 16, 32, 44.1, 48, 88.2 or 96 kHz.
 * The chip's stream is 10 kHz (clock / 312), so the back end interpolates:
 * a Q16 phase accumulator steps through the synth samples at synth_rate /
 * 32000 per frame and linearly interpolates between the last two.  Changing
 * the "clock" control changes the step, not the frame clock.
 *
 * Two chained DMA channels feed the PIO TX FIFO, one 32-bit word per frame.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "audio_i2s.pio.h"
#include "config.h"
#include "audio.h"

#if HAS_I2S

static uint32_t          buf[2][I2S_BLOCK];
static volatile bool     block_free[2];
static volatile uint32_t underruns;
static int               dma_ch[2] = { -1, -1 };
static int               fill_idx;
static bool              irq_installed;
static PIO               pio = pio1;
static uint              sm;
static uint              offset;
static bool              pio_loaded;

/* interpolation state */
static uint32_t          step;          /* Q16 synth samples per frame */
static uint32_t          phase;         /* Q16 */
static int16_t           s_prev, s_cur;

static void __isr dma_irq_handler(void)
{
	for (int i = 0; i < 2; i++) {
		if (dma_ch[i] >= 0 && dma_channel_get_irq0_status(dma_ch[i])) {
			dma_channel_acknowledge_irq0(dma_ch[i]);
			if (block_free[i]) underruns++;
			block_free[i] = true;
			dma_channel_set_read_addr(dma_ch[i], buf[i], false);
			dma_channel_set_trans_count(dma_ch[i], I2S_BLOCK, false);
		}
	}
}

void audio_i2s_set_rate(uint32_t synth_rate)
{
	step = (uint32_t)(((uint64_t)synth_rate << 16) / I2S_RATE);
}

void audio_i2s_init(uint32_t synth_rate)
{
	audio_i2s_set_rate(synth_rate);
	phase = 0; s_prev = s_cur = 0;

	if (!pio_loaded) {
		offset = pio_add_program(pio, &audio_i2s_program);
		sm = pio_claim_unused_sm(pio, true);
		pio_loaded = true;
	}
	audio_i2s_program_init(pio, sm, offset, PIN_I2S_DIN, PIN_I2S_BCLK);
	/* 64 PIO cycles per frame (32 bits x 2 cycles per bit). */
	float div = (float)clock_get_hz(clk_sys) / ((float)I2S_RATE * 64.0f);
	pio_sm_set_clkdiv(pio, sm, div);

	for (int i = 0; i < 2; i++) {
		memset(buf[i], 0, sizeof buf[i]);
		block_free[i] = false;
	}
	for (int i = 0; i < 2; i++) if (dma_ch[i] < 0) dma_ch[i] = dma_claim_unused_channel(true);
	for (int i = 0; i < 2; i++) {
		dma_channel_config c = dma_channel_get_default_config(dma_ch[i]);
		channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
		channel_config_set_read_increment(&c, true);
		channel_config_set_write_increment(&c, false);
		channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
		channel_config_set_chain_to(&c, dma_ch[i ^ 1]);
		dma_channel_configure(dma_ch[i], &c, &pio->txf[sm], buf[i], I2S_BLOCK, false);
		dma_channel_set_irq0_enabled(dma_ch[i], true);
	}
	if (!irq_installed) {
		irq_add_shared_handler(DMA_IRQ_0, dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
		irq_set_enabled(DMA_IRQ_0, true);
		irq_installed = true;
	}
	fill_idx = 0;
	pio_sm_set_enabled(pio, sm, true);
	dma_channel_start(dma_ch[0]);
}

void audio_i2s_stop(void)
{
	for (int i = 0; i < 2; i++) {
		dma_channel_set_irq0_enabled(dma_ch[i], false);
		dma_channel_abort(dma_ch[i]);
	}
	pio_sm_set_enabled(pio, sm, false);      /* no BCLK: the amp goes to standby */
}

uint32_t audio_i2s_underruns(void) { return underruns; }

void audio_i2s_process(audio_render_fn render)
{
	while (!block_free[fill_idx])
		__wfe();

	uint32_t *p = buf[fill_idx];
	for (int i = 0; i < I2S_BLOCK; i++) {
		phase += step;
		while (phase >= 0x10000u) {
			phase -= 0x10000u;
			s_prev = s_cur;
			render(&s_cur, 1);
		}
		int32_t frac = (int32_t)(phase & 0xFFFF);
		int32_t v = s_prev + (((int32_t)(s_cur - s_prev) * frac) >> 16);
		uint32_t half = (uint32_t)(uint16_t)(int16_t)v;
		p[i] = half | (half << 16);               /* same sample, both channels */
	}
	block_free[fill_idx] = false;
	fill_idx ^= 1;
}

#endif /* HAS_I2S */
