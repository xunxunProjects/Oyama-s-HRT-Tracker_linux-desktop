#include "oyama/bip39.h"

#include "oyama/vault.h"

#include <sodium.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static void clear_error(oyama_error *err) {
    memset(err, 0, sizeof(*err));
}

static int entropy_fail(unsigned char *buf, size_t len) {
    (void)buf;
    (void)len;
    return -1;
}

static void test_vectors(void) {
    oyama_error err;

    clear_error(&err);
    check(oyama_bip39_validate(
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon art",
              &err) == OYAMA_VAULT_OK,
          "all-zero vector validates");
    if (err.code != OYAMA_VAULT_OK) {
        fprintf(stderr, "  err: %s\n", err.message);
    }

    clear_error(&err);
    check(oyama_bip39_validate(
              "legal winner thank year wave sausage worth useful "
              "legal winner thank year wave sausage worth useful "
              "legal winner thank year wave sausage worth title",
              &err) == OYAMA_VAULT_OK,
          "legal-winner vector validates");
}

static void test_roundtrip(void) {
    int i;
    for (i = 0; i < 100; ++i) {
        char phrase[OYAMA_BIP39_MAX_LEN];
        oyama_error err;
        clear_error(&err);
        if (oyama_bip39_generate(phrase, sizeof(phrase), &err) !=
            OYAMA_VAULT_OK) {
            fprintf(stderr, "FAIL: generate %d: %s\n", i, err.message);
            ++failures;
            continue;
        }
        clear_error(&err);
        if (oyama_bip39_validate(phrase, &err) != OYAMA_VAULT_OK) {
            fprintf(stderr, "FAIL: validate %d: %s (%s)\n", i, err.message,
                    phrase);
            ++failures;
        }
        check(oyama_bip39_word_count(phrase) == OYAMA_BIP39_WORDS,
              "roundtrip word count");
    }
}

static void test_reject(void) {
    oyama_error err;

    /* bad checksum: change last word */
    clear_error(&err);
    check(oyama_bip39_validate(
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon about",
              &err) != OYAMA_VAULT_OK,
          "bad checksum rejected");
    /* non-wordlist word */
    clear_error(&err);
    check(oyama_bip39_validate(
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon abandon abandon "
              "abandon abandon zzzzzzzz",
              &err) != OYAMA_VAULT_OK,
          "non-wordlist word rejected");
    /* 12 words */
    clear_error(&err);
    check(oyama_bip39_validate(
              "abandon abandon abandon abandon abandon abandon "
              "abandon abandon abandon abandon abandon about",
              &err) != OYAMA_VAULT_OK,
          "12 words rejected");
    /* 25 words */
    {
        char buf[OYAMA_BIP39_MAX_LEN];
        snprintf(buf, sizeof(buf),
                 "abandon abandon abandon abandon abandon abandon "
                 "abandon abandon abandon abandon abandon abandon "
                 "abandon abandon abandon abandon abandon abandon "
                 "abandon abandon abandon abandon abandon abandon art");
        clear_error(&err);
        check(oyama_bip39_validate(buf, &err) != OYAMA_VAULT_OK,
              "25 words rejected");
    }
}

static void test_word_count_spaces(void) {
    check(oyama_bip39_word_count("  a b c  ") == 3,
          "word_count leading/trailing spaces");
    check(oyama_bip39_word_count("") == 0, "word_count empty");
    check(oyama_bip39_word_count("one") == 1, "word_count single");
}

static void test_entropy_fail(void) {
    oyama_error err;
    char phrase[OYAMA_BIP39_MAX_LEN];
    oyama_bip39_set_entropy_provider(entropy_fail);
    clear_error(&err);
    check(oyama_bip39_generate(phrase, sizeof(phrase), &err) !=
              OYAMA_VAULT_OK,
          "entropy failure returns error");
    check(err.code == OYAMA_VAULT_ERR_CRYPTO, "entropy failure code is CRYPTO");
    oyama_bip39_set_entropy_provider(NULL);
}

static void test_normalize(void) {
    char out[OYAMA_BIP39_MAX_LEN];
    oyama_error err;
    clear_error(&err);
    check(oyama_bip39_normalize("  Foo  BAR  baz ", out, sizeof(out), &err) ==
              OYAMA_VAULT_OK,
          "normalize ok");
    check(strcmp(out, "foo bar baz") == 0, "normalize result");
}

int main(void) {
    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return 1;
    }
    test_vectors();
    test_roundtrip();
    test_reject();
    test_word_count_spaces();
    test_entropy_fail();
    test_normalize();

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("bip39_test: PASS\n");
    return 0;
}