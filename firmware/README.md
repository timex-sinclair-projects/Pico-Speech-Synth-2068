# ZX Voice replica firmware (Phases 1 and 2)

RP2040 firmware that makes the 2023 "ZX Voice with Pico" board (RP2040-Zero,
74LVC245, 74HC138, 2N3904) behave as an SP0256-AL2 on Wilf Rigter's ZX Voice
port map, using ROM emulation instead of recordings.

```
core 0   USB console · PIO capture of OUT 23 / OUT 55 · text and control bytes
core 1   SP0256 core (../core/sp0256.c) · PWM audio through DMA · /LRQ and SBY
```

## Build and flash

```sh
./build.sh                 # -> build/zxvoice.uf2
```

Needs cmake, python3, the ARM GNU toolchain in `~/pico/arm-gnu-toolchain-*`
and the Pico SDK in `~/pico/pico-sdk` (or set `PICO_SDK_PATH`). Hold BOOTSEL
while plugging the board in, then copy `zxvoice.uf2` onto the `RPI-RP2`
drive. The board reboots into the firmware and appears as a USB serial port.

## Pins (config.h)

| GPIO | Signal | Notes |
|---|---|---|
| GP0–GP7 | D0–D7 | 2023 board wires D0–D5; GP6/GP7 pulled down |
| GP8 | /ALD | OUT 23 strobe, 74HC138 Y5 through the '245 |
| GP9 | /TXT | OUT 55 strobe (Rev B; pulled up until then) |
| GP10 | /RESET | bus reset (Rev B; pulled up until then) |
| GP11 | PWM | audio, 100 kHz carrier, into the RC filter |
| GP13 | /LRQ | 1 = busy; drives the 2N3904 that pulls D7 low on IN 39 |
| GP14 | SBY | 1 = idle |

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

Core 1 renders one 10 kHz sample at a time and writes it ten times into a
DMA buffer; two chained DMA channels stream the buffer into the PWM
compare register at 100 kHz, paced by the PWM itself. Latency is one
64-sample block, 6.4 ms. The "clock" control changes only the PWM period,
so pitch and speed move together as with a different crystal. Speed and
pitch controls are applied inside the core (see `../harness/README.md`).

## Console

Connect at any baud rate. `HELP` lists the commands: `SPEAK HH1 EH LL AX OW
PA5`, `SAY hello world`, `ENGINE NRL|CTS`, `HELLO`, `LIST`, `STATUS`, `STOP`,
`RESET`, `SPEED 50..200`, `PITCH 50..200`, `CLOCK 2500000..4000000`. Console
speech is queued behind anything the Z80 sends, so it never disturbs a
running program.

## Not yet done

- I2S output for the Rev B MAX98357A (the DMA/PWM path is isolated in
  `audio.c`, so this is a second back end, not a rewrite).
- Untested on hardware as of this commit: the code has been compiled, and
  the core is the one validated in the desktop harness, but the PIO capture,
  DMA audio and pin behaviour need a board and a scope.
