/*
 * config.h -- pin map and tunables for the ZX Voice replica firmware.
 *
 * Pin numbers match the "ZX Voice with Pico" PCB of 2023-08-31 (RP2040-Zero,
 * 74LVC245 + 74HC138 + 2N3904).  Rev B adds D6/D7, /TXT and /RESET; on the
 * 2023 board those GPIOs are unconnected and are held inactive by pulls.
 */
#pragma once

#define PIN_D0            0   /* D0..D7 on GP0..GP7 (2023 board: D0..D5)     */
#define PIN_ALD           8   /* /ALD strobe, OUT 23  (74HC138 Y5 via '245)  */
#define PIN_TXT           9   /* /TXT strobe, OUT 55  (Rev B)                */
#define PIN_RESET        10   /* /RESET from the bus  (Rev B)                */
#define PIN_PWM          11   /* PWM audio -> RC filter -> jack               */
#define PIN_LRQ          13   /* /LRQ: 1 = busy -> 2N3904 -> D7 low on IN 39 */
#define PIN_SBY          14   /* SBY: 1 = idle (LED)                          */

#define DEFAULT_CLOCK_HZ 3120000   /* SP0256 crystal; TS1000 drove 3.25 MHz  */
#define CLOCK_MIN_HZ     2500000
#define CLOCK_MAX_HZ     4000000

#define OVERSAMPLE       10        /* PWM periods per synth sample            */
#define AUDIO_BLOCK      64        /* synth samples per DMA half-buffer       */

#define TEXT_BUFFER      512       /* OUT 55 text buffer, bytes               */
#define HOST_QUEUE       512       /* console SPEAK queue, allophones         */

#define FW_VERSION       "0.2-phase2"
