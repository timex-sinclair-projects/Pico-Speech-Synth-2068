/*
 * board.c -- board-ID jumpers and bus clock measurement.
 *
 * The bus CLK reaches GP12 through U_S.  A PIO state machine counts rising
 * edges for 20 ms; 3.25 MHz means a TS1000/ZX81, 3.5 MHz a TS2068.  On the
 * original ZX Voice the SP0256 ran from the CPU clock on the TS1000 (so 4 %
 * fast and high) and from its own 3.12 MHz crystal on the 2068, and the
 * firmware reproduces that unless the console overrides the clock.
 */
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "config.h"
#include "board.h"

static int      id = -1;
static uint32_t clk_hz;

#if HAS_CLK_SENSE
/* wait for a rising edge, then decrement X; X starts at 0xFFFFFFFF. */
static uint32_t measure_clock(void)
{
	uint16_t prog[3];
	struct pio_program edge_count_program = { .instructions = prog, .length = 3, .origin = -1 };
	prog[0] = pio_encode_wait_pin(false, 0);
	prog[1] = pio_encode_wait_pin(true, 0);
	prog[2] = pio_encode_jmp_x_dec(0);

	PIO pio = pio1;
	uint sm = pio_claim_unused_sm(pio, true);
	uint off = pio_add_program(pio, &edge_count_program);
	pio_sm_config c = pio_get_default_sm_config();
	sm_config_set_wrap(&c, off, off + 2);
	sm_config_set_in_pins(&c, PIN_CLK);
	sm_config_set_clkdiv(&c, 1.0f);
	pio_gpio_init(pio, PIN_CLK);
	pio_sm_set_consecutive_pindirs(pio, sm, PIN_CLK, 1, false);
	pio_sm_init(pio, sm, off, &c);
	pio_sm_exec(pio, sm, pio_encode_mov_not(pio_x, pio_null));   /* x = 0xFFFFFFFF */

	absolute_time_t t0 = get_absolute_time();
	pio_sm_set_enabled(pio, sm, true);
	sleep_ms(20);
	pio_sm_set_enabled(pio, sm, false);
	int64_t us = absolute_time_diff_us(t0, get_absolute_time());

	pio_sm_exec(pio, sm, pio_encode_mov(pio_isr, pio_x));
	pio_sm_exec(pio, sm, pio_encode_push(false, false));
	uint32_t x = pio_sm_get(pio, sm);
	uint32_t edges = 0xFFFFFFFFu - x;

	pio_sm_unclaim(pio, sm);
	pio_remove_program(pio, &edge_count_program, off);
	gpio_set_function(PIN_CLK, GPIO_FUNC_SIO);
	gpio_set_dir(PIN_CLK, GPIO_IN);

	if (edges < 1000) return 0;                         /* nothing toggling */
	return (uint32_t)((uint64_t)edges * 1000000u / (uint64_t)us);
}
#endif

void board_init(void)
{
#if HAS_BOARD_ID
	gpio_init(PIN_ID0); gpio_set_dir(PIN_ID0, GPIO_IN); gpio_pull_up(PIN_ID0);
	gpio_init(PIN_ID1); gpio_set_dir(PIN_ID1, GPIO_IN); gpio_pull_up(PIN_ID1);
	sleep_us(100);
	id = (gpio_get(PIN_ID0) ? 0 : 1) | (gpio_get(PIN_ID1) ? 0 : 2);   /* closed jumper = 1 */
#endif
#if HAS_CLK_SENSE
	clk_hz = measure_clock();
#endif
}

int      board_id(void)           { return id; }
uint32_t board_bus_clock_hz(void) { return clk_hz; }

const char *board_host_name(void)
{
	if (clk_hz > 3150000 && clk_hz < 3350000) return "TS1000";
	if (clk_hz > 3400000 && clk_hz < 3600000) return "TS2068";
	return "unknown";
}

uint32_t board_suggested_sp0256_clock(void)
{
	if (clk_hz > 3150000 && clk_hz < 3350000) return clk_hz;   /* J2: CPU clock */
	return DEFAULT_CLOCK_HZ;                                   /* crystal      */
}
