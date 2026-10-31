#include "vault_crypto.h"

#include "oyama/vault.h"

#include <sodium.h>

#include <stdio.h>
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

static int algorithm_available(unsigned char alg) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return crypto_aead_aes256gcm_is_available() == 1;
    }
    return alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305;
}

static void fill_bytes(unsigned char *out, size_t len, unsigned char start) {
    size_t i;
    for (i = 0; i < len; ++i) {
        out[i] = (unsigned char)(start + i);
    }
}

static void test_derive(void) {
    const unsigned char salt[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                    8, 9, 10, 11, 12, 13, 14, 15};
    unsigned char k1[32];
    unsigned char k2[32];
    oyama_error err;
    const char phrase[] = "alpha beta gamma";
    const size_t phrase_len = sizeof(phrase) - 1;

    clear_error(&err);
    check(oyama_vault_crypto_init() == 0, "crypto init");
    check(oyama_vault_derive_key(phrase, phrase_len, salt, 1, 8388608, k1,
                                 &err) == OYAMA_VAULT_OK,
          "derive");
    clear_error(&err);
    check(oyama_vault_derive_key(phrase, phrase_len, salt, 1, 8388608, k2,
                                 &err) == OYAMA_VAULT_OK,
          "derive again");
    check(sodium_memcmp(k1, k2, sizeof(k1)) == 0, "KDF deterministic");

    clear_error(&err);
    check(oyama_vault_derive_key("alpha beta delta", 16, salt, 1, 8388608,
                                 k2, &err) == OYAMA_VAULT_OK,
          "derive other phrase");
    check(sodium_memcmp(k1, k2, sizeof(k1)) != 0,
          "different phrase differs");

    sodium_memzero(k1, sizeof(k1));
    sodium_memzero(k2, sizeof(k2));
}

static void test_wrap(unsigned char alg) {
    unsigned char dek[32];
    unsigned char kek[32];
    unsigned char other_kek[32];
    unsigned char nonce[24];
    unsigned char wrapped[64];
    unsigned char opened[32];
    unsigned char aad[] = "oyama-vault-meta-test";
    unsigned char bad_aad[sizeof(aad)];
    size_t nonce_len = 0;
    size_t wrapped_len = 0;
    size_t expected_nonce = alg == OYAMA_VAULT_ALG_AES256GCM ? 12u : 24u;
    oyama_error err;

    if (!algorithm_available(alg)) {
        return;
    }

    fill_bytes(dek, sizeof(dek), 1);
    fill_bytes(kek, sizeof(kek), 33);
    memcpy(other_kek, kek, sizeof(other_kek));
    other_kek[0] ^= 0x01;
    memcpy(bad_aad, aad, sizeof(bad_aad));
    bad_aad[3] ^= 0x01;

    clear_error(&err);
    check(oyama_vault_wrap_dek(dek, kek, aad, sizeof(aad) - 1, alg, nonce,
                               &nonce_len, wrapped, &wrapped_len,
                               &err) == OYAMA_VAULT_OK,
          "wrap DEK");
    check(nonce_len == expected_nonce, "wrap nonce length");
    check(wrapped_len == sizeof(dek) + crypto_aead_aes256gcm_ABYTES,
          "wrap length");

    clear_error(&err);
    check(oyama_vault_unwrap_dek(wrapped, wrapped_len, kek, aad,
                                 sizeof(aad) - 1, alg, nonce, nonce_len,
                                 opened, &err) == OYAMA_VAULT_OK,
          "unwrap DEK");
    check(sodium_memcmp(dek, opened, sizeof(dek)) == 0, "unwrap content");

    clear_error(&err);
    check(oyama_vault_unwrap_dek(wrapped, wrapped_len, other_kek, aad,
                                 sizeof(aad) - 1, alg, nonce, nonce_len,
                                 opened, &err) ==
              OYAMA_VAULT_ERR_BAD_PASSPHRASE,
          "wrong KEK rejected");

    clear_error(&err);
    check(oyama_vault_unwrap_dek(wrapped, wrapped_len, kek, bad_aad,
                                 sizeof(bad_aad) - 1, alg, nonce, nonce_len,
                                 opened, &err) ==
              OYAMA_VAULT_ERR_BAD_PASSPHRASE,
          "wrong AAD rejected");

    sodium_memzero(dek, sizeof(dek));
    sodium_memzero(kek, sizeof(kek));
    sodium_memzero(other_kek, sizeof(other_kek));
    sodium_memzero(opened, sizeof(opened));
}

static void test_record(unsigned char alg) {
    unsigned char uuid[16];
    unsigned char other_uuid[16];
    unsigned char dek[32];
    unsigned char envelope[97];
    unsigned char *blob = NULL;
    unsigned char *opened = NULL;
    size_t blob_len = 0;
    size_t opened_len = 0;
    size_t i;
    oyama_error err;

    if (!algorithm_available(alg)) {
        return;
    }

    fill_bytes(uuid, sizeof(uuid), 10);
    memcpy(other_uuid, uuid, sizeof(other_uuid));
    other_uuid[7] ^= 0x01;
    fill_bytes(dek, sizeof(dek), 70);
    fill_bytes(envelope, sizeof(envelope), 17);

    clear_error(&err);
    check(oyama_vault_seal_record(uuid, envelope, sizeof(envelope), dek, alg,
                                  &blob, &blob_len, &err) == OYAMA_VAULT_OK,
          "seal record");
    check(blob != NULL, "seal returns blob");
    check(blob_len == 4 + (alg == OYAMA_VAULT_ALG_AES256GCM ? 12u : 24u) +
                          sizeof(envelope) + 16,
          "seal length");
    check(blob != NULL && blob[0] == OYAMA_VAULT_RECORD_VERSION &&
              blob[1] == alg && blob[3] == 0,
          "blob header");

    clear_error(&err);
    check(oyama_vault_open_record(uuid, blob, blob_len, dek, alg, &opened,
                                  &opened_len, &err) == OYAMA_VAULT_OK,
          "open record");
    check(opened_len == sizeof(envelope) &&
              memcmp(opened, envelope, sizeof(envelope)) == 0,
          "record round-trip");
    if (opened != NULL) {
        oyama_vault_secure_free(opened, opened_len);
        opened = NULL;
    }

    clear_error(&err);
    check(oyama_vault_open_record(other_uuid, blob, blob_len, dek, alg,
                                  &opened, &opened_len, &err) !=
              OYAMA_VAULT_OK,
          "record UUID is authenticated");
    opened = NULL;

    for (i = 4; i < blob_len; ++i) {
        blob[i] ^= 0x01;
        clear_error(&err);
        check(oyama_vault_open_record(uuid, blob, blob_len, dek, alg, &opened,
                                      &opened_len, &err) != OYAMA_VAULT_OK,
              "record tamper rejected");
        blob[i] ^= 0x01;
        if (opened != NULL) {
            oyama_vault_secure_free(opened, opened_len);
            opened = NULL;
        }
    }

    if (blob != NULL) {
        oyama_vault_secure_free(blob, blob_len);
    }
    sodium_memzero(dek, sizeof(dek));
}

static void test_status_and_free(void) {
    int status;
    oyama_buf buf;
    oyama_record record;
    const char *name;

    for (status = OYAMA_VAULT_OK; status <= OYAMA_VAULT_ERR_CALLBACK;
         ++status) {
        name = oyama_vault_status_name(status);
        check(name != NULL && name[0] != '\0', "status name");
    }

    memset(&buf, 0, sizeof(buf));
    oyama_buf_free(&buf);
    oyama_buf_free(&buf);

    memset(&record, 0, sizeof(record));
    oyama_record_free(&record);
    oyama_record_free(&record);
}

int main(void) {
    test_derive();
    test_wrap(OYAMA_VAULT_ALG_AES256GCM);
    test_wrap(OYAMA_VAULT_ALG_XCHACHA20POLY1305);
    test_record(OYAMA_VAULT_ALG_AES256GCM);
    test_record(OYAMA_VAULT_ALG_XCHACHA20POLY1305);
    test_status_and_free();

    if (failures != 0) {
        fprintf(stderr, "vault_crypto_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("vault_crypto_test: ok\n");
    return 0;
}
