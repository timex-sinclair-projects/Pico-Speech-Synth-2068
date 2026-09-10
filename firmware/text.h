/* text.h -- the OUT 55 text and control channel. */
#pragma once
#include <stdint.h>

void text_init(void);
void text_bus_byte(uint8_t v);     /* from the PIO IRQ */
void text_reset(void);
void text_poll(void);              /* core 0 main loop: run pending work */

void text_say(const char *line);        /* speak text now (console SAY) */
void text_set_engine(int engine);        /* 0 = NRL, 1 = CTS256 */
const char *text_engine_name(void);

/* Statistics for the console. */
uint32_t text_chars(void);
uint32_t text_controls(void);
int      text_pending(void);
