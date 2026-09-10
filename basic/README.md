# BASIC side

## TS2068

`demo2068.bas` — TS2068 BASIC has OUT and IN, so no driver is needed. The
demo speaks "hello" with OUT 23 the original way (polling IN 39 for bit 7),
sends sentences to the text port with OUT 55, switches engines with the
0xD0/0xD1 control bytes, and shows the speed and pitch controls.

## TS1000 / ZX81

Sinclair BASIC on the ZX81 has no OUT or IN, so `zxvoice1000.asm` is a
258-byte machine-code driver that lives in a REM statement at line 1
(code at 16514) and reads the string variable A$ straight from the
variables area. Nothing has to be POKEd.

| Call | Does |
|---|---|
| `LET N=USR 16514` | speaks A$ as allophone numbers: OUT 23 for each byte, waiting on IN 39 bit 7 first; stops at the end of A$ or at a byte of 128 or more (Rigter's terminator). N = bytes sent |
| `LET N=USR 16517` | says A$ as text: ZX81 characters converted to ASCII, OUT 55 each, then a carriage return |
| `LET N=USR 16520` | sends the bytes of A$ unchanged to OUT 55, for control bytes: `CHR$ 209` CTS256 engine, `CHR$ 208` NRL, `CHR$ 128` stop, 144+n pitch, 160+n speed, 176+n clock |
| `POKE 16523,CODE "B"-32` | use B$ instead of A$ (any single letter) |

```
20 LET A$=CHR$ 27+CHR$ 7+CHR$ 45+CHR$ 15+CHR$ 53+CHR$ 4
30 LET N=USR 16514
40 LET A$="HELLO. I AM THE TIMEX SINCLAIR 1000."
50 LET N=USR 16517
```

The ready wait gives up after 65536 polls (about a second in FAST, a few
seconds in SLOW), so a missing board cannot hang the machine; N then
reports how many bytes went out. Inverse-video characters are sent as
their normal letters; tokens, graphics and NEWLINE become spaces.

Files:

- `zxvoice1000.p` — ready to LOAD: the driver in line 1 plus `demo1000.bas`
  as lines 10–170 (sends "hello" both ways, switches engines, then takes
  sentences from INPUT). Built with zmakebas in ZX81 mode.
- `loader1000.bas` — for typing in by hand: a REM line of 258 characters
  and a hex string; the loader POKEs the code, using the fact that
  `CODE c - 28` is the hex digit value for 0–9 and A–F in the ZX81
  character set. Delete lines 10–50 afterwards and SAVE.
- `zxvoice1000.asm` — source (sjasmplus). `make` rebuilds everything.
  The code contains no byte 118 (NEWLINE), which would end the REM line.
  Registers the ROM's SLOW-mode display relies on (IX, IY, AF' and the
  alternate set) are not touched.

Notes on the ZX81: any OUT ends the ULA's vertical-sync pulse, so a long
text send in SLOW mode may make the picture jitter for a frame; put FAST
before the call and SLOW after it if that matters. The driver was verified
in a Z80 simulator against a synthetic variables area holding numbers,
FOR variables, long-name numbers, arrays and other strings ahead of A$; it
has not yet run on a real TS1000.
