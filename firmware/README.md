# ZX Voice replica firmware

RP2040 firmware that makes a ZX Voice card behave as an SP0256-AL2 on Wilf
Rigter's port map, using ROM emulation instead of recordings. One source
tree, two boards, selected at build time in `config.h`:

| `ZXV_BOARD` | Card | Output |
|---|---|---|
| `BOARD_ZERO2023` | the 2023-08-31 RP2040-Zero card (74LVC245, 74HC138, 2N3904, PWM line-out) | `build/zero2023/zxvoice-zero2023.uf2` |
| `BOARD_REVB` | `../hardware/revb`: RP2040 on the board, three '245s, '688 + '138, '1G125, MAX98357A | `build/revb/zxvoice-revb.uf2` |

```
core 0   USB console · PIO capture of OUT 23 / OUT 55 · text and control bytes
core 1   SP0256 core (../core/sp0256.c) · PWM audio through DMA · /LRQ and SBY
```

## Build and flash

```sh
./build.sh                 # both boards
BOARDS=revb ./build.sh     # one of them
```

Needs cmake, python3, the ARM GNU toolchain in `~/pico/arm-gnu-toolchain-*`
and the Pico SDK in `~/pico/pico-sdk` (or set `PICO_SDK_PATH`). Hold BOOTSEL
while plugging the board in, then copy `zxvoice.uf2` onto the `RPI-RP2`
drive. The board reboots into the firmware and appears as a USB serial port.

## Pins (config.h)

| GPIO | 2023 card | Rev B |
|---|---|---|
| GP0–GP7 | D0–D5 (GP6/7 pulled down) | D0–D7 |
| GP8 | /ALD, OUT 23 strobe | same |
| GP9 | unwired (pulled up) | /TXT, OUT 55 strobe |
| GP10 | unwired (pulled up) | /RESET |
| GP11 | PWM line-out | PWM line-out |
| GP12 | — | bus CLK sense |
| GP13 | /LRQ, 1 = busy, into the 2N3904 | READY, 1 = ready, into the 74LVC1G125 |
| GP14 | SBY LED | SBY LED |
| GP15 | — | /RDSTAT sense (counts IN 39) |
| GP16–18 | — | I2S DIN, BCLK, LRCLK to the MAX98357A |
| GP19 | — | amplifier enable (SD_MODE) |
| GP20–27 | — | status byte, driven onto D0–D7 by U_R during IN 55 |
| GP28, GP29 | — | board-ID jumpers (expect 1) |

The Rev B build uses `boards/zxvoice_revb.h` (bare RP2040, 4 MB W25Q32).

## How the bus is handled

- A PIO state machine spins on the strobe and samples D0–D7 within two PIO
  cycles of its falling edge, which is inside the Z80's data-valid window.
  No latch is needed. The byte lands in the RX FIFO and raises an interrupt
  on core 0.
- OUT 23 bytes go into a one-deep buffer that mirrors the chip's address
  latch. The interrupt handler drives /LRQ busy immediately, so a program
  that polls IN 39 right after the OUT sees busy, as it did on the real chip.
  A write that arrives while the buffer or the chip's own latch is full is
  dropped, as on the real chip. Core 1 hands the byte to the sequencer and
  releases /LRQ when the sequencer takes it, which is when the chip did.
- OUT 55 bytes: printable ASCII accumulates in a 256-byte buffer and is
  spoken through the selected text engine (`../tts/`) at `.` `!` `?` CR or
  NUL. Bytes with bit 7 set are controls: 0x80 stop, 0x81 flush, 0x90+n
  pitch, 0xA0+n speed, 0xB0+n clock, 0xC0 echo allophones to the console,
  0xD0 NRL engine, 0xD1 CTS256 engine, 0xFF reset.
- Bus /RESET resets the chip, as SBY RESET did.

## Audio

The back end pulls samples from the synth (`audio.h`), so two back ends
share one core:

- **PWM** (`audio_pwm.c`, both boards): each synth sample is held for ten
  PWM periods at 100 kHz, streamed by two chained DMA channels into the
  compare register. Latency one 64-sample block, 6.4 ms. The "clock"
  control changes the PWM period, so pitch and speed move together as with
  a different crystal.
- **I2S** (`audio_i2s.c`, Rev B, default there): the MAX98357A accepts
  LRCLK only at 8/16/32/44.1/48 kHz, so frames run at 32 kHz and the
  10 kHz stream is linearly interpolated through a Q16 phase accumulator;
  the "clock" control changes the step. 256-frame blocks, 8 ms. The
  amplifier is enabled through SD_MODE while talking and for 400 ms after,
  then muted. `OUTPUT SPEAKER|LINE` on the console switches back ends.

Speed and pitch controls are applied inside the core (see
`../harness/README.md`).

## Rev B extras

- **Status byte on IN 55**: bit 7 ready (as IN 39), bit 6 text buffer has
  room, bit 5 talking, bit 4 engine (1 = CTS256). Driven on GP20–27 and put
  on the bus by U_R while /RDEXT is low; no firmware timing involved.
- **Bus clock sense**: at boot a PIO state machine counts CLK edges for
  20 ms. 3.25 MHz means a TS1000, 3.5 MHz a 2068. On a TS1000 the original
  ZX Voice ran the SP0256 from the CPU clock (jumper J2), so the firmware
  sets the emulated clock to the measured value there and keeps the
  3.12 MHz crystal value on a 2068. `CLOCK` on the console overrides.
- **Board ID**: GP28/29 jumpers are read at boot; a mismatch with the
  compiled board prints a warning on the console.

## Console

Connect at any baud rate. `HELP` lists the commands: `SPEAK HH1 EH LL AX OW
PA5`, `SAY hello world`, `ENGINE NRL|CTS`, `HELLO`, `LIST`, `STATUS`, `STOP`,
`RESET`, `SPEED 50..200`, `PITCH 50..200`, `CLOCK 2500000..4000000`. Console
speech is queued behind anything the Z80 sends, so it never disturbs a
running program.

## Not yet done

- Untested on hardware as of this commit: the code has been compiled, and
  the core is the one validated in the desktop harness, but the PIO capture,
  DMA audio and pin behaviour need a board and a scope.
