/* bus.h -- Z80 I/O bus capture (PIO) and bus-side reset. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void bus_init(void);

/* Statistics for the console. */
typedef struct {
	uint32_t ald_writes;      /* bytes captured on OUT 23              */
	uint32_t ald_dropped;     /* dropped because the chip was busy     */
	uint32_t txt_writes;      /* bytes captured on OUT 55              */
	uint32_t resets;          /* bus /RESET pulses seen                */
} bus_stats_t;

extern volatile bus_stats_t bus_stats;
