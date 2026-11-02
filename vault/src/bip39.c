#include "oyama/bip39.h"

#include <sodium.h>

#if defined(__linux__)
#include <sys/random.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern const char *const oyama_bip39_words[2048];

static oyama_bip39_entropy_provider current_entropy_provider = NULL;

static int default_entropy(unsigned char *buf, size_t len) {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(buf, len);
    return 0;
#else
    /* Linux and others: getrandom with /dev/urandom fallback. */
    ssize_t r = getrandom(buf, len, 0);
    if (r == (ssize_t)len) {
        return 0;
    }
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    size_t done = 0;
    while (done < len) {
        ssize_t n = read(fd, buf + done, len - done);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        done += (size_t)n;
    }
    close(fd);
    return 0;
#endif
}

void oyama_bip39_set_entropy_provider(oyama_bip39_entropy_provider fn) {
    current_entropy_provider = fn;
}

static int get_entropy(unsigned char *buf, size_t len) {
    if (current_entropy_provider != NULL) {
        return current_entropy_provider(buf, len);
    }
    return default_entropy(buf, len);
}

static int fail(oyama_error *err, int code, const char *msg) {
    if (err != NULL) {
        err->code = code;
        snprintf(err->message, sizeof(err->message), "%s", msg);
    }
    return code;
}

static int word_index(const char *word) {
    int i;
    for (i = 0; i < 2048; ++i) {
        if (strcmp(oyama_bip39_words[i], word) == 0) {
            return i;
        }
    }
    return -1;
}

int oyama_bip39_word_count(const char *phrase) {
    int count = 0;
    int in_word = 0;
    const char *p;
    if (phrase == NULL) {
        return 0;
    }
    for (p = phrase; *p != '\0'; ++p) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            in_word = 0;
        } else {
            if (!in_word) {
                ++count;
                in_word = 1;
            }
        }
    }
    return count;
}

int oyama_bip39_normalize(const char *in, char *out, size_t out_len,
                           oyama_error *err) {
    size_t pos = 0;
    int in_word = 0;
    int need_space = 0;
    const char *p;
    if (in == NULL || out == NULL || out_len == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid normalize argument");
    }
    for (p = in; *p != '\0'; ++p) {
        char c = *p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (in_word) {
                in_word = 0;
                need_space = 1;
            }
        } else {
            if (need_space) {
                if (pos + 1 >= out_len) {
                    return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                                "normalize buffer too small");
                }
                out[pos++] = ' ';
                need_space = 0;
            }
            in_word = 1;
            if (c >= 'A' && c <= 'Z') {
                c = (char)(c + 32);
            }
            if (pos + 1 >= out_len) {
                return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                            "normalize buffer too small");
            }
            out[pos++] = c;
        }
    }
    out[pos] = '\0';
    return OYAMA_VAULT_OK;
}

int oyama_bip39_generate(char *out, size_t out_len, oyama_error *err) {
    unsigned char entropy[32];
    unsigned char checksum[32];
    unsigned char data[33];
    unsigned int acc = 0;
    int acc_bits = 0;
    int word_idx = 0;
    char *p;
    size_t remaining;
    int i;

    if (out == NULL || out_len < OYAMA_BIP39_MAX_LEN) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid generate buffer");
    }

    if (get_entropy(entropy, 32) != 0) {
        sodium_memzero(entropy, sizeof(entropy));
        return fail(err, OYAMA_VAULT_ERR_CRYPTO, "entropy source unavailable");
    }

    crypto_hash_sha256(checksum, entropy, 32);

    memcpy(data, entropy, 32);
    data[32] = checksum[0];

    p = out;
    remaining = out_len;
    *p = '\0';

    for (i = 0; i < 33; ++i) {
        acc = (acc << 8) | data[i];
        acc_bits += 8;
        while (acc_bits >= 11 && word_idx < OYAMA_BIP39_WORDS) {
            acc_bits -= 11;
            {
                unsigned int idx = (acc >> acc_bits) & 0x7FFu;
                const char *w = oyama_bip39_words[idx];
                size_t wlen = strlen(w);
                if (word_idx > 0) {
                    if (remaining < 2) {
                        sodium_memzero(entropy, sizeof(entropy));
                        sodium_memzero(data, sizeof(data));
                        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                                    "output buffer too small");
                    }
                    *p++ = ' ';
                    --remaining;
                }
                if (remaining < wlen + 1) {
                    sodium_memzero(entropy, sizeof(entropy));
                    sodium_memzero(data, sizeof(data));
                    return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                                "output buffer too small");
                }
                memcpy(p, w, wlen);
                p += wlen;
                remaining -= wlen;
                *p = '\0';
                ++word_idx;
            }
        }
    }

    sodium_memzero(entropy, sizeof(entropy));
    sodium_memzero(data, sizeof(data));
    sodium_memzero(checksum, sizeof(checksum));

    if (word_idx != OYAMA_BIP39_WORDS) {
        return fail(err, OYAMA_VAULT_ERR_CRYPTO, "phrase generation failed");
    }
    return OYAMA_VAULT_OK;
}

int oyama_bip39_validate(const char *phrase, oyama_error *err) {
    char buf[OYAMA_BIP39_MAX_LEN];
    unsigned int indices[OYAMA_BIP39_WORDS];
    unsigned char data[33];
    unsigned char hash[32];
    char *tok;
    int i = 0;
    int n;

    if (phrase == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT, "invalid phrase");
    }

    if (oyama_bip39_normalize(phrase, buf, sizeof(buf), err) !=
        OYAMA_VAULT_OK) {
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }

    n = oyama_bip39_word_count(buf);
    if (n != OYAMA_BIP39_WORDS) {
        return fail(err, OYAMA_VAULT_ERR_BAD_PASSPHRASE,
                    "phrase must contain 24 words");
    }

    tok = strtok(buf, " ");
    while (tok != NULL && i < OYAMA_BIP39_WORDS) {
        int idx = word_index(tok);
        if (idx < 0) {
            return fail(err, OYAMA_VAULT_ERR_BAD_PASSPHRASE,
                        "unknown BIP39 word");
        }
        indices[i++] = (unsigned int)idx;
        tok = strtok(NULL, " ");
    }
    if (i != OYAMA_BIP39_WORDS || tok != NULL) {
        return fail(err, OYAMA_VAULT_ERR_BAD_PASSPHRASE, "invalid word count");
    }

    memset(data, 0, sizeof(data));
    {
        unsigned int acc = 0;
        int acc_bits = 0;
        size_t byte_pos = 0;
        for (i = 0; i < OYAMA_BIP39_WORDS; ++i) {
            acc = (acc << 11) | indices[i];
            acc_bits += 11;
            while (acc_bits >= 8) {
                acc_bits -= 8;
                data[byte_pos++] = (unsigned char)((acc >> acc_bits) & 0xFFu);
            }
        }
    }

    crypto_hash_sha256(hash, data, 32);
    if (hash[0] != data[32]) {
        sodium_memzero(data, sizeof(data));
        sodium_memzero(hash, sizeof(hash));
        return fail(err, OYAMA_VAULT_ERR_BAD_PASSPHRASE, "bad checksum");
    }

    sodium_memzero(data, sizeof(data));
    sodium_memzero(hash, sizeof(hash));
    return OYAMA_VAULT_OK;
}