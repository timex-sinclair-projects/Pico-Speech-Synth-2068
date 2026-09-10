/* board.h -- board identification and bus clock measurement (Rev B). */
#pragma once
#include <stdint.h>

void     board_init(void);
int      board_id(void);            /* 0..3 from the jumpers, -1 if not fitted */
uint32_t board_bus_clock_hz(void);  /* measured bus CLK, 0 if not fitted/seen  */
const char *board_host_name(void);  /* "TS1000", "TS2068", "unknown"           */

/* The SP0256 clock the original ZX Voice ran at on this host: the CPU clock
 * on a TS1000 (3.25 MHz, jumper J2), a 3.12 MHz crystal on the 2068. */
uint32_t board_suggested_sp0256_clock(void);
