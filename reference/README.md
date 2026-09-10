# Reference material (not compiled)

- `sp0256.cpp`, `sp0256.h` — MAME's SP0256 emulation (BSD-3-Clause, Joseph Zbiciak and Tim Lindner), the basis of `../core/sp0256.c`.
- `wasser-eng2phon/` — John A. Wasser, "English to Phoneme Translation", final version 15 April 1985, public domain ("I make no copyright claims on it"). NRL letter-to-sound rules (NRL Report 7948, 1976) in `english.c`, number and spelling code in `saynum.c` / `spellwor.c`. Source: https://www.cs.cmu.edu/Groups/AI/areas/speech/systems/eng2phon/ . Converted to `../tts/rules_nrl.c` by `../tools/gen_rules.py`.
- `cts256/RULES.TXT` — the 432 text-to-allophone rules of the GI CTS256A-AL2, extracted from its ROM by Michel Bernard (GmEsoft), GPL-3.0, https://github.com/GmEsoft/CTS256A-AL2 . The ROM itself is General Instrument / Microchip copyright with no distribution grant; only the extracted rule text is used here. Converted to `../tts/rules_cts.c` by `../tools/gen_rules.py`.
