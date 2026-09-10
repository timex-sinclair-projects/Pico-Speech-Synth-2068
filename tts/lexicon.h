/* lexicon.h -- exception dictionary. */
#pragma once
#include <stdint.h>

/* Upper-case word -> allophone list terminated by 0xFF, or NULL. */
const uint8_t *lexicon_lookup(const char *word);
