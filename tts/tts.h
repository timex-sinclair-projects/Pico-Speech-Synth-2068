/*
 * tts.h -- English text to SP0256-AL2 allophones.
 *
 * Two engines share one front end:
 *   TTS_NRL     NRL letter-to-sound rules (Wasser, public domain) producing
 *               phonemes, mapped to allophones with the datasheet's rules.
 *               Numbers are spoken as cardinals/ordinals, symbols by name.
 *   TTS_CTS256  the rule set of GI's CTS256A-AL2 text-to-speech chip, which
 *               produces allophones directly.  Digits are spoken singly and
 *               punctuation follows the chip's own table.
 *
 * No allocation.  Allophones are delivered through a callback as they are
 * produced; every utterance ends with a pause, as the SP0256 requires.
 */
#pragma once
#include <stdint.h>

typedef enum { TTS_NRL = 0, TTS_CTS256 = 1 } tts_engine_t;

typedef void (*tts_emit_fn)(void *ctx, uint8_t allophone);

void tts_speak(tts_engine_t engine, const char *text, tts_emit_fn emit, void *ctx);

const char *tts_engine_name(tts_engine_t engine);
