#ifndef OYAMA_VAULT_CRYPTO_H
#define OYAMA_VAULT_CRYPTO_H

#include "oyama/vault.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OYAMA_VAULT_META_VERSION 1
#define OYAMA_VAULT_RECORD_VERSION 1

#define OYAMA_VAULT_ALG_AES256GCM 1
#define OYAMA_VAULT_ALG_XCHACHA20POLY1305 2

#define OYAMA_VAULT_KEY_BYTES 32
#define OYAMA_VAULT_SALT_BYTES 16
#define OYAMA_VAULT_TAG_BYTES 16
#define OYAMA_VAULT_AES_NONCE_BYTES 12
#define OYAMA_VAULT_XCHACHA_NONCE_BYTES 24

#define OYAMA_VAULT_KDF_ARGON2ID13 2

#define OYAMA_VAULT_META_AAD_MAX 64
#define OYAMA_VAULT_RECORD_AAD_BYTES 39

int oyama_vault_crypto_init(void);

int oyama_vault_derive_key(const char *phrase, size_t phrase_len,
                           const unsigned char salt[OYAMA_VAULT_SALT_BYTES],
                           uint64_t opslimit, uint64_t memlimit,
                           unsigned char out[OYAMA_VAULT_KEY_BYTES],
                           oyama_error *err);

int oyama_vault_wrap_dek(const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                         const unsigned char kek[OYAMA_VAULT_KEY_BYTES],
                         const unsigned char *aad, size_t aad_len,
                         unsigned char alg,
                         unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES],
                         size_t *nonce_len,
                         unsigned char *out, size_t *out_len,
                         oyama_error *err);

int oyama_vault_unwrap_dek(const unsigned char *wrapped, size_t wrapped_len,
                           const unsigned char kek[OYAMA_VAULT_KEY_BYTES],
                           const unsigned char *aad, size_t aad_len,
                           unsigned char alg,
                           const unsigned char *nonce, size_t nonce_len,
                           unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                           oyama_error *err);

int oyama_vault_seal_record(const unsigned char uuid[16],
                            const unsigned char *envelope,
                            size_t envelope_len,
                            const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                            unsigned char alg,
                            unsigned char **out, size_t *out_len,
                            oyama_error *err);

int oyama_vault_open_record(const unsigned char uuid[16],
                            const unsigned char *blob, size_t blob_len,
                            const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                            unsigned char alg,
                            unsigned char **envelope,
                            size_t *envelope_len,
                            oyama_error *err);

int oyama_vault_build_meta_aad(
    const unsigned char salt[OYAMA_VAULT_SALT_BYTES],
    uint64_t opslimit, uint64_t memlimit, unsigned char alg,
    unsigned char out[OYAMA_VAULT_META_AAD_MAX], size_t *out_len);

void oyama_vault_secure_free(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
