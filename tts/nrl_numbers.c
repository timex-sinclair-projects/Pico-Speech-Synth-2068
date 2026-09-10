/*
 * nrl_numbers.c -- numbers and spelled characters as Wasser phoneme strings.
 *
 * Ported from John A. Wasser's saynum.c and spellwor.c (1985, public
 * domain).  Output goes to a caller-supplied phoneme buffer; each word ends
 * with a space.
 */
#include <string.h>
#include "nrl_numbers.h"

static const char *const Cardinals[] = {
	"zIHrOW ", "wAHn ", "tUW ", "THrIY ", "fOWr ", "fAYv ", "sIHks ", "sEHvAXn ",
	"EYt ", "nAYn ", "tEHn ", "IYlEHvAXn ", "twEHlv ", "THERtIYn ", "fOWrtIYn ",
	"fIHftIYn ", "sIHkstIYn ", "sEHvEHntIYn ", "EYtIYn ", "nAYntIYn "
};
static const char *const Twenties[] = {
	"twEHntIY ", "THERtIY ", "fAOrtIY ", "fIHftIY ", "sIHkstIY ", "sEHvEHntIY ",
	"EYtIY ", "nAYntIY "
};
static const char *const Ordinals[] = {
	"zIHrOWEHTH ", "fERst ", "sEHkAHnd ", "THERd ", "fOWrTH ", "fIHfTH ", "sIHksTH ",
	"sEHvEHnTH ", "EYtTH ", "nAYnTH ", "tEHnTH ", "IYlEHvEHnTH ", "twEHlvTH ",
	"THERtIYnTH ", "fAOrtIYnTH ", "fIHftIYnTH ", "sIHkstIYnTH ", "sEHvEHntIYnTH ",
	"EYtIYnTH ", "nAYntIYnTH "
};
static const char *const Ord_twenties[] = {
	"twEHntIYEHTH ", "THERtIYEHTH ", "fOWrtIYEHTH ", "fIHftIYEHTH ", "sIHkstIYEHTH ",
	"sEHvEHntIYEHTH ", "EYtIYEHTH ", "nAYntIYEHTH "
};

/* Names for ASCII 32..126, from spellwor.c.  Letters give their names. */
static const char *const Ascii[] = {
	"spEYs ", "EHksklAEmEYSHAXn mAArk ", "dAHbl kwOWt ", "nUWmbER sAYn ", "dAAlER sAYn ",
	"pERsEHnt ", "AEmpERsAEnd ", "kwOWt ", "OWpEHn pEHrEHn ", "klOWz pEHrEHn ",
	"AEstEHrIHsk ", "plAHs ", "kAAmmAX ", "mIHnAHs ", "pIYrIYAAd ", "slAESH ",
	"zIHrOW ", "wAHn ", "tUW ", "THrIY ", "fOWr ", "fAYv ", "sIHks ", "sEHvAXn ", "EYt ", "nAYn ",
	"kAAlAXn ", "sEHmIHkAAlAXn ", "lEHs DHAEn ", "EHkwAXl sAYn ", "grEYtER DHAEn ",
	"kwEHsCHAXn mAArk ", "AEt sAYn ",
	"EY ", "bIY ", "sIY ", "dIY ", "IY ", "EHf ", "jIY ", "EYtCH ", "AY ", "jEY ", "kEY ",
	"EHl ", "EHm ", "EHn ", "OW ", "pIY ", "kyUW ", "AAr ", "EHs ", "tIY ", "yUW ", "vIY ",
	"dAHblyUW ", "EHks ", "wAY ", "zIY ",
	"lEHft brAEkEHt ", "bAEkslAESH ", "rAYt brAEkEHt ", "kAErEHt ", "AHndERskAOr ",
	"AEpAAstrAAfIY ",
	"EY ", "bIY ", "sIY ", "dIY ", "IY ", "EHf ", "jIY ", "EYtCH ", "AY ", "jEY ", "kEY ",
	"EHl ", "EHm ", "EHn ", "OW ", "pIY ", "kyUW ", "AAr ", "EHs ", "tIY ", "yUW ", "vIY ",
	"dAHblyUW ", "EHks ", "wAY ", "zIY ",
	"lEHft brEYs ", "vERtIHkAXl bAAr ", "rAYt brEYs ", "tAYld "
};

void nrl_append(phbuf_t *b, const char *s)
{
	int n = (int)strlen(s);
	if (b->len + n >= b->cap) n = b->cap - 1 - b->len;
	if (n > 0) { memcpy(b->buf + b->len, s, (size_t)n); b->len += n; }
	b->buf[b->len] = 0;
}

void nrl_say_ascii(phbuf_t *b, int c)
{
	if (c >= 32 && c < 32 + (int)(sizeof Ascii / sizeof Ascii[0]))
		nrl_append(b, Ascii[c - 32]);
}

void nrl_say_cardinal(phbuf_t *b, long value)
{
	if (value < 0) {
		nrl_append(b, "mAYnAHs ");
		value = -value;
	}
	if (value >= 1000000000L) {
		nrl_say_cardinal(b, value / 1000000000L);
		nrl_append(b, "bIHlIYAXn ");
		value %= 1000000000L;
		if (value == 0) return;
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if (value >= 1000000L) {
		nrl_say_cardinal(b, value / 1000000L);
		nrl_append(b, "mIHlIYAXn ");
		value %= 1000000L;
		if (value == 0) return;
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if ((value >= 1000L && value <= 1099L) || value >= 2000L) {
		nrl_say_cardinal(b, value / 1000L);
		nrl_append(b, "THAWzAEnd ");
		value %= 1000L;
		if (value == 0) return;
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if (value >= 100L) {
		nrl_append(b, Cardinals[value / 100]);
		nrl_append(b, "hAHndrEHd ");
		value %= 100;
		if (value == 0) return;
	}
	if (value >= 20) {
		nrl_append(b, Twenties[(value - 20) / 10]);
		value %= 10;
		if (value == 0) return;
	}
	nrl_append(b, Cardinals[value]);
}

void nrl_say_ordinal(phbuf_t *b, long value)
{
	if (value < 0) {
		nrl_append(b, "mAYnAHs ");
		value = -value;
	}
	if (value >= 1000000000L) {
		nrl_say_cardinal(b, value / 1000000000L);
		value %= 1000000000L;
		if (value == 0) { nrl_append(b, "bIHlIYAXnTH "); return; }
		nrl_append(b, "bIHlIYAXn ");
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if (value >= 1000000L) {
		nrl_say_cardinal(b, value / 1000000L);
		value %= 1000000L;
		if (value == 0) { nrl_append(b, "mIHlIYAXnTH "); return; }
		nrl_append(b, "mIHlIYAXn ");
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if ((value >= 1000L && value <= 1099L) || value >= 2000L) {
		nrl_say_cardinal(b, value / 1000L);
		value %= 1000L;
		if (value == 0) { nrl_append(b, "THAWzAEndTH "); return; }
		nrl_append(b, "THAWzAEnd ");
		if (value < 100) nrl_append(b, "AEnd ");
	}
	if (value >= 100L) {
		nrl_append(b, Cardinals[value / 100]);
		value %= 100;
		if (value == 0) { nrl_append(b, "hAHndrEHdTH "); return; }
		nrl_append(b, "hAHndrEHd ");
	}
	if (value >= 20) {
		if (value % 10 == 0) { nrl_append(b, Ord_twenties[(value - 20) / 10]); return; }
		nrl_append(b, Twenties[(value - 20) / 10]);
		value %= 10;
	}
	nrl_append(b, Ordinals[value]);
}
