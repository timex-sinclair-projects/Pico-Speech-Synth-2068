/*
 * phon2allo.h -- Wasser/NRL phoneme string -> SP0256-AL2 allophones.
 *
 * Input is the phoneme notation of Wasser's english.c: two upper-case
 * letters per vowel or digraph consonant (IY IH EY EH AE AA AO OW UH UW ER
 * AX AH AY AW OY TH DH SH ZH NG CH WH), one lower-case letter per simple
 * consonant (p b t d k g f v s z h m n l w y r j), spaces between words.
 * Output applies the positional rules of the SP0256-AL2 datasheet, Table 5.
 */
#pragma once
#include <stdint.h>

typedef void (*allo_emit_fn)(void *ctx, uint8_t allophone);

/* Translate one word (no spaces).  Emits allophones only, no trailing pause.
 * after_pause = 1 if the caller has just emitted a pause, so that a word
 * starting with a stop does not add a second one. */
void phon2allo_word(const char *ph, int len, int after_pause, allo_emit_fn emit, void *ctx);
