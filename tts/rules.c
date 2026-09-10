/*
 * rules.c -- NRL-style letter-to-sound rule matcher.
 *
 * Context matching follows John A. Wasser's phoneme.c (public domain) with
 * the extra symbols used by the CTS256A-AL2 rule set (* > & @ ? $ <).
 */
#include <string.h>
#include "rules.h"

static int is_vowel(char c) { return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U'; }
static int is_cons(char c)  { return c >= 'A' && c <= 'Z' && !is_vowel(c); }
static int in_set(const char *set, char c) { return c != 0 && strchr(set, c) != NULL; }
static int is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

int rules_group_of(char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
	if (c == '\'') return 27;
	if (c >= '0' && c <= '9') return 28;
	return 0;
}

/* Two-letter sibilant / '@' digraph tests, looking forward or backward. */
static int digraph_fwd(const char *t, const char *set)  /* CH SH TH at t */
{
	return t[1] == 'H' && in_set(set, t[0]);
}
static int digraph_bwd(const char *t, const char *set)  /* CH SH TH ending at t */
{
	return t[0] == 'H' && in_set(set, t[-1]);
}

/* text points at the character just before the match; pattern is scanned
 * from its end toward its start.  The word buffer begins with ' ', which is
 * never a vowel or consonant, so the loops stop there.                    */
static int left_match(const char *pat, int len, const char *text)
{
	const char *p = pat + len - 1;

	for (; p >= pat; p--) {
		char c = *p;
		if (is_alpha(c) || c == '\'' || c == ' ') {
			if (c != *text) return 0;
			text--;
			continue;
		}
		switch (c) {
		case '#': if (!is_vowel(*text)) return 0; text--; while (is_vowel(*text)) text--; break;
		case '?': if (!is_vowel(text[0]) || !is_vowel(text[-1])) return 0; text -= 2; while (is_vowel(*text)) text--; break;
		case ':': while (is_cons(*text)) text--; break;
		case '*': if (!is_cons(*text)) return 0; text--; while (is_cons(*text)) text--; break;
		case '^': if (!is_cons(*text)) return 0; text--; break;
		case '.': if (!in_set("BDGJLMNRVWXZ", *text)) return 0; text--; break;
		case '+': if (!in_set("EIY", *text)) return 0; text--; break;
		case '>': if (!in_set("OU", *text)) return 0; text--; break;
		case '&':
			if (digraph_bwd(text, "CS")) text -= 2;
			else if (in_set("SCGZXJ", *text)) text--;
			else return 0;
			break;
		case '@':
			if (digraph_bwd(text, "TCS")) text -= 2;
			else if (in_set("TSRDLZNJ", *text)) text--;
			else return 0;
			break;
		case '$': break;
		default:  return 0;      /* '%' is not valid on the left */
		}
	}
	return 1;
}

/* text points at the first character after the match. */
static int right_match(const char *pat, int len, const char *text)
{
	const char *end = pat + len;

	for (; pat < end; pat++) {
		char c = *pat;
		if (is_alpha(c) || c == '\'' || c == ' ') {
			if (c != *text) return 0;
			text++;
			continue;
		}
		switch (c) {
		case '#': if (!is_vowel(*text)) return 0; text++; while (is_vowel(*text)) text++; break;
		case '?': if (!is_vowel(text[0]) || !is_vowel(text[1])) return 0; text += 2; while (is_vowel(*text)) text++; break;
		case ':': while (is_cons(*text)) text++; break;
		case '*': if (!is_cons(*text)) return 0; text++; while (is_cons(*text)) text++; break;
		case '^': if (!is_cons(*text)) return 0; text++; break;
		case '.': if (!in_set("BDGJLMNRVWXZ", *text)) return 0; text++; break;
		case '+': if (!in_set("EIY", *text)) return 0; text++; break;
		case '>': if (!in_set("OU", *text)) return 0; text++; break;
		case '&':
			if (digraph_fwd(text, "CS")) text += 2;
			else if (in_set("SCGZXJ", *text)) text++;
			else return 0;
			break;
		case '@':
			if (digraph_fwd(text, "TCS")) text += 2;
			else if (in_set("TSRDLZNJ", *text)) text++;
			else return 0;
			break;
		case '%':                /* ER E ES ED ING ELY, as in Wasser */
			if (*text == 'E') {
				text++;
				if (*text == 'L') {
					if (text[1] == 'Y') text += 2;
					/* else: E alone, do not gobble the L */
				} else if (*text == 'R' || *text == 'S' || *text == 'D') {
					text++;
				}
			} else if (text[0] == 'I' && text[1] == 'N' && text[2] == 'G') {
				text += 3;
			} else {
				return 0;
			}
			break;
		case '$': break;
		default:  return 0;
		}
	}
	return 1;
}

int rules_find(const rule_table_t *t, const char *word, int index,
               const char **out, int *outlen)
{
	const char *const *rules = t->groups[rules_group_of(word[index])];

	for (; *rules; rules++) {
		const char *r  = *rules;
		const char *lb = strchr(r, '[');
		const char *rb = lb ? strchr(lb, ']') : NULL;
		const char *eq = rb ? strchr(rb, '=') : NULL;
		int mlen, k;

		if (!lb || !rb || !eq) continue;               /* malformed */
		mlen = (int)(rb - lb - 1);

		for (k = 0; k < mlen; k++)
			if (word[index + k] != lb[1 + k]) break;
		if (k < mlen) continue;

		if (!left_match(r, (int)(lb - r), word + index - 1)) continue;
		if (!right_match(rb + 1, (int)(eq - rb - 1), word + index + mlen)) continue;

		*out    = eq + 1;
		*outlen = (int)strlen(eq + 1);
		return index + (mlen ? mlen : 1);
	}
	return -1;
}
