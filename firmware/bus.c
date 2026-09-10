/*
 * bus.c -- Z80 I/O bus capture.
 *
 * Two PIO state machines watch /ALD and /TXT.  Each captured byte arrives in
 * the RX FIFO and raises PIO IRQ0, which runs on core 0.  The handler hands
 * allophones straight to the synth's one-deep input buffer (so /LRQ changes
 * within a couple of microseconds of the OUT, before the host can poll it)
 * and text/control bytes to the text layer.
 */
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "bus.pio.h"
#include "config.h"
#include "bus.h"
#include "synth.h"
#include "text.h"

volatile bus_stats_t bus_stats;

static PIO  pio;
static uint sm_ald, sm_txt;

static void __isr __time_critical_func(pio_irq_handler)(void)
{
	while (!pio_sm_is_rx_fifo_empty(pio, sm_ald)) {
		uint8_t v = (uint8_t)pio_sm_get(pio, sm_ald);
		bus_stats.ald_writes++;
		if (!synth_bus_ald(v & 0x3F))          /* A7, A8 tied low on the ZX Voice */
			bus_stats.ald_dropped++;
	}
	while (!pio_sm_is_rx_fifo_empty(pio, sm_txt)) {
		uint8_t v = (uint8_t)pio_sm_get(pio, sm_txt);
		bus_stats.txt_writes++;
		text_bus_byte(v);
	}
}

static void __isr gpio_irq_handler(uint gpio, uint32_t events)
{
	(void)events;
#if HAS_RDSTAT
	if (gpio == PIN_RDSTAT) { bus_stats.status_reads++; return; }
#endif
	if (gpio == PIN_RESET) {
		bus_stats.resets++;
		synth_request_reset();
		text_reset();
	}
}

void bus_init(void)
{
	/* Data bus.  D6/D7 are unconnected on the 2023 board: pull them down. */
	for (uint i = 0; i < 8; i++) {
		gpio_init(PIN_D0 + i);
		gpio_set_dir(PIN_D0 + i, GPIO_IN);
		gpio_set_pulls(PIN_D0 + i, false, i >= 6);
	}
	/* Strobes are active low; /TXT is unconnected on the 2023 board. */
	gpio_init(PIN_ALD); gpio_set_dir(PIN_ALD, GPIO_IN); gpio_pull_up(PIN_ALD);
	gpio_init(PIN_TXT); gpio_set_dir(PIN_TXT, GPIO_IN); gpio_pull_up(PIN_TXT);

	pio = pio0;
	uint offset = pio_add_program(pio, &strobe_capture_program);
	sm_ald = pio_claim_unused_sm(pio, true);
	sm_txt = pio_claim_unused_sm(pio, true);
	strobe_capture_program_init(pio, sm_ald, offset, PIN_D0, PIN_ALD);
	strobe_capture_program_init(pio, sm_txt, offset, PIN_D0, PIN_TXT);

	pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty + sm_ald, true);
	pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty + sm_txt, true);
	irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
	irq_set_priority(PIO0_IRQ_0, 0);              /* highest: /LRQ must follow fast */
	irq_set_enabled(PIO0_IRQ_0, true);

	/* Bus /RESET (Rev B).  Unconnected on the 2023 board: pulled up. */
	gpio_init(PIN_RESET);
	gpio_set_dir(PIN_RESET, GPIO_IN);
	gpio_pull_up(PIN_RESET);
	gpio_set_irq_enabled_with_callback(PIN_RESET, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
#if HAS_RDSTAT
	gpio_init(PIN_RDSTAT);
	gpio_set_dir(PIN_RDSTAT, GPIO_IN);
	gpio_pull_up(PIN_RDSTAT);
	gpio_set_irq_enabled(PIN_RDSTAT, GPIO_IRQ_EDGE_FALL, true);
#endif
}
