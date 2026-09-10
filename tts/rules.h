/*
 * rules.h -- NRL-style letter-to-sound rule matcher.
 *
 * A rule is a string  LEFT[MATCH]RIGHT=OUTPUT .  MATCH is literal text.
 * LEFT and RIGHT are context patterns made of literal letters, apostrophe,
 * ' ' (word boundary) and these symbols:
 *
 *   #  one or more vowels          :  zero or more consonants
 *   ^  one consonant               *  one or more consonants
 *   .  a voiced consonant          +  a front vowel (E I Y)
 *   >  a back vowel (O U)          &  a sibilant (S C G Z X J CH SH)
 *   @  T S R D L Z N J TH CH SH    ?  two or more vowels
 *   %  a suffix: ER E ES ED ING ELY (right context only)
 *   $  ignored (present in one CTS256 rule)
 *
 * Words are presented as " WORD " in upper case with a space either side.
 * Rules are grouped by the first character of MATCH: 0 = other, 1..26 =
 * A..Z, 27 = apostrophe, 28 = digit.  Within a group the first rule that
 * matches wins, so order matters.
 */
#pragma once

typedef struct {
	const char *const *const *groups;    /* 29 NULL-terminated arrays */
} rule_table_t;

extern const rule_table_t rules_nrl;     /* output: Wasser phoneme string */
extern const rule_table_t rules_cts;     /* output: allophone names       */

int rules_group_of(char c);

/* Try the rules for word[index].  On a match, sets *out to the OUTPUT text
 * (not NUL-terminated; *outlen bytes) and returns the index just past the
 * matched text (at least index + 1).  Returns -1 if no rule matched. */
int rules_find(const rule_table_t *t, const char *word, int index,
               const char **out, int *outlen);
