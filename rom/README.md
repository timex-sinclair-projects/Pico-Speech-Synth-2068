# SP0256-AL2 ROM image

| File | Bytes | CRC32 | Format |
|---|---|---|---|
| `sp0256-al2.bin` | 2048 | `b504ac15` | MAME bit order. **This is what the core and firmware use.** |
| `al2.bin` | 2048 | `df8de0b0` | Joe Zbiciak's original dump, every byte bit-reversed relative to the MAME image. The harness detects and converts it. |
| `al2.bit` | — | — | Zbiciak's bit-level listing of the same data. |
| `al2.sym` | — | — | Symbol file: entry-point addresses for all 64 allophones (`_@@PA1` at 0x1000, two bytes per entry). |
| `NOTICE.txt` | — | — | Text of the distribution page and the Microchip legal e-mail, captured 2026-09-10. |

Source: http://spatula-city.org/~im14u2c/sp0256-al2/

## Copyright and distribution

Microchip Technology Inc. holds the copyright to the SP0256-AL2 ROM image and the
intellectual property rights to the algorithms and data it contains. On 9 August
2007 Microchip's legal department granted Joe Zbiciak a non-exclusive right to
distribute this ROM data provided that Microchip's ownership is acknowledged
(see `NOTICE.txt`). This project redistributes the image under that grant and
carries the same acknowledgement:

> SP0256-AL2 ROM image copyright Microchip Technology Inc. Distributed with
> permission; Microchip retains the intellectual property rights to the
> algorithms and data it contains.

## Memory map

The image is mapped at byte address 0x1000 in the SP0256's 64 KB space. The
first 128 bytes are a jump table: entry *n* is a 2-byte JMP at 0x1000 + 2*n*,
which is why loading address *n* through ALD starts allophone *n*. Bit 7 and
bit 6 of the ALD value (A7, A8 on the chip) are tied low on the ZX Voice, so
only entries 0–63 are reachable, exactly as on the original board.
