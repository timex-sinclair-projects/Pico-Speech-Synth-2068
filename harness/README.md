# SP0256 core and desktop harness (Phase 0)

`../core/sp0256.[ch]` is a portable C99 port of MAME's `sp0256.cpp`
(BSD-3-Clause, Joseph Zbiciak and Tim Lindner). It has no allocation, no
floating point and no dependencies beyond `<stdint.h>`/`<string.h>`, so the
same file drops into the RP2040 firmware. The MAME device framework, save
states and the SPB640 FIFO path were removed. The MAME original is kept in
`../reference/` for comparison.

```
make                  # build ./harness
make durations        # all 64 entries: sequencer time vs datasheet Table 6
make wavs             # one WAV per entry in out/, each closed with PA1
make sampler          # all 64 entries in one WAV (sampler.wav)
make hello            # "hello world" as in the datasheet (hello.wav)
./harness -r ../rom/sp0256-al2.bin -s "TT2 EH SS TT2 PA5" -o test.wav
./harness -r ../rom/sp0256-al2.bin -c 3250000 -s "..."   # TS1000 CPU clock
```

Three playback controls are available; all default to the real chip:

| Control | Flag | Range | Effect |
|---|---|---|---|
| Clock | `-c HZ` | 2.5–4.0 MHz sensible (sample rate = clock/312) | Pitch and speed together, exactly like a different crystal. 3 250 000 reproduces the TS1000, which drove the chip from the CPU clock. |
| Speed | `-t PCT` | 50–200 | Speed only; frame repeat counts are scaled, pitch is untouched. |
| Pitch | `-p PCT` | 50–200 | Pitch only; the period register is scaled and repeat counts compensated, so durations stay within a few percent. Noise (unvoiced) frames are unaffected. |

Speed and pitch are integer scalings of the chip's own frame parameters, so
they cost nothing at run time and the output is still the chip's synthesis,
just with different numbers in its registers. The clock control lives outside
the core: the caller consumes samples at a different rate.

Sequences accept allophone names (`HH1`, `PA3`), the short aliases from
`allophoneDefs.h` (`HH`, `KK`, `TT` ...) or numbers 0–63. WAVs are mono 16-bit
at the chip's native rate, clock/312 (10 000 Hz at 3.12 MHz).

## API

```c
sp0256_t sp;
sp0256_init(&sp, rom, 2048);       /* MAME bit order */
sp0256_ald(&sp, 27);               /* returns 0 if the 1-deep buffer was full */
sp0256_lrq_pin(&sp);               /* 1 = buffer full, matches the /LRQ pin  */
sp0256_sby_pin(&sp);               /* 1 = sequencer idle                     */
sp0256_render(&sp, buf, n);        /* n samples, int16                        */
```

The interface semantics follow the chip: a write while the buffer is full is
dropped, LRQ clears as soon as the sequencer starts the buffered entry (so a
host can always queue one ahead), SBY rises when the sequencer halts.

## What Phase 0 established

- **ROM path works.** The image runs, entries decode, and the five pauses
  come out within 4 ms of Table 6 (PA1 7, PA2 26, PA3 46, PA4 98, PA5 202 ms).
  Two bit orders of the image exist; the harness detects either.
- **Pitch matches the real chip.** Autocorrelation of the CPC Wiki recordings
  (made from a real SP0256-AL2) and of the emulation both give 109 Hz for OY,
  AY, NN1 and MM. The AL2 data never uses the amplitude/pitch interpolation
  registers, so that path is untested by this ROM.
- **Table 6 is not the sequencer time.** Voiced and noise entries run about
  0.7× the Table 6 figure in the emulator (mean 47 ms short); a few (NN1, SH,
  GG2) run longer. The per-period amplitude envelopes of the recordings show
  why: the chip keeps sounding its final frame until the next command
  arrives (the datasheet says every utterance must be followed by a pause for
  this reason), and the recordings were captured for the Table 6 duration and
  then cut, leaving a click at the end and, for NN1, a truncated rising
  envelope. So the recordings cannot confirm or refute the sequencer timing,
  and the emulator's frame timing (repeat count × pitch period) agrees with
  jzintv, GmEsoft's port and the Speech256 FPGA. The harness therefore
  measures each entry to the point where a queued PA1 takes over, and every
  render is closed with a pause.
- **Cost.** One 10 kHz sample is six second-order sections in 16-bit integer
  arithmetic plus the excitation, well under 100 cycles; the RP2040 will spend
  under 1 % of a core on it.
