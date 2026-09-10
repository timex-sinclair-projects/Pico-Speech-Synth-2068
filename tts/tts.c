/*
 * tts.c -- text front end for both engines.
 */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "sp0256_names.h"
#include "rules.h"
#include "phon2allo.h"
#include "nrl_numbers.h"
#include "lexicon.h"
#include "tts.h"

#define MAXWORD 32           /* letters per word; longer words are split */
#define PHCAP   512          /* phoneme buffer for the NRL path           */

typedef struct {
	tts_engine_t engine;
	tts_emit_fn  emit;
	void        *ctx;
	int          last;       /* last allophone emitted, or -1            */
	char         phb[PHCAP]; /* NRL: pending phonemes, words end in ' '  */
	phbuf_t      ph;
} tts_t;

const char *tts_engine_name(tts_engine_t e) { return e == TTS_CTS256 ? "CTS256" : "NRL"; }

static void put(tts_t *t, int a)
{
	t->emit(t->ctx, (uint8_t)a);
	t->last = a;
}

static void put_list(tts_t *t, const uint8_t *a)
{
	for (; *a != 0xFF; a++) put(t, *a);
}

static void pause_if_needed(tts_t *t, int pause)
{
	if (t->last < 0 || t->last > AL_PA5) put(t, pause);
}

/* Emit allophone names "PA2 BB2 IY". */
static void put_names(tts_t *t, const char *s, int len)
{
	char tok[8] = {0};
	int n = 0, i;
	for (i = 0; i <= len; i++) {
		if (i == len || s[i] == ' ') {
			if (n) {
				tok[n] = 0;
				int v = sp0256_lookup(tok);
				if (v >= 0) put(t, v);
				n = 0;
			}
		} else if (n < 5) {
			tok[n++] = s[i];
		}
	}
}

/* ---------------------------------------------------------------------- */
/*  NRL path                                                              */
/* ---------------------------------------------------------------------- */
static void nrl_flush(tts_t *t)
{
	/* Words in the phoneme buffer are separated by spaces. */
	int i = 0, len = t->ph.len;
	while (i < len) {
		int j = i;
		while (j < len && t->phb[j] != ' ') j++;
		if (j > i) {
			pause_if_needed(t, AL_PA3);
			phon2allo_word(t->phb + i, j - i, 1, t->emit, t->ctx);
			t->last = AL_OY;              /* something non-pause was emitted */
		}
		i = j + 1;
	}
	t->ph.len = 0;
	t->phb[0] = 0;
}

static void nrl_word(tts_t *t, const char *w)   /* w = " WORD " */
{
	int index = 1;
	while (w[index] != ' ' && w[index]) {
		const char *out;
		int outlen;
		int next = rules_find(&rules_nrl, w, index, &out, &outlen);
		if (next < 0) { index++; continue; }
		if (outlen) {
			if (t->ph.len + outlen + 2 >= PHCAP) nrl_flush(t);
			memcpy(t->phb + t->ph.len, out, (size_t)outlen);
			t->ph.len += outlen;
			t->phb[t->ph.len] = 0;
		}
		index = next;
	}
	nrl_append(&t->ph, " ");
}

static const char *nrl_abbrev(const char *w)     /* w = " WORD " */
{
	if (!strcmp(w, " DR "))  return " DOCTOR ";
	if (!strcmp(w, " MR "))  return " MISTER ";
	if (!strcmp(w, " MRS ")) return " MISSUS ";
	if (!strcmp(w, " ST "))  return " STREET ";
	return NULL;
}

static int nrl_number(tts_t *t, const char *s)   /* returns chars consumed */
{
	long value = 0;
	int i = 0, lastdigit = 0;
	while (isdigit((unsigned char)s[i])) {
		if (value < 100000000L) value = value * 10 + (s[i] - '0');
		lastdigit = s[i];
		i++;
	}
	/* ordinals: 1st 2nd 3rd 4th ... */
	{
		char a = (char)toupper((unsigned char)s[i]), b = (char)toupper((unsigned char)s[i + 1]);
		int ord = 0;
		if (!isalnum((unsigned char)s[i + 2])) {
			if (lastdigit == '1' && a == 'S' && b == 'T') ord = 1;
			if (lastdigit == '2' && a == 'N' && b == 'D') ord = 1;
			if (lastdigit == '3' && a == 'R' && b == 'D') ord = 1;
			if (strchr("04567890", lastdigit) && a == 'T' && b == 'H') ord = 1;
		}
		if (ord) { nrl_say_ordinal(&t->ph, value); return i + 2; }
	}
	nrl_say_cardinal(&t->ph, value);
	if (s[i] == '.' && isdigit((unsigned char)s[i + 1])) {
		nrl_append(&t->ph, "pOYnt ");
		for (i++; isdigit((unsigned char)s[i]); i++) nrl_say_ascii(&t->ph, s[i]);
	}
	return i;
}

static int nrl_dollars(tts_t *t, const char *s)   /* s points at '$' */
{
	long value = 0;
	int i = 1;
	for (; isdigit((unsigned char)s[i]) || s[i] == ','; i++)
		if (s[i] != ',' && value < 100000000L) value = value * 10 + (s[i] - '0');
	nrl_say_cardinal(&t->ph, value);
	if (s[i] == '.' && isdigit((unsigned char)s[i + 1]) && isdigit((unsigned char)s[i + 2]) &&
	    !isdigit((unsigned char)s[i + 3])) {
		int cents = (s[i + 1] - '0') * 10 + (s[i + 2] - '0');
		nrl_append(&t->ph, value == 1 ? "dAAlER " : "dAAlERz ");
		if (cents) {
			nrl_append(&t->ph, "AEnd ");
			nrl_say_cardinal(&t->ph, cents);
			nrl_append(&t->ph, cents == 1 ? "sEHnt " : "sEHnts ");
		}
		return i + 3;
	}
	nrl_append(&t->ph, value == 1 ? "dAAlER " : "dAAlERz ");
	return i;
}

static void nrl_text(tts_t *t, const char *s)
{
	int i = 0;
	t->ph.buf = t->phb; t->ph.len = 0; t->ph.cap = PHCAP; t->phb[0] = 0;

	while (s[i]) {
		unsigned char c = (unsigned char)s[i];

		if (isalpha(c) || (c == '\'' && isalpha((unsigned char)s[i + 1]))) {
			char w[MAXWORD + 3];
			int n = 0;
			w[n++] = ' ';
			while ((isalpha((unsigned char)s[i]) || s[i] == '\'') && n < MAXWORD + 1)
				w[n++] = (char)toupper((unsigned char)s[i++]);
			w[n++] = ' '; w[n] = 0;

			if (n == 3) {                               /* single letter */
				nrl_say_ascii(&t->ph, w[1]);
				continue;
			}
			{
				const uint8_t *lx;
				char bare[MAXWORD + 1];
				memcpy(bare, w + 1, (size_t)(n - 2)); bare[n - 2] = 0;
				lx = lexicon_lookup(bare);
				if (lx) {
					nrl_flush(t);
					pause_if_needed(t, AL_PA3);
					put_list(t, lx);
					continue;
				}
			}
			if (s[i] == '.') {
				const char *ab = nrl_abbrev(w);
				if (ab) { nrl_word(t, ab); i++; continue; }
			}
			if (isdigit((unsigned char)s[i])) {         /* AB12: spell it */
				int k;
				for (k = 1; k < n - 1; k++) nrl_say_ascii(&t->ph, w[k]);
				continue;
			}
			nrl_word(t, w);
			continue;
		}

		if (c == '$' && isdigit((unsigned char)s[i + 1])) { i += nrl_dollars(t, s + i); continue; }
		if (isdigit(c)) { i += nrl_number(t, s + i); continue; }

		i++;
		switch (c) {
		case ' ': case '\t': case '\n': case '\r': case '-': break;
		case ',': case ';':            nrl_flush(t); put(t, AL_PA4); break;
		case ':':                      nrl_flush(t); put(t, AL_PA5); break;
		case '.': case '!': case '?':  nrl_flush(t); put(t, AL_PA5); put(t, AL_PA5); break;
		case '"': case '(': case ')': case '\'': break;
		default:
			if (c >= 33 && c < 127) nrl_say_ascii(&t->ph, c);
			break;
		}
	}
	nrl_flush(t);
}

/* ---------------------------------------------------------------------- */
/*  CTS256 path                                                           */
/* ---------------------------------------------------------------------- */
static void cts_rule(tts_t *t, const char *w, int index_limit)
{
	int index = 1;
	while (index < index_limit) {
		const char *out;
		int outlen;
		int next = rules_find(&rules_cts, w, index, &out, &outlen);
		if (next < 0) { index++; continue; }
		if (outlen) put_names(t, out, outlen);
		index = next;
	}
}

static void cts_text(tts_t *t, const char *s)
{
	int i = 0;
	while (s[i]) {
		unsigned char c = (unsigned char)s[i];
		char w[MAXWORD + 3];
		int n = 0;

		if (isalpha(c) || (c == '\'' && isalpha((unsigned char)s[i + 1]))) {
			w[n++] = ' ';
			while ((isalpha((unsigned char)s[i]) || s[i] == '\'') && n < MAXWORD + 1)
				w[n++] = (char)toupper((unsigned char)s[i++]);
			w[n++] = ' '; w[n] = 0;
			{
				char bare[MAXWORD + 1];
				const uint8_t *lx;
				memcpy(bare, w + 1, (size_t)(n - 2)); bare[n - 2] = 0;
				lx = lexicon_lookup(bare);
				if (lx) { put_list(t, lx); continue; }
			}
			cts_rule(t, w, n - 1);
			continue;
		}
		/* Any other character, space and digits included, is a one-character
		 * word for the chip's punctuation and digit rules.               */
		w[0] = ' '; w[1] = (char)toupper(c); w[2] = ' '; w[3] = 0;
		if (c == '\n' || c == '\r' || c == '\t') w[1] = ' ';
		cts_rule(t, w, 2);
		i++;
	}
}

/* ---------------------------------------------------------------------- */
void tts_speak(tts_engine_t engine, const char *text, tts_emit_fn emit, void *ctx)
{
	tts_t t;
	memset(&t, 0, sizeof t);
	t.engine = engine; t.emit = emit; t.ctx = ctx; t.last = -1;

	if (engine == TTS_CTS256) cts_text(&t, text);
	else                      nrl_text(&t, text);

	/* The chip holds its last frame until a pause arrives. */
	if (t.last < 0 || t.last > AL_PA5) put(&t, AL_PA5);
}
