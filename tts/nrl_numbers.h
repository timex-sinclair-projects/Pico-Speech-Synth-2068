/* nrl_numbers.h -- numbers and spelled characters as Wasser phoneme strings. */
#pragma once

typedef struct {
	char *buf;
	int   len, cap;
} phbuf_t;

void nrl_append(phbuf_t *b, const char *s);
void nrl_say_ascii(phbuf_t *b, int c);          /* name of a printable character */
void nrl_say_cardinal(phbuf_t *b, long value);
void nrl_say_ordinal(phbuf_t *b, long value);
