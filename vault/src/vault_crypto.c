#include "vault_crypto.h"

#include <sodium.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int crypto_ready = 0;

static int fail(oyama_error *err, int code, const char *message) {
    if (err != NULL) {
        err->code = code;
        (void)snprintf(err->message, sizeof(err->message), "%s", message);
    }
    return code;
}

static int succeed(oyama_error *err) {
    if (err != NULL) {
        err->code = OYAMA_VAULT_OK;
        err->message[0] = '\0';
    }
    return OYAMA_VAULT_OK;
}

static void write_u32_be(unsigned char *out, uint32_t value) {
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

static void write_u64_be(unsigned char *out, uint64_t value) {
    out[0] = (unsigned char)(value >> 56);
    out[1] = (unsigned char)(value >> 48);
    out[2] = (unsigned char)(value >> 40);
    out[3] = (unsigned char)(value >> 32);
    out[4] = (unsigned char)(value >> 24);
    out[5] = (unsigned char)(value >> 16);
    out[6] = (unsigned char)(value >> 8);
    out[7] = (unsigned char)value;
}

static size_t nonce_len_for_alg(unsigned char alg) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return OYAMA_VAULT_AES_NONCE_BYTES;
    }
    if (alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305) {
        return OYAMA_VAULT_XCHACHA_NONCE_BYTES;
    }
    return 0;
}

static int algorithm_available(unsigned char alg) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return crypto_aead_aes256gcm_is_available() == 1;
    }
    return alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305;
}

static int aead_encrypt(unsigned char alg, unsigned char *ciphertext,
                        unsigned long long *ciphertext_len,
                        const unsigned char *plaintext,
                        unsigned long long plaintext_len,
                        const unsigned char *aad, unsigned long long aad_len,
                        const unsigned char *nonce,
                        const unsigned char *key) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return crypto_aead_aes256gcm_encrypt(
            ciphertext, ciphertext_len, plaintext, plaintext_len, aad, aad_len,
            NULL, nonce, key);
    }
    if (alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305) {
        return crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext, ciphertext_len, plaintext, plaintext_len, aad, aad_len,
            NULL, nonce, key);
    }
    return -1;
}

static int aead_decrypt(unsigned char alg, unsigned char *plaintext,
                        unsigned long long *plaintext_len,
                        const unsigned char *ciphertext,
                        unsigned long long ciphertext_len,
                        const unsigned char *aad, unsigned long long aad_len,
                        const unsigned char *nonce,
                        const unsigned char *key) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return crypto_aead_aes256gcm_decrypt(
            plaintext, plaintext_len, NULL, ciphertext, ciphertext_len, aad,
            aad_len, nonce, key);
    }
    if (alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305) {
        return crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext, plaintext_len, NULL, ciphertext, ciphertext_len, aad,
            aad_len, nonce, key);
    }
    return -1;
}

int oyama_vault_crypto_init(void) {
    if (crypto_ready) {
        return 0;
    }
    if (sodium_init() < 0) {
        return -1;
    }
    crypto_ready = 1;
    return 0;
}

int oyama_vault_derive_key(const char *phrase, size_t phrase_len,
                           const unsigned char salt[OYAMA_VAULT_SALT_BYTES],
                           uint64_t opslimit, uint64_t memlimit,
                           unsigned char out[OYAMA_VAULT_KEY_BYTES],
                           oyama_error *err) {
    if (phrase == NULL || salt == NULL || out == NULL || phrase_len == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid key derivation argument");
    }
    if (opslimit < 1 || opslimit > 10 || memlimit < 8 * 1024 * 1024 ||
        memlimit > 256 * 1024 * 1024) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "KDF parameters are out of range");
    }
    if (oyama_vault_crypto_init() != 0) {
        return fail(err, OYAMA_VAULT_ERR_CRYPTO,
                    "libsodium initialization failed");
    }
    if (crypto_pwhash(out, OYAMA_VAULT_KEY_BYTES, phrase,
                      (unsigned long long)phrase_len, salt,
                      (unsigned long long)opslimit, (size_t)memlimit,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        sodium_memzero(out, OYAMA_VAULT_KEY_BYTES);
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "key derivation ran out of memory");
    }
    return succeed(err);
}

int oyama_vault_wrap_dek(const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                         const unsigned char kek[OYAMA_VAULT_KEY_BYTES],
                         const unsigned char *aad, size_t aad_len,
                         unsigned char alg,
                         unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES],
                         size_t *nonce_len, unsigned char *out,
                         size_t *out_len, oyama_error *err) {
    size_t nl;
    unsigned long long ciphertext_len = 0;
    int rc;

    if (dek == NULL || kek == NULL || nonce == NULL || nonce_len == NULL ||
        out == NULL || out_len == NULL || (aad == NULL && aad_len != 0)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid DEK wrap argument");
    }
    nl = nonce_len_for_alg(alg);
    if (nl == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "unknown AEAD algorithm");
    }
    if (!algorithm_available(alg)) {
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "AEAD algorithm is not available");
    }
    if (oyama_vault_crypto_init() != 0) {
        return fail(err, OYAMA_VAULT_ERR_CRYPTO,
                    "libsodium initialization failed");
    }

    randombytes_buf(nonce, nl);
    rc = aead_encrypt(alg, out, &ciphertext_len, dek, OYAMA_VAULT_KEY_BYTES,
                      aad, (unsigned long long)aad_len, nonce, kek);
    if (rc != 0) {
        sodium_memzero(out, OYAMA_VAULT_KEY_BYTES + OYAMA_VAULT_TAG_BYTES);
        return fail(err, OYAMA_VAULT_ERR_CRYPTO, "DEK wrap failed");
    }
    *nonce_len = nl;
    *out_len = (size_t)ciphertext_len;
    return succeed(err);
}

int oyama_vault_unwrap_dek(const unsigned char *wrapped, size_t wrapped_len,
                           const unsigned char kek[OYAMA_VAULT_KEY_BYTES],
                           const unsigned char *aad, size_t aad_len,
                           unsigned char alg, const unsigned char *nonce,
                           size_t nonce_len,
                           unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                           oyama_error *err) {
    size_t expected_nonce = nonce_len_for_alg(alg);
    unsigned long long plaintext_len = 0;
    int rc;

    if (wrapped == NULL || kek == NULL || nonce == NULL || dek == NULL ||
        (aad == NULL && aad_len != 0)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid DEK unwrap argument");
    }
    if (expected_nonce == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "unknown AEAD algorithm");
    }
    if (!algorithm_available(alg)) {
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "AEAD algorithm is not available");
    }
    if (nonce_len != expected_nonce ||
        wrapped_len != OYAMA_VAULT_KEY_BYTES + OYAMA_VAULT_TAG_BYTES) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "wrapped DEK has an invalid length");
    }

    rc = aead_decrypt(alg, dek, &plaintext_len, wrapped,
                      (unsigned long long)wrapped_len, aad,
                      (unsigned long long)aad_len, nonce, kek);
    if (rc != 0 || plaintext_len != OYAMA_VAULT_KEY_BYTES) {
        sodium_memzero(dek, OYAMA_VAULT_KEY_BYTES);
        return fail(err, OYAMA_VAULT_ERR_BAD_PASSPHRASE,
                    "recovery phrase does not unwrap the vault key");
    }
    return succeed(err);
}

static void build_record_aad(const unsigned char uuid[16], unsigned char version,
                             unsigned char alg,
                             unsigned char out[OYAMA_VAULT_RECORD_AAD_BYTES]) {
    static const char prefix[] = "oyama-vault-record-v1";
    memcpy(out, prefix, sizeof(prefix) - 1);
    memcpy(out + sizeof(prefix) - 1, uuid, 16);
    out[sizeof(prefix) - 1 + 16] = version;
    out[sizeof(prefix) - 1 + 17] = alg;
}

int oyama_vault_build_meta_aad(
    const unsigned char salt[OYAMA_VAULT_SALT_BYTES], uint64_t opslimit,
    uint64_t memlimit, unsigned char alg,
    unsigned char out[OYAMA_VAULT_META_AAD_MAX], size_t *out_len) {
    static const char prefix[] = "oyama-vault-meta-v1";
    size_t pos = 0;

    if (salt == NULL || out == NULL || out_len == NULL) {
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }
    memcpy(out + pos, prefix, sizeof(prefix) - 1);
    pos += sizeof(prefix) - 1;
    write_u32_be(out + pos, OYAMA_VAULT_META_VERSION);
    pos += 4;
    write_u32_be(out + pos, OYAMA_VAULT_KDF_ARGON2ID13);
    pos += 4;
    memcpy(out + pos, salt, OYAMA_VAULT_SALT_BYTES);
    pos += OYAMA_VAULT_SALT_BYTES;
    write_u64_be(out + pos, opslimit);
    pos += 8;
    write_u64_be(out + pos, memlimit);
    pos += 8;
    out[pos] = alg;
    ++pos;
    *out_len = pos;
    return OYAMA_VAULT_OK;
}

int oyama_vault_seal_record(const unsigned char uuid[16],
                            const unsigned char *envelope,
                            size_t envelope_len,
                            const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                            unsigned char alg, unsigned char **out,
                            size_t *out_len, oyama_error *err) {
    unsigned char aad[OYAMA_VAULT_RECORD_AAD_BYTES];
    unsigned char *blob;
    unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES];
    size_t nl;
    size_t total;
    unsigned long long ciphertext_len = 0;
    int rc;

    if (out == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record seal argument");
    }
    *out = NULL;
    if (out_len == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record seal argument");
    }
    *out_len = 0;
    if (uuid == NULL || envelope == NULL || envelope_len == 0 || dek == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record seal argument");
    }
    nl = nonce_len_for_alg(alg);
    if (nl == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "unknown AEAD algorithm");
    }
    if (!algorithm_available(alg)) {
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "AEAD algorithm is not available");
    }
    if (envelope_len > SIZE_MAX - 4 - nl - OYAMA_VAULT_TAG_BYTES) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record is too large");
    }
    if (oyama_vault_crypto_init() != 0) {
        return fail(err, OYAMA_VAULT_ERR_CRYPTO,
                    "libsodium initialization failed");
    }

    total = 4 + nl + envelope_len + OYAMA_VAULT_TAG_BYTES;
    blob = sodium_malloc(total);
    if (blob == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate record buffer");
    }

    blob[0] = OYAMA_VAULT_RECORD_VERSION;
    blob[1] = alg;
    blob[2] = (unsigned char)nl;
    blob[3] = 0;
    randombytes_buf(nonce, nl);
    memcpy(blob + 4, nonce, nl);
    build_record_aad(uuid, OYAMA_VAULT_RECORD_VERSION, alg, aad);

    rc = aead_encrypt(alg, blob + 4 + nl, &ciphertext_len, envelope,
                      (unsigned long long)envelope_len, aad, sizeof(aad),
                      nonce, dek);
    if (rc != 0 || ciphertext_len != envelope_len + OYAMA_VAULT_TAG_BYTES) {
        sodium_memzero(blob, total);
        sodium_free(blob);
        return fail(err, OYAMA_VAULT_ERR_CRYPTO, "record encryption failed");
    }

    *out = blob;
    *out_len = total;
    sodium_memzero(nonce, sizeof(nonce));
    return succeed(err);
}

int oyama_vault_open_record(const unsigned char uuid[16],
                            const unsigned char *blob, size_t blob_len,
                            const unsigned char dek[OYAMA_VAULT_KEY_BYTES],
                            unsigned char alg, unsigned char **envelope,
                            size_t *envelope_len, oyama_error *err) {
    unsigned char aad[OYAMA_VAULT_RECORD_AAD_BYTES];
    unsigned char *plain;
    size_t nl;
    size_t ciphertext_len;
    size_t plaintext_len = 0;
    unsigned long long decrypted_len = 0;
    int rc;

    if (envelope == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record open argument");
    }
    *envelope = NULL;
    if (envelope_len == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record open argument");
    }
    *envelope_len = 0;
    if (uuid == NULL || blob == NULL || dek == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record open argument");
    }
    if (blob_len < 4) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, "record blob is truncated");
    }
    if (blob[0] != OYAMA_VAULT_RECORD_VERSION) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "unsupported record format version");
    }
    if (blob[1] != alg) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record algorithm does not match vault metadata");
    }
    nl = nonce_len_for_alg(alg);
    if (nl == 0 || blob[2] != nl || blob[3] != 0) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record blob header is invalid");
    }
    if (!algorithm_available(alg)) {
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "AEAD algorithm is not available");
    }
    if (blob_len < 4 + nl + OYAMA_VAULT_TAG_BYTES) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, "record blob is truncated");
    }
    ciphertext_len = blob_len - 4 - nl;
    plaintext_len = ciphertext_len - OYAMA_VAULT_TAG_BYTES;

    plain = sodium_malloc(plaintext_len == 0 ? 1 : plaintext_len);
    if (plain == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate record buffer");
    }
    build_record_aad(uuid, OYAMA_VAULT_RECORD_VERSION, alg, aad);
    rc = aead_decrypt(alg, plain, &decrypted_len, blob + 4 + nl,
                      (unsigned long long)ciphertext_len, aad, sizeof(aad),
                      blob + 4, dek);
    if (rc != 0 || decrypted_len != plaintext_len) {
        sodium_memzero(plain, plaintext_len == 0 ? 1 : plaintext_len);
        sodium_free(plain);
        return fail(err, OYAMA_VAULT_ERR_CRYPTO,
                    "record authentication failed");
    }

    *envelope = plain;
    *envelope_len = plaintext_len;
    return succeed(err);
}

void oyama_vault_secure_free(void *ptr, size_t len) {
    if (ptr == NULL) {
        return;
    }
    sodium_memzero(ptr, len);
    sodium_free(ptr);
}

void oyama_buf_free(oyama_buf *buf) {
    if (buf == NULL) {
        return;
    }
    oyama_vault_secure_free(buf->data, buf->len);
    buf->data = NULL;
    buf->len = 0;
}

void oyama_record_free(oyama_record *record) {
    if (record == NULL) {
        return;
    }
    oyama_buf_free(&record->payload);
    sodium_memzero(record->id, sizeof(record->id));
    sodium_memzero(record->kind, sizeof(record->kind));
    record->updated_at_ms = 0;
}

const char *oyama_vault_status_name(int status) {
    switch (status) {
    case OYAMA_VAULT_OK:
        return "ok";
    case OYAMA_VAULT_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case OYAMA_VAULT_ERR_NOT_INITIALIZED:
        return "not initialized";
    case OYAMA_VAULT_ERR_ALREADY_INITIALIZED:
        return "already initialized";
    case OYAMA_VAULT_ERR_LOCKED:
        return "locked";
    case OYAMA_VAULT_ERR_BAD_PASSPHRASE:
        return "bad passphrase";
    case OYAMA_VAULT_ERR_CRYPTO:
        return "crypto failure";
    case OYAMA_VAULT_ERR_DATABASE:
        return "database failure";
    case OYAMA_VAULT_ERR_CORRUPT:
        return "corrupt data";
    case OYAMA_VAULT_ERR_UNSUPPORTED:
        return "unsupported";
    case OYAMA_VAULT_ERR_NOT_FOUND:
        return "not found";
    case OYAMA_VAULT_ERR_NOMEM:
        return "out of memory";
    case OYAMA_VAULT_ERR_CALLBACK:
        return "callback failure";
    default:
        return "unknown";
    }
}
