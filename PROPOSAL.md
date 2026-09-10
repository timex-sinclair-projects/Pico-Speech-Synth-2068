# ZX Voice Replica on RP2040 — Design Proposal

*Prepared 2026-09-10, revised the same day after decisions on I2S audio, ROM distribution, TS1000 footprint, dual TTS engines, SMD assembly and port decode. Builds on the Pico-Speech-Synth-2068 repo (RP2040-Zero + 74LVC245 + 74HC138 + 2N3904 PCB, Aug 2023) and the Woodroffe-derived C code in this folder.*

## Status, end of 10 September 2026

| Phase | State | Where |
|---|---|---|
| 0 Desktop port of the SP0256 core | Done. ROM path proven; pauses within 4 ms of Table 6; pitch matches real-chip recordings. Speed, pitch and clock controls. | `core/`, `harness/`, `rom/` |
| 1 Pico firmware | Written and compiled, not yet run on hardware. | `firmware/` v0.3 |
| 2 Text engines, BASIC, TS1000 driver | Done on the desktop: NRL and CTS256 engines, OUT 55, 2068 demo, TS1000 REM-line driver (`zxvoice1000.p`). | `tts/`, `basic/` |
| 3 Rev B hardware | Designed, PCB not yet drawn: design page, pin-level netlist for EasyEDA Pro, Rev B firmware build (I2S 32 kHz, status byte, clock sense, board ID). | `hardware/revb/`, `docs/rev-b-hardware.html` |

All merged to `main`. Not yet done anywhere: running the firmware on a real card. Next inputs: hardware results on the 2068 and TS1000, then the Rev B schematic in EasyEDA.

## 1. Where things stand

**What the ZX Voice actually is.** Wilf Rigter's ZVOICE / ZX Voice (ZX-Appeal, Oct 1986; CATS Jan 1987) was a Vancouver Sinclair Users Group club kit (~$25), not a commercial product. It kept the I/O map of Rigter's earlier 8255-based ZSPEAK so existing software ran unchanged. It works on the TS1000/1500/ZX81 and the TS2068 from one PCB.

**Port map, confirmed from the schematic and Rigter's listing.** The 74HC138 decodes only A4, A5, A7 plus /RD, /IORQ, /M1:

| Function | Decode | Ports that match | Rigter used |
|---|---|---|---|
| Load allophone (ALD) | OUT, A7=0 A5=0 A4=1 | 0x10–0x1F, 0x50–0x5F | `OUT (23)` (0x17); BASIC `OUT 17,n` |
| Status on D7 | IN, A7=0 A5=1 A4=0 | 0x20–0x2F, 0x60–0x6F | `IN A,(39)` (0x27); BASIC `IN 37` |

Data is 6-bit (0–63) on D0–D5. D7 = 1 means ready, D7 = 0 means busy (LRQ high → 2N3904 on → D7 pulled low, only while the status port is being read). The 2.2k pull-up on D7 was 2068-only.

**State of the existing work.**

- The Pico-Speech-Synth-2068 firmware is MicroPython. It polls the ALD pin in a Python loop, but the ALD strobe is only as long as the Z80 I/O cycle, roughly 700 ns at 3.25–3.5 MHz, and the data bus changes a few hundred ns after it. A Python loop iterates every 5–20 µs, so writes are captured by luck (bus capacitance), not by design. There is no input latch, and there is no one-deep input buffer, so a write that arrives while an allophone is playing is dropped rather than queued as the real chip does.
- Playback is sample-based (CPC Wiki WAVs, 11025 Hz) with bit-banged timing. The `allophones.c` in this folder has WAV-header bytes left in 26 of the 64 arrays (the converter stripped a fixed header length; e.g. `a07eh`, `a19iy`, `a63bb2` begin with the literal `data` chunk tag, `a33dd2` begins with a `fmt` chunk). The `allophonesizeCorrected[]` table exists because the samples don't match the chip's real allophone durations.
- The PCB is sound as a baseline: level shifting through a 74LVC245 (5 V-tolerant inputs even when unpowered, which covers the power-up window where the Z80 bus is live before the Pico's regulator is up), original decode logic, original filter topology. It shifts only D0–D5 and ALD, so it cannot carry an 8-bit ASCII byte, and /RESET is not wired.

## 2. Recommendation in one paragraph

Keep the board architecture (RP2040-Zero, 74LVC245, '138 decoder, original filter) but rewrite the firmware in C with the Pico SDK: PIO captures each OUT cycle on the falling edge of the strobe, and core 1 runs a port of MAME's `sp0256.cpp` (BSD-3-Clause) against the 2 KB SP0256-AL2 ROM image, so the replica reproduces the chip's LPC synthesis, timing, LRQ/SBY behaviour and all 64 entries exactly instead of playing recordings. Add one extra write port (OUT 55) for ASCII text and control bytes, and a public-domain NRL-rules text-to-allophone engine on core 0. Rev B of the PCB shifts all eight data bits through the 74LVC245, adds a second '245 for the decoder strobes, /RESET and /CLK, tightens the port decode with a 74HCT688 comparator, and moves audio to a MAX98357A I2S amplifier. All parts are SMD for factory assembly, with the RP2040 placed directly on the board.

## 3. Hardware

### 3.1 Board choice

| Board | Flash | GPIO | 5 V tolerance | Verdict |
|---|---|---|---|---|
| RP2040 on the board | 2–4 MB (W25Q16/32) | 30 | none | **Recommend for factory assembly.** RP2040 + flash + 12 MHz crystal + 3.3 V LDO + USB-C is the published minimal design; the assembler stocks every part, and there is no module to hand-solder. |
| Waveshare RP2040-Zero | 2 MB | 20 on the edge (+9 on back pads) | none | Keep as the hand-built option; same firmware, same pin map. |
| Raspberry Pi Pico | 2 MB | 26 | none | Only needed if you move port decode into PIO (see 3.4). |
| Pico 2 (RP2350) | 4 MB | 26 | GPIO0–25 tolerate 5.5 V only while IOVDD is at 3.3 V | Not worth it: the Z80 drives the bus ~0.5 ms before the Pico regulator is up, so level shifters stay. |

The LPC core needs ~2 KB ROM, a few KB RAM and well under 5 % of one core, so the RP2040-Zero has ample headroom, including for the 600 KB sample set if you want to keep a "recorded" voice as an option.

### 3.2 Rev B schematic changes (from the 2023-08-31 schematic)

1. **74LVC245: shift D0–D7**, not D0–D5 + ALD. The text port needs the full byte.
2. **Second 74LVC245 for the strobes.** Keep the SN74HC138 running at 5 V exactly as on the original, and pass its outputs (/ALD, /TXT, and the read-enable if the firmware wants to see it) plus /RESET and /CLK through a second '245 in A→B. Same part you already trust for the bus, one footprint, and every 5 V signal reaches the Pico through the same 5 V-tolerant path. (Alternative: a 74LVC138A at 3.3 V would output Pico-level strobes directly and save the second buffer, but it is SMD-only and a different part to stock.)
3. **Use three decoder outputs:** Y5 → /ALD (OUT 23), Y2 → status read enable (IN 39), Y7 → /TXT (OUT 55, new text/command port). Y3 (IN 55) is reserved for a future byte-wide readback via a second '245 in B→A direction.
4. **D7 status driver:** either keep the 2N3904 (base from the Pico's LRQ pin through 10k, emitter to Y2) or use a 74LVC1G125 with OE from Y2 and data from the LRQ pin. The '1G125 actively drives D7 high when ready, which removes the dependence on the 2068 pull-up and works identically on the TS1000. Recommend the '1G125.
5. **/RESET** (24A) to a GPIO through the second '245, so the emulator clears its buffers like SBY RESET.
6. **/CLK** (9B) to a GPIO through the second '245 (optional). The original ran the SP0256 from the CPU clock on the TS1000 (3.25 MHz, so 4 % fast) and from a 3.12 MHz crystal on the 2068. Measuring CLK lets the firmware reproduce that pitch difference automatically.
7. **Audio: MAX98357A I2S amplifier** (TQFN-16, factory-stocked) driving a 4–8 Ω speaker from the 5 V rail through a 2-pin JST-PH header. Three GPIOs (BCLK, LRCLK, DIN), volume set in firmware (control byte on OUT 55) with the GAIN pin strapped for 9 dB. This replaces the filter, pot and external amplifier. Keep the PWM → RC → 3.5 mm jack as a line-out for headphones or a recorder (two resistors, two capacitors); the firmware mirrors the same sample stream to both. The RC corner moves to 3.3k/22n (2.2 kHz); a solder jumper selects the original 33k "vintage" value.
8. SBY → an LED (a nice "talking" indicator, as on the original SP0256 boards).
9. **All SMD:** 74LVC245 ×2 (TSSOP-20), 74HCT138 (SOIC-16), 74HCT688 (SOIC-20), 74LVC1G125 (SOT-23-5), MAX98357A (TQFN-16), RP2040 (QFN-56), W25Q32 (SOIC-8), 12 MHz crystal, AP2112K-3.3 LDO, USB-C receptacle, BOOTSEL button, 0603 passives. Use **HCT**, not HC, for the two 5 V logic parts: the 2068's rear-connector signals come through 74LS buffers whose guaranteed high is 2.7 V, below the 3.5 V an HC input needs at 5 V. HCT inputs switch at TTL levels.

### 3.3 Pin map (RP2040-Zero)

| GPIO | Signal | Direction | Via |
|---|---|---|---|
| GP0–GP7 | D0–D7 | in | 74LVC245 |
| GP8 | /ALD (OUT 23 strobe) | in | 74HC138 Y5 via '245 #2 |
| GP9 | /TXT (OUT 55 strobe) | in | 74HC138 Y7 via '245 #2 |
| GP10 | /RESET | in | '245 #2 |
| GP11 | PWM line-out | out | RC filter → 3.5 mm jack |
| GP12 | /LRQ | out | '1G125 / 2N3904 → D7 |
| GP13 | SBY | out | LED |
| GP14 | CLK sense (optional) | in | '245 #2 |
| GP15, GP26, GP27 | I2S BCLK / LRCLK / DIN | out | MAX98357A → speaker |
| GP28, GP29 | spare (config jumpers) | in | — |

### 3.4 Alternative considered: decode in PIO, no '138

Feeding A4, A5, A7, /IORQ, /RD, /WR, /M1 through a second '245 and matching them in PIO makes the ports fully software-defined. It costs a second level shifter, seven more GPIOs (RP2040-Zero is then at its limit; a Pico is comfortable), and the read cycle becomes software-timed (PIO must enable the D7 driver within ~500 ns, which is feasible). The '138 keeps reads zero-latency in hardware and keeps the replica electrically faithful, so I recommend the '138 unless you want configurable port numbers.

### 3.5 Connector

Rev B uses the **TS1000 edge footprint**, which is a subset of the 2068 rear connector (the 2068 pinout is the ZX81 pinout shifted three positions: 2068 pin 4A = ZX81 D7, 7A = D0, 18A = /IORQ, 25A = /M1, 4B = +5 V). The keyway slot guarantees correct insertion on both machines, which is how Rigter's single board served both.

### 3.6 Port decode: add a 74HCT688

The '138 alone looks at three address bits, so each function answers to 32 port numbers (writes 0x10–0x1F and 0x50–0x5F, reads 0x20–0x2F and 0x60–0x6F). Nothing in Timex's own map lives there (the SCLD, AY and bank ports are all 0xF4–0xFF), and the ZX81 ULA only answers even ports, but any third-party device that also decodes loosely in the A7 = 0 half can collide. A comparator closes that:

- **74HCT688**, P inputs from A0–A3, A6, A7 (the two spare P/Q pairs tied together), Q inputs from six solder jumpers, /G from /IORQ. Its /P=Q output feeds the '138's G2A in place of raw A7.
- **Default jumper pattern** A0–A3 = 0111, A6 = A7 = 0, so the four decoded ports are exactly 0x07 (unused), **0x17 = OUT 23** (allophone), **0x27 = IN 39** (status), **0x37 = OUT 55 / IN 55** (text, future readback). A4/A5 keep selecting the function, so Rigter's machine-code driver (23/39) works unchanged.
- **Compatibility jumper ("loose decode")** routes A7 straight to G2A as the original did. Needed only for software that used the other port numbers in the class, such as the BASIC examples with `OUT 17` / `IN 37` (0x11 / 0x25), which do not match the strict compare.
- Propagation through HCT688 + HCT138 is under 60 ns against a ~700 ns I/O cycle, so timing is unaffected.

Alternative: one ATF16V8 PLD (SOIC-20) replaces both the '688 and the '138, decodes any map, and could add /WAIT, but it needs programming before assembly. The '688 + '138 pair needs no firmware on the 5 V side, so I recommend it.

## 4. Firmware

### 4.1 Architecture (C, Pico SDK)

```
 Z80 bus ──'245──► GP0-7 ─┐
          '138 Y5 ► GP8 ──┤ PIO SM0: wait 0 pin ALD; in pins,8; push   ─► RX FIFO ─► IRQ ─► allophone latch (1 deep)
          '138 Y7 ► GP9 ──┘ PIO SM1: wait 0 pin TXT; in pins,8; push   ─► RX FIFO ─► IRQ ─► text/command ring buffer

 core 0: command parser, text-to-allophone engine, USB-CDC console, config
 core 1: SP0256 LPC core (10 kHz) ─► sample ring ─► DMA ─► PWM (100 kHz carrier, ×10 hold) or I2S
```

- **Capture.** The Z80 puts data on the bus in T1 and the strobe falls at T2, so sampling D0–D7 on the strobe's falling edge (PIO latency 2 SM cycles, 16 ns) is always valid. No latch chip needed.
- **Chip model.** Port MAME `sp0256.cpp` (Zbiciak/Lindner, BSD-3-Clause): 16-opcode microsequencer, 2 KB ROM, 12-pole lattice as six second-order sections, sample rate = clock/312 (10.000 kHz at 3.12 MHz). Keep its interface semantics: one-deep input buffer, LRQ high while the buffer is full, SBY high when idle. A write while the buffer is full is ignored, exactly as the real chip.
- **ROM.** `sp0256-al2.bin`, 2048 bytes, CRC32 b504ac15. Joe Zbiciak holds a 2007 Microchip letter permitting distribution with copyright acknowledgement; ship it with that notice.
- **Audio.** Core 1 writes 10 kHz samples; DMA feeds PWM at 100 kHz with each sample held ×10 (the chip's own output was a per-sample PWM, so this is faithful). Optional DSP "vintage filter" models the 33k/22n RC pair for people using the I2S path.
- **Console.** Keep the existing SPEAK / LIST / STATUS / DEBUG command set over USB-CDC and add SAY "text", so the board is testable and usable from a PC as a USB speech synthesizer.
- **Config in flash:** pitch/clock, filter mode, TTS variant, exception lexicon.

### 4.2 Text-to-speech

Ship **both engines** and select with a control byte on OUT 55 (0xD0 = NRL, 0xD1 = CTS256); the choice persists in flash as the default.

1. **NRL letter-to-sound rules via John Wasser's 1985 "English to Phoneme" (public domain).** ~330 rules, number and abbreviation handling, unknown-word fallback. Output is a phoneme string; a mapper applies the datasheet's Table 5 positional rules (KK1/KK2/KK3, GG1–3, DD1/DD2, BB1/BB2, TT1/TT2, NN1/NN2, HH1/HH2, DH1/DH2, YY1/YY2, ER1/ER2, UW1/UW2; PA3 before voiceless stops, PA2 before voiced; doubled stressed short vowels; PA4/PA5 at punctuation). Flash cost ~30 KB, RAM a few hundred bytes. Plus a small editable exception lexicon (Rigter's word table and the datasheet dictionary are good seeds).
2. **CTS256A-AL2 rules** (GmEsoft's extraction, GPL-3.0, compatible with the repo licence). This gives the "GI reference" voice as the Radio Shack text-to-speech board sounded. The rules derive from a GI/Microchip ROM with no distribution grant, so note that in the README alongside the SP0256 ROM notice.
3. A word lexicon (Rigter's Part 2 approach) serves both engines as the shared exception table.

Expect CTS256-grade output: intelligible, robotic, no prosody. Pauses at punctuation and vowel doubling do most of the work for naturalness.

### 4.3 Bus protocol

| Access | Meaning |
|---|---|
| `OUT 23, n` (0–63) | Allophone, exactly as ZX Voice. Bit 7 set = ignored (Rigter's terminator convention). |
| `IN 39` bit 7 | 1 = ready for another byte, 0 = busy. Same in both modes. |
| `OUT 55, c` (0x20–0x7E) | Append ASCII to the text buffer (256 bytes). Speaks at `.` `!` `?` or CR, or when full. |
| `OUT 55, 0x00` / CR | Flush: speak what is buffered. |
| `OUT 55, 0x80–0xFF` | Control: 0x80 stop, 0x81 flush, 0x90+n pitch (16 steps, 50–200 %), 0xA0+n speed (16 steps, 50–200 %), 0xB0+n clock (16 steps, 2.5–4.0 MHz: pitch and speed together, as a crystal change; 0xB4 = 3.12 MHz, 0xB5 = 3.25 MHz TS1000), 0xC0 allophone-echo (text engine emits codes to the console for learning), 0xC8+n filter, **0xD0 NRL engine, 0xD1 CTS256 engine**, 0xE0+n volume (I2S amp), 0xFF reset. |

Text and allophones can be mixed; both feed the same allophone queue. In text mode D7 tells BASIC whether the buffer has room, so a 2068 program can be as simple as `FOR i=1 TO LEN a$: OUT 55,CODE a$(i): NEXT i: OUT 55,13`. TS1000 BASIC has no OUT/IN, so ship updated versions of Rigter's REM-line machine-code driver (allophone string sender and text sender).

## 5. Plan

| Phase | Deliverable | Hardware |
|---|---|---|
| 0 | Port `sp0256.cpp` to a desktop test harness; render all 64 entries to WAV and compare durations to Table 6. Prove the ROM path. **Done 2026-09-10** (`core/`, `harness/`). | none |
| 1 | Pico SDK firmware: PIO capture, LPC core, PWM DMA, LRQ/SBY, USB console. Allophone mode working on the 2068. **Written and compiled 2026-09-10** (`firmware/`), awaiting hardware test. | **existing PCB** (D0–D5 is enough for this phase) |
| 2 | Both TTS engines (desktop first, then Pico), OUT 55 text port and engine switch, exception lexicon, 2068 BASIC demos, TS1000 ML driver. **Done 2026-09-10**: engines and port (`tts/`), 2068 demo and TS1000 REM-line driver with .P file (`basic/`). | Rev B PCB (D6/D7, second '245, '688 decode, /RESET, I2S amp, TS1000 footprint) |
| 3 | CLK-tracking pitch, IN 55 byte readback, I2S amplifier, documentation and timexsinclair.com write-up. **Firmware side done 2026-09-10 (v0.3); EasyEDA schematic/PCB and write-up remain.** | Rev B |

Phase 1 needs no new hardware, so the firmware rewrite can start immediately and be validated on the board you already have.

## 6. Risks and open questions

- **ROM licensing.** Accepted: distribution of `sp0256-al2.bin` under Zbiciak's Microchip letter, with the notice shipped. The CTS256 rule set has no equivalent letter; document that.
- **Strict decode vs old examples.** With the '688 in strict mode, only 0x17/0x27/0x37 answer. Published BASIC examples using `OUT 17` / `IN 37` need the loose-decode jumper or a one-line edit. Document both port sets.
- **Bare RP2040 layout.** Placing the chip directly adds the USB, crystal and flash layout to the PCB work. The RP2040-Zero module remains the fallback with the same firmware.
- **Vsync on the ZX81.** Any OUT ends the vertical sync pulse on a ZX81, so bulk text sends will flicker in SLOW mode, as they did with the original. The ML driver can send in FAST mode or between frames.

## Sources

- ZVOICE article index: https://www.timexsinclair.com/article/zvoice/ ; ZX-Appeal Oct 86 scan: https://archive.org/details/zx-appeal/ZX-Appeal%20Oct%2086/page/n9 ; CATS Jan 87 Part 2: https://archive.org/details/cats-newsletter/CATS%20v4%20n10%20Jan%201987/page/n6
- Existing repo: https://github.com/timex-sinclair-projects/Pico-Speech-Synth-2068
- MAME SP0256 core (BSD-3): https://github.com/mamedev/mame/blob/master/src/devices/sound/sp0256.cpp ; ROM notice: http://spatula-city.org/~im14u2c/sp0256-al2/
- GmEsoft SP0256/CTS256 emulator (GPL-3): https://github.com/GmEsoft/SP0256_CTS256A-AL2 ; CTS256 rules: https://github.com/GmEsoft/CTS256A-AL2
- Wasser "English to Phoneme" (public domain): https://www.cs.cmu.edu/Groups/AI/areas/speech/systems/eng2phon/0.html ; NRL Report 7948: https://apps.dtic.mil/sti/pdfs/ADA021929.pdf
- Woodroffe Pico SP0256 on Z80 bus (PIO example, CTS256 port): https://github.com/ExtremeElectronics/Pico-SP0256-AL2-Em-for-Rc2014-SD-CARD-Module
- Derek Fountain on Pico/5 V bus power-up: https://www.derekfountain.org/zx_pico_5v.php
- RP2350 5 V tolerance thread: https://forums.raspberrypi.com/viewtopic.php?t=375118
