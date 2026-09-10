/*
 * config.h -- board selection, pin maps and tunables.
 *
 * Build with -DZXV_BOARD=BOARD_ZERO2023 (default) or -DZXV_BOARD=BOARD_REVB.
 * build.sh produces one UF2 per board.
 *
 *   BOARD_ZERO2023  the 2023-08-31 card: RP2040-Zero, one 74LVC245 (D0-D5
 *                   and /ALD), 74HC138, 2N3904 on D7, PWM line-out.
 *   BOARD_REVB      hardware/revb: RP2040 on the board, three 74LVC245s,
 *                   74HCT688 + 74HCT138, 74LVC1G125 on D7, MAX98357A I2S
 *                   amplifier, status byte on IN 55, bus clock sense,
 *                   board-ID jumpers.
 */
#pragma once

#define BOARD_ZERO2023 1
#define BOARD_REVB     2

#ifndef ZXV_BOARD
#define ZXV_BOARD BOARD_ZERO2023
#endif

/* ---- pins common to both boards --------------------------------------- */
#define PIN_D0            0   /* D0..D7 on GP0..GP7 (2023 board: D0..D5)     */
#define PIN_ALD           8   /* /ALD strobe, OUT 23                          */
#define PIN_TXT           9   /* /TXT strobe, OUT 55 (unwired on 2023 board)  */
#define PIN_RESET        10   /* /RESET from the bus (unwired on 2023 board)  */
#define PIN_PWM          11   /* PWM audio -> RC filter -> jack               */
#define PIN_READY        13   /* D7 status driver, polarity per board         */
#define PIN_SBY          14   /* SBY: 1 = idle (LED)                          */

#if ZXV_BOARD == BOARD_REVB
#define BOARD_NAME        "Rev B"
#define READY_ACTIVE_HIGH 1   /* GP13 -> 74LVC1G125 A: 1 = ready              */
#define PIN_CLK          12   /* bus CLK through U_S                          */
#define PIN_RDSTAT       15   /* /RDSTAT sense (Y2)                           */
#define PIN_I2S_DIN      16
#define PIN_I2S_BCLK     17   /* LRCLK on 18 (side-set pair)                  */
#define PIN_AMP_EN       19   /* MAX98357A SD_MODE: 1 = on, left channel      */
#define PIN_STATUS0      20   /* GP20..GP27 -> U_R -> D0..D7 on IN 55         */
#define PIN_ID0          28   /* board-ID jumpers to GND, internal pull-ups   */
#define PIN_ID1          29
#define HAS_I2S           1
#define HAS_STATUS_BYTE   1
#define HAS_CLK_SENSE     1
#define HAS_RDSTAT        1
#define HAS_BOARD_ID      1
#define HAS_AMP_EN        1
#define BOARD_ID_EXPECTED 1   /* JP9 closed, JP10 open                        */
#else
#define BOARD_NAME        "2023 RP2040-Zero"
#define READY_ACTIVE_HIGH 0   /* GP13 -> 2N3904 base: 1 = busy (/LRQ)         */
#define HAS_I2S           0
#define HAS_STATUS_BYTE   0
#define HAS_CLK_SENSE     0
#define HAS_RDSTAT        0
#define HAS_BOARD_ID      0
#define HAS_AMP_EN        0
#endif

/* ---- tunables ---------------------------------------------------------- */
#define DEFAULT_CLOCK_HZ 3120000   /* SP0256 crystal; TS1000 drove 3.25 MHz  */
#define CLOCK_MIN_HZ     2500000
#define CLOCK_MAX_HZ     4000000

#define OVERSAMPLE       10        /* PWM periods per synth sample            */
#define AUDIO_BLOCK      64        /* synth samples per PWM DMA half-buffer   */
#define I2S_RATE         32000     /* MAX98357A accepts 8/16/32/44.1/48 kHz   */
#define I2S_BLOCK        256       /* frames per I2S DMA half-buffer (8 ms)   */
#define AMP_HOLD_MS      400       /* keep the amplifier on this long after   */
                                   /* the chip goes idle                      */

#define TEXT_BUFFER      512       /* OUT 55 text buffer, bytes               */
#define HOST_QUEUE       512       /* console SPEAK queue, allophones         */

#define FW_VERSION       "0.3-phase3"
