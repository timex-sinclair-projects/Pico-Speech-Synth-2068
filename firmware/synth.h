/* synth.h -- the emulated SP0256 and its host-side queues (runs on core 1). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

void synth_start(void);                 /* launches core 1 */

/* Bus side (called from the PIO IRQ on core 0).  Mirrors the chip's 1-deep
 * input buffer: returns false and drops the byte when busy, as the chip. */
bool synth_bus_ald(uint8_t allophone);

/* Console / text-engine side: queued, fed to the chip when it is ready. */
bool synth_queue(uint8_t allophone);    /* false if the queue is full */
void synth_queue_flush(void);           /* discard queued allophones  */
int  synth_queue_count(void);

void synth_request_reset(void);         /* from the bus /RESET or console */
void synth_stop(void);                  /* flush queue and silence      */

/* Controls. */
void     synth_set_clock(uint32_t hz);  /* 2.5..4.0 MHz                 */
uint32_t synth_get_clock(void);
void     synth_set_speed(int pct);      /* 50..200                      */
void     synth_set_pitch(int pct);      /* 50..200                      */
int      synth_get_speed(void);
int      synth_get_pitch(void);

/* Bits 0..6 of the IN 55 status byte (Rev B); bit 7 and bit 5 are set here. */
void     synth_set_status_bits(uint8_t bits);

/* Status for the console. */
bool     synth_busy(void);              /* /LRQ level                   */
bool     synth_idle(void);              /* SBY level                    */
uint32_t synth_played(void);            /* allophones started           */
