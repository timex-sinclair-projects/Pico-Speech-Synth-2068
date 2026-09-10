/*
 * lexicon.c -- exception dictionary, consulted before the rules by both
 * engines.  Entries are upper-case words; allophone lists end with 0xFF.
 * Seeded from the SP0256-AL2 datasheet's dictionary (Table 1) and a few
 * words the rules get wrong.  Keep it small: every entry costs flash.
 */
#include <string.h>
#include "sp0256_names.h"
#include "lexicon.h"

#define E 0xFF

static const struct { const char *word; uint8_t allo[16]; } lex[] = {
	{ "HELLO",     { AL_HH1, AL_EH, AL_LL, AL_AX, AL_OW, E } },
	{ "COMPUTER",  { AL_KK1, AL_AX, AL_MM, AL_PP, AL_YY1, AL_UW1, AL_TT2, AL_ER1, E } },
	{ "SINCLAIR",  { AL_SS, AL_SS, AL_IH, AL_NG, AL_PA3, AL_KK1, AL_LL, AL_XR, E } },
	{ "TIMEX",     { AL_TT2, AL_AY, AL_MM, AL_EH, AL_PA3, AL_KK2, AL_SS, E } },
	{ "ZX",        { AL_ZZ, AL_EH, AL_DD1, AL_PA2, AL_EH, AL_PA3, AL_KK2, AL_SS, E } },
	{ "SPECTRUM",  { AL_SS, AL_SS, AL_PA3, AL_PP, AL_EH, AL_PA3, AL_KK1, AL_TT2, AL_RR2, AL_AX, AL_MM, E } },
	{ "ROBOT",     { AL_RR1, AL_OW, AL_PA2, AL_BB2, AL_AA, AL_PA3, AL_TT2, E } },
	{ "SPEAK",     { AL_SS, AL_SS, AL_PA3, AL_PP, AL_IY, AL_PA3, AL_KK2, E } },
	{ "ALARM",     { AL_AX, AL_LL, AL_AR, AL_MM, E } },
	{ "CLOCK",     { AL_KK1, AL_LL, AL_AA, AL_AA, AL_PA3, AL_KK2, E } },
	{ "DAUGHTER",  { AL_DD2, AL_AO, AL_TT2, AL_ER1, E } },
	{ "ENGINE",    { AL_EH, AL_NN1, AL_PA2, AL_JH, AL_IH, AL_NN1, E } },
	{ "YEAR",      { AL_YY2, AL_YR, E } },
	{ "THE",       { AL_DH1, AL_AX, E } },
};

const uint8_t *lexicon_lookup(const char *word)
{
	unsigned i;
	for (i = 0; i < sizeof lex / sizeof lex[0]; i++)
		if (!strcmp(lex[i].word, word))
			return lex[i].allo;
	return NULL;
}
