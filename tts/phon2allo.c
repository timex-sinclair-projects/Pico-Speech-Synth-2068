/*
 * phon2allo.c -- Wasser/NRL phonemes -> SP0256-AL2 allophones.
 *
 * The rules are the datasheet's Table 5 "Guidelines for using the
 * allophones", made mechanical:
 *
 *   stops    PA3 before PP TT KK CH, PA2 before BB DD GG JH
 *   TT1      before S (tests, its), else TT2
 *   DD1      final or before S, else DD2
 *   BB1      final or before a consonant, else BB2
 *   KK1      before front vowels, KK2 final or before S, KK3 before back
 *            vowels and in initial clusters with R L W
 *   GG1      before high front vowels, GG2 before high back vowels and in
 *            clusters, GG3 before low vowels and final
 *   SS       doubled except in final position
 *   DH1      word-initial, DH2 elsewhere
 *   HH1/HH2, NN1/NN2   by the following vowel (front / back)
 *   YY2      word-initial, YY1 in clusters
 *   RR1      after a vowel or initial, RR2 after a consonant
 *   ER2/UW2  in monosyllables, ER1/UW1 elsewhere (UW1 always after YY)
 *   AA+r AO+r EH+r IY+r  ->  AR OR XR YR;   AX+l final -> EL
 */
#include <string.h>
#include "sp0256_names.h"
#include "phon2allo.h"

enum {
	/* vowels */
	P_IY, P_IH, P_EY, P_EH, P_AE, P_AA, P_AO, P_OW, P_UH, P_UW, P_ER, P_AX,
	P_AH, P_AY, P_AW, P_OY,
	/* merged r-coloured vowels and syllabic L */
	P_AR, P_OR, P_XR, P_YR, P_EL,
	/* consonants */
	P_p, P_b, P_t, P_d, P_k, P_g, P_f, P_v, P_TH, P_DH, P_s, P_z, P_SH, P_ZH,
	P_h, P_m, P_n, P_NG, P_l, P_w, P_y, P_r, P_CH, P_j, P_WH,
	P_NONE
};

static const struct { const char *name; int id; } two[] = {
	{"IY",P_IY},{"IH",P_IH},{"EY",P_EY},{"EH",P_EH},{"AE",P_AE},{"AA",P_AA},
	{"AO",P_AO},{"OW",P_OW},{"UH",P_UH},{"UW",P_UW},{"ER",P_ER},{"AX",P_AX},
	{"AH",P_AH},{"AY",P_AY},{"AW",P_AW},{"OY",P_OY},{"TH",P_TH},{"DH",P_DH},
	{"SH",P_SH},{"ZH",P_ZH},{"NG",P_NG},{"CH",P_CH},{"WH",P_WH},{"HH",P_h}
};
static const struct { char c; int id; } one[] = {
	{'p',P_p},{'b',P_b},{'t',P_t},{'d',P_d},{'k',P_k},{'g',P_g},{'f',P_f},
	{'v',P_v},{'s',P_s},{'z',P_z},{'h',P_h},{'m',P_m},{'n',P_n},{'l',P_l},
	{'w',P_w},{'y',P_y},{'r',P_r},{'j',P_j}
};

static int is_vowel(int p) { return p >= P_IY && p <= P_EL; }
static int is_back(int p)  { return p == P_UW || p == P_UH || p == P_OW || p == P_OY ||
                                    p == P_AO || p == P_AA || p == P_AW || p == P_AR || p == P_OR; }
static int is_high_front(int p) { return p == P_IY || p == P_IH || p == P_EY || p == P_EH || p == P_y || p == P_YR; }
static int is_sib(int p)   { return p == P_s || p == P_z; }

#define MAXPH 64

static int parse(const char *ph, int len, int *out)
{
	int n = 0, i = 0;
	while (i < len && n < MAXPH) {
		char c = ph[i];
		if (c >= 'A' && c <= 'Z' && i + 1 < len) {
			int k, id = P_NONE;
			for (k = 0; k < (int)(sizeof two / sizeof two[0]); k++)
				if (two[k].name[0] == c && two[k].name[1] == ph[i + 1]) { id = two[k].id; break; }
			i += 2;
			if (id != P_NONE) out[n++] = id;
		} else {
			int k, id = P_NONE;
			for (k = 0; k < (int)(sizeof one / sizeof one[0]); k++)
				if (one[k].c == c) { id = one[k].id; break; }
			i++;
			if (id != P_NONE) out[n++] = id;
		}
	}
	return n;
}

/* Merge vowel+r pairs and final AX+l into the chip's combined allophones. */
static int merge(int *p, int n)
{
	int i, j = 0;
	for (i = 0; i < n; i++) {
		int cur = p[i], nxt = (i + 1 < n) ? p[i + 1] : P_NONE;
		if (nxt == P_r) {
			int m = P_NONE;
			if      (cur == P_AA) m = P_AR;
			else if (cur == P_AO) m = P_OR;
			else if (cur == P_EH) m = P_XR;
			else if (cur == P_IY) m = P_YR;
			if (m != P_NONE) { p[j++] = m; i++; continue; }
		}
		if (cur == P_AX && nxt == P_l && i + 2 == n) { p[j++] = P_EL; i++; continue; }
		p[j++] = cur;
	}
	return j;
}

typedef struct { allo_emit_fn emit; void *ctx; int last; } out_t;

static void put(out_t *o, int a)
{
	o->emit(o->ctx, (uint8_t)a);
	o->last = a;
}

static void stop_pause(out_t *o, int pause)
{
	if (o->last > AL_PA5 || o->last < 0)      /* no pause already pending */
		put(o, pause);
}

void phon2allo_word(const char *ph, int len, int after_pause, allo_emit_fn emit, void *ctx)
{
	int p[MAXPH];
	int n = parse(ph, len, p), i, nv = 0;
	out_t o = { emit, ctx, after_pause ? AL_PA3 : -1 };

	n = merge(p, n);
	for (i = 0; i < n; i++) if (is_vowel(p[i])) nv++;

	for (i = 0; i < n; i++) {
		int cur  = p[i];
		int prev = i ? p[i - 1] : P_NONE;
		int next = (i + 1 < n) ? p[i + 1] : P_NONE;
		int final = (next == P_NONE);
		int initial = (i == 0);

		switch (cur) {
		case P_IY: put(&o, AL_IY); break;
		case P_IH: put(&o, AL_IH); break;
		case P_EY: put(&o, AL_EY); break;
		case P_EH: put(&o, AL_EH); break;
		case P_AE: put(&o, AL_AE); break;
		case P_AA: put(&o, AL_AA); break;
		case P_AO: put(&o, AL_AO); break;
		case P_OW: put(&o, AL_OW); break;
		case P_UH: put(&o, AL_UH); break;
		case P_UW: put(&o, (prev == P_y) ? AL_UW1 : AL_UW2); break;
		case P_ER: put(&o, (nv <= 1) ? AL_ER2 : AL_ER1); break;
		case P_AX: put(&o, AL_AX); break;
		case P_AH: put(&o, AL_AX); break;
		case P_AY: put(&o, AL_AY); break;
		case P_AW: put(&o, AL_AW); break;
		case P_OY: put(&o, AL_OY); break;
		case P_AR: put(&o, AL_AR); break;
		case P_OR: put(&o, AL_OR); break;
		case P_XR: put(&o, AL_XR); break;
		case P_YR: put(&o, AL_YR); break;
		case P_EL: put(&o, AL_EL); break;

		case P_p:  stop_pause(&o, AL_PA3); put(&o, AL_PP); break;
		case P_t:  stop_pause(&o, AL_PA3); put(&o, is_sib(next) ? AL_TT1 : AL_TT2); break;
		case P_CH: stop_pause(&o, AL_PA3); put(&o, AL_CH); break;
		case P_k:
			stop_pause(&o, AL_PA3);
			if (final || is_sib(next))                       put(&o, AL_KK2);
			else if (is_back(next) || next == P_r || next == P_l || next == P_w) put(&o, AL_KK3);
			else                                             put(&o, AL_KK1);
			break;
		case P_b:  stop_pause(&o, AL_PA2); put(&o, (final || !is_vowel(next)) ? AL_BB1 : AL_BB2); break;
		case P_d:  stop_pause(&o, AL_PA2); put(&o, (final || is_sib(next)) ? AL_DD1 : AL_DD2); break;
		case P_j:  stop_pause(&o, AL_PA2); put(&o, AL_JH); break;
		case P_g:
			stop_pause(&o, AL_PA2);
			if (final)                                                       put(&o, AL_GG3);
			else if (is_high_front(next))                                    put(&o, AL_GG1);
			else if (next == P_UW || next == P_UH || next == P_OW || next == P_OY ||
			         next == P_AX || next == P_l || next == P_r || next == P_w) put(&o, AL_GG2);
			else                                                             put(&o, AL_GG3);
			break;

		case P_f:  put(&o, AL_FF); break;
		case P_v:  put(&o, AL_VV); break;
		case P_TH: put(&o, AL_TH); break;
		case P_DH: put(&o, initial ? AL_DH1 : AL_DH2); break;
		case P_s:  put(&o, AL_SS); if (!final) put(&o, AL_SS); break;
		case P_z:  put(&o, AL_ZZ); break;
		case P_SH: put(&o, AL_SH); break;
		case P_ZH: put(&o, AL_ZH); break;
		case P_h:  put(&o, is_back(next) ? AL_HH2 : AL_HH1); break;
		case P_m:  put(&o, AL_MM); break;
		case P_n:  put(&o, is_back(next) ? AL_NN2 : AL_NN1); break;
		case P_NG: put(&o, AL_NG); break;
		case P_l:  put(&o, AL_LL); break;
		case P_w:  put(&o, AL_WW); break;
		case P_WH: put(&o, AL_WH); break;
		case P_y:  put(&o, initial ? AL_YY2 : AL_YY1); break;
		case P_r:  put(&o, (prev != P_NONE && !is_vowel(prev)) ? AL_RR2 : AL_RR1); break;
		default: break;
		}
	}
}
