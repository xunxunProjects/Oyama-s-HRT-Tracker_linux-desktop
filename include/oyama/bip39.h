#ifndef OYAMA_BIP39_H
#define OYAMA_BIP39_H

#include "oyama/vault.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OYAMA_BIP39_WORDS 24
#define OYAMA_BIP39_MAX_LEN 512

typedef int (*oyama_bip39_entropy_provider)(unsigned char *buf, size_t len);

int oyama_bip39_generate(char *out, size_t out_len, oyama_error *err);
int oyama_bip39_validate(const char *phrase, oyama_error *err);
int oyama_bip39_word_count(const char *phrase);
int oyama_bip39_normalize(const char *in, char *out, size_t out_len,
                           oyama_error *err);

/* Test hook: replace the CSPRNG. Pass NULL to restore the default. */
void oyama_bip39_set_entropy_provider(oyama_bip39_entropy_provider fn);

#ifdef __cplusplus
}
#endif

#endif
