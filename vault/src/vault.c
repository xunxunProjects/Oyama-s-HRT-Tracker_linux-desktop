#include "vault_crypto.h"

#include <libpq-fe.h>
#include <sodium.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct oyama_vault {
    PGconn *conn;
    unsigned char *dek;
    uint64_t kdf_opslimit;
    uint64_t kdf_memlimit;
    unsigned char alg;
    int unlocked;
    int in_transaction;
};

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

static int result_error(oyama_vault *vault, PGresult *result, oyama_error *err,
                        const char *what) {
    const char *detail = result != NULL ? PQresultErrorMessage(result)
                                        : PQerrorMessage(vault->conn);
    if (err != NULL) {
        err->code = OYAMA_VAULT_ERR_DATABASE;
        (void)snprintf(err->message, sizeof(err->message), "%s: %s", what,
                       detail);
    }
    if (result != NULL) {
        PQclear(result);
    }
    return OYAMA_VAULT_ERR_DATABASE;
}

static uint64_t now_ms(void) {
    time_t now = time(NULL);
    return now < 0 ? 0 : (uint64_t)now * 1000;
}

static int parse_u64(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0' || out == NULL) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static const char *alg_name(unsigned char alg) {
    if (alg == OYAMA_VAULT_ALG_AES256GCM) {
        return "aes256gcm";
    }
    if (alg == OYAMA_VAULT_ALG_XCHACHA20POLY1305) {
        return "xchacha20poly1305";
    }
    return "";
}

static unsigned char parse_alg(const char *name) {
    if (name != NULL && strcmp(name, "aes256gcm") == 0) {
        return OYAMA_VAULT_ALG_AES256GCM;
    }
    if (name != NULL && strcmp(name, "xchacha20poly1305") == 0) {
        return OYAMA_VAULT_ALG_XCHACHA20POLY1305;
    }
    return 0;
}

static int copy_bytea(PGresult *result, int column, unsigned char *out,
                      size_t expected, oyama_error *err, const char *what) {
    unsigned char *decoded;
    size_t decoded_len = 0;

    if (PQgetisnull(result, 0, column)) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, what);
    }
    decoded = PQunescapeBytea(
        (const unsigned char *)PQgetvalue(result, 0, column), &decoded_len);
    if (decoded == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot decode vault metadata");
    }
    if (decoded_len != expected) {
        PQfreemem(decoded);
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, what);
    }
    memcpy(out, decoded, expected);
    PQfreemem(decoded);
    return OYAMA_VAULT_OK;
}

static int text_field(PGresult *result, int column, const char **out,
                       oyama_error *err, const char *what) {
    if (PQgetisnull(result, 0, column)) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, what);
    }
    *out = PQgetvalue(result, 0, column);
    return OYAMA_VAULT_OK;
}

int oyama_vault_open(const char *conninfo, oyama_vault **out,
                     oyama_error *err) {
    oyama_vault *vault;

    if (out == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "vault output pointer is null");
    }
    *out = NULL;
    if (conninfo == NULL || conninfo[0] == '\0') {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "database connection string is empty");
    }
    if (oyama_vault_crypto_init() != 0) {
        return fail(err, OYAMA_VAULT_ERR_CRYPTO,
                    "libsodium initialization failed");
    }

    vault = calloc(1, sizeof(*vault));
    if (vault == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate vault handle");
    }
    vault->conn = PQconnectdb(conninfo);
    if (vault->conn == NULL) {
        free(vault);
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate database connection");
    }
    if (PQstatus(vault->conn) != CONNECTION_OK) {
        if (err != NULL) {
            err->code = OYAMA_VAULT_ERR_DATABASE;
            (void)snprintf(err->message, sizeof(err->message),
                           "cannot connect to database: %s",
                           PQerrorMessage(vault->conn));
        }
        PQfinish(vault->conn);
        free(vault);
        return OYAMA_VAULT_ERR_DATABASE;
    }

    *out = vault;
    return succeed(err);
}

void oyama_vault_lock(oyama_vault *vault) {
    if (vault == NULL) {
        return;
    }
    if (vault->in_transaction && vault->conn != NULL) {
        PGresult *result = PQexec(vault->conn, "ROLLBACK");
        if (result != NULL) {
            PQclear(result);
        }
        vault->in_transaction = 0;
    }
    if (vault->dek != NULL) {
        sodium_memzero(vault->dek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(vault->dek);
        vault->dek = NULL;
    }
    vault->unlocked = 0;
    vault->alg = 0;
    vault->kdf_opslimit = 0;
    vault->kdf_memlimit = 0;
}

void oyama_vault_close(oyama_vault *vault) {
    if (vault == NULL) {
        return;
    }
    oyama_vault_lock(vault);
    if (vault->conn != NULL) {
        PQfinish(vault->conn);
    }
    sodium_memzero(vault, sizeof(*vault));
    free(vault);
}

static int records_exist(const oyama_vault *vault, int *out,
                         oyama_error *err) {
    PGresult *result = PQexec(vault->conn,
                              "SELECT 1 FROM vault_records LIMIT 1");
    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error((oyama_vault *)vault, result, err,
                            "cannot read vault records");
    }
    *out = PQntuples(result) != 0;
    PQclear(result);
    return OYAMA_VAULT_OK;
}

int oyama_vault_get_state(const oyama_vault *vault, oyama_vault_state *out,
                          oyama_error *err) {
    PGresult *result;
    int rows;
    int orphaned = 0;
    int rc;

    if (vault == NULL || out == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid vault state argument");
    }
    result = PQexec(vault->conn, "SELECT 1 FROM vault_meta LIMIT 1");
    if (result == NULL ||
        PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error((oyama_vault *)vault, result, err,
                            "cannot read vault metadata");
    }
    rows = PQntuples(result);
    PQclear(result);
    if (rows == 0) {
        rc = records_exist(vault, &orphaned, err);
        if (rc != OYAMA_VAULT_OK) {
            return rc;
        }
        if (orphaned) {
            return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                        "vault records exist without metadata");
        }
        *out = OYAMA_VAULT_UNINITIALIZED;
    } else if (vault->unlocked && vault->dek != NULL) {
        *out = OYAMA_VAULT_UNLOCKED;
    } else {
        *out = OYAMA_VAULT_LOCKED;
    }
    return succeed(err);
}

static int check_uninitialized(oyama_vault *vault, oyama_error *err) {
    PGresult *result = PQexec(vault->conn,
                              "SELECT 1 FROM vault_meta LIMIT 1");
    int rows;
    int orphaned = 0;
    int rc;

    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error(vault, result, err,
                            "cannot read vault metadata");
    }
    rows = PQntuples(result);
    PQclear(result);
    if (rows != 0) {
        return fail(err, OYAMA_VAULT_ERR_ALREADY_INITIALIZED,
                    "vault metadata already exists");
    }
    rc = records_exist(vault, &orphaned, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (orphaned) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "vault records exist without metadata");
    }
    return OYAMA_VAULT_OK;
}

int oyama_vault_setup(oyama_vault *vault, const char *phrase,
                      size_t phrase_len, oyama_error *err) {
    const oyama_vault_kdf_params production = {3, 67108864};
    return oyama_vault_setup_with_params(vault, phrase, phrase_len,
                                         &production, err);
}

int oyama_vault_setup_with_params(oyama_vault *vault, const char *phrase,
                                  size_t phrase_len,
                                  const oyama_vault_kdf_params *params,
                                  oyama_error *err) {
    unsigned char *dek = NULL;
    unsigned char *kek = NULL;
    unsigned char salt[OYAMA_VAULT_SALT_BYTES];
    unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES];
    unsigned char wrapped[OYAMA_VAULT_KEY_BYTES + OYAMA_VAULT_TAG_BYTES];
    unsigned char meta_aad[OYAMA_VAULT_META_AAD_MAX];
    size_t nonce_len = 0;
    size_t wrapped_len = 0;
    size_t meta_aad_len = 0;
    unsigned char alg;
    char format_text[16];
    char ops_text[32];
    char mem_text[32];
    char created_text[32];
    char updated_text[32];
    const char *values[10];
    int lengths[10];
    int formats[10];
    PGresult *result;
    int rc;

    if (vault == NULL || phrase == NULL || phrase_len == 0 || params == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid setup argument");
    }
    rc = check_uninitialized(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }

    dek = sodium_malloc(OYAMA_VAULT_KEY_BYTES);
    kek = sodium_malloc(OYAMA_VAULT_KEY_BYTES);
    if (dek == NULL || kek == NULL) {
        if (dek != NULL) {
            sodium_free(dek);
        }
        if (kek != NULL) {
            sodium_free(kek);
        }
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate vault key material");
    }

    alg = crypto_aead_aes256gcm_is_available()
              ? OYAMA_VAULT_ALG_AES256GCM
              : OYAMA_VAULT_ALG_XCHACHA20POLY1305;
    randombytes_buf(dek, OYAMA_VAULT_KEY_BYTES);
    randombytes_buf(salt, sizeof(salt));

    rc = oyama_vault_derive_key(phrase, phrase_len, salt, params->opslimit,
                                params->memlimit, kek, err);
    if (rc != OYAMA_VAULT_OK) {
        goto cleanup;
    }
    (void)oyama_vault_build_meta_aad(salt, params->opslimit,
                                     params->memlimit, alg, meta_aad,
                                     &meta_aad_len);
    rc = oyama_vault_wrap_dek(dek, kek, meta_aad, meta_aad_len, alg, nonce,
                              &nonce_len, wrapped, &wrapped_len, err);
    if (rc != OYAMA_VAULT_OK) {
        goto cleanup;
    }

    (void)snprintf(format_text, sizeof(format_text), "%d",
                   OYAMA_VAULT_META_VERSION);
    (void)snprintf(ops_text, sizeof(ops_text), "%llu",
                   (unsigned long long)params->opslimit);
    (void)snprintf(mem_text, sizeof(mem_text), "%llu",
                   (unsigned long long)params->memlimit);
    (void)snprintf(created_text, sizeof(created_text), "%llu",
                   (unsigned long long)now_ms());
    (void)snprintf(updated_text, sizeof(updated_text), "%s", created_text);

    values[0] = format_text;
    values[1] = "argon2id13";
    values[2] = (const char *)salt;
    values[3] = ops_text;
    values[4] = mem_text;
    values[5] = alg_name(alg);
    values[6] = (const char *)nonce;
    values[7] = (const char *)wrapped;
    values[8] = created_text;
    values[9] = updated_text;
    lengths[0] = (int)strlen(format_text);
    lengths[1] = 10;
    lengths[2] = (int)sizeof(salt);
    lengths[3] = (int)strlen(ops_text);
    lengths[4] = (int)strlen(mem_text);
    lengths[5] = (int)strlen(alg_name(alg));
    lengths[6] = (int)nonce_len;
    lengths[7] = (int)wrapped_len;
    lengths[8] = (int)strlen(created_text);
    lengths[9] = (int)strlen(updated_text);
    formats[0] = 0;
    formats[1] = 0;
    formats[2] = 1;
    formats[3] = 0;
    formats[4] = 0;
    formats[5] = 0;
    formats[6] = 1;
    formats[7] = 1;
    formats[8] = 0;
    formats[9] = 0;

    result = PQexecParams(
        vault->conn,
        "INSERT INTO vault_meta "
        "(id, format_version, kdf_algorithm, kdf_salt, kdf_opslimit, "
        " kdf_memlimit, record_algorithm, wrap_nonce, wrapped_dek, "
        " created_at, updated_at) "
        "VALUES (true, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
        "ON CONFLICT (id) DO NOTHING",
        10, NULL, values, lengths, formats, 0);
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        rc = result_error(vault, result, err,
                          "cannot write vault metadata");
        goto cleanup;
    }
    if (strcmp(PQcmdTuples(result), "1") != 0) {
        PQclear(result);
        rc = fail(err, OYAMA_VAULT_ERR_ALREADY_INITIALIZED,
                  "vault metadata already exists");
        goto cleanup;
    }
    PQclear(result);
    rc = succeed(err);

cleanup:
    if (dek != NULL) {
        sodium_memzero(dek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(dek);
    }
    if (kek != NULL) {
        sodium_memzero(kek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(kek);
    }
    sodium_memzero(salt, sizeof(salt));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(wrapped, sizeof(wrapped));
    sodium_memzero(meta_aad, sizeof(meta_aad));
    return rc;
}

int oyama_vault_unlock(oyama_vault *vault, const char *phrase,
                       size_t phrase_len, oyama_error *err) {
    PGresult *result = NULL;
    unsigned char *kek = NULL;
    unsigned char *dek = NULL;
    unsigned char salt[OYAMA_VAULT_SALT_BYTES];
    unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES];
    unsigned char wrapped[OYAMA_VAULT_KEY_BYTES + OYAMA_VAULT_TAG_BYTES];
    unsigned char meta_aad[OYAMA_VAULT_META_AAD_MAX];
    size_t nonce_len;
    size_t meta_aad_len = 0;
    const char *format_text;
    const char *kdf_algorithm;
    const char *record_algorithm;
    const char *ops_text;
    const char *mem_text;
    uint64_t format_version = 0;
    uint64_t opslimit = 0;
    uint64_t memlimit = 0;
    unsigned char alg;
    int rc;

    if (vault == NULL || phrase == NULL || phrase_len == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid unlock argument");
    }
    oyama_vault_lock(vault);

    result = PQexec(
        vault->conn,
        "SELECT format_version, kdf_algorithm, kdf_salt, kdf_opslimit, "
        "kdf_memlimit, record_algorithm, wrap_nonce, wrapped_dek "
        "FROM vault_meta WHERE id = true");
    if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error(vault, result, err,
                            "cannot read vault metadata");
    }
    if (PQntuples(result) == 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_NOT_INITIALIZED,
                    "vault has not been initialized");
    }

    if (text_field(result, 0, &format_text, err,
                   "vault format version is missing") != OYAMA_VAULT_OK ||
        !parse_u64(format_text, &format_version) ||
        format_version != OYAMA_VAULT_META_VERSION) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "unsupported vault format version");
    }
    if (text_field(result, 1, &kdf_algorithm, err,
                   "vault KDF algorithm is missing") != OYAMA_VAULT_OK ||
        strcmp(kdf_algorithm, "argon2id13") != 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "unsupported vault KDF algorithm");
    }
    if (text_field(result, 3, &ops_text, err,
                   "vault KDF opslimit is missing") != OYAMA_VAULT_OK ||
        !parse_u64(ops_text, &opslimit) ||
        text_field(result, 4, &mem_text, err,
                   "vault KDF memlimit is missing") != OYAMA_VAULT_OK ||
        !parse_u64(mem_text, &memlimit)) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "vault KDF parameters are invalid");
    }
    if (text_field(result, 5, &record_algorithm, err,
                   "vault record algorithm is missing") != OYAMA_VAULT_OK) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "vault record algorithm is missing");
    }
    alg = parse_alg(record_algorithm);
    if (alg == 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "unsupported vault record algorithm");
    }
    if (alg == OYAMA_VAULT_ALG_AES256GCM &&
        !crypto_aead_aes256gcm_is_available()) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_UNSUPPORTED,
                    "AES-256-GCM is not available on this CPU");
    }
    nonce_len = alg == OYAMA_VAULT_ALG_AES256GCM
                    ? OYAMA_VAULT_AES_NONCE_BYTES
                    : OYAMA_VAULT_XCHACHA_NONCE_BYTES;
    if (copy_bytea(result, 2, salt, sizeof(salt), err,
                   "vault salt has an invalid length") != OYAMA_VAULT_OK ||
        copy_bytea(result, 6, nonce, nonce_len, err,
                   "vault wrap nonce has an invalid length") !=
            OYAMA_VAULT_OK ||
        copy_bytea(result, 7, wrapped, sizeof(wrapped), err,
                   "wrapped vault key has an invalid length") !=
            OYAMA_VAULT_OK) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "vault metadata is corrupt");
    }
    PQclear(result);
    result = NULL;

    kek = sodium_malloc(OYAMA_VAULT_KEY_BYTES);
    dek = sodium_malloc(OYAMA_VAULT_KEY_BYTES);
    if (kek == NULL || dek == NULL) {
        if (kek != NULL) {
            sodium_free(kek);
        }
        if (dek != NULL) {
            sodium_free(dek);
        }
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate vault key material");
    }
    rc = oyama_vault_derive_key(phrase, phrase_len, salt, opslimit, memlimit,
                                kek, err);
    if (rc != OYAMA_VAULT_OK) {
        goto unlock_cleanup;
    }
    (void)oyama_vault_build_meta_aad(salt, opslimit, memlimit, alg, meta_aad,
                                     &meta_aad_len);
    rc = oyama_vault_unwrap_dek(wrapped, sizeof(wrapped), kek, meta_aad,
                                meta_aad_len, alg, nonce, nonce_len, dek,
                                err);
    if (rc != OYAMA_VAULT_OK) {
        goto unlock_cleanup;
    }

    vault->dek = dek;
    dek = NULL;
    vault->alg = alg;
    vault->kdf_opslimit = opslimit;
    vault->kdf_memlimit = memlimit;
    vault->unlocked = 1;
    rc = succeed(err);

unlock_cleanup:
    if (kek != NULL) {
        sodium_memzero(kek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(kek);
    }
    if (dek != NULL) {
        sodium_memzero(dek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(dek);
    }
    sodium_memzero(salt, sizeof(salt));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(wrapped, sizeof(wrapped));
    sodium_memzero(meta_aad, sizeof(meta_aad));
    return rc;
}

int oyama_vault_change_phrase(oyama_vault *vault, const char *new_phrase,
                              size_t new_len, oyama_error *err) {
    unsigned char salt[OYAMA_VAULT_SALT_BYTES];
    unsigned char nonce[OYAMA_VAULT_XCHACHA_NONCE_BYTES];
    unsigned char wrapped[OYAMA_VAULT_KEY_BYTES + OYAMA_VAULT_TAG_BYTES];
    unsigned char meta_aad[OYAMA_VAULT_META_AAD_MAX];
    unsigned char *kek = NULL;
    size_t nonce_len = 0;
    size_t wrapped_len = 0;
    size_t meta_aad_len = 0;
    char ops_text[32];
    char mem_text[32];
    char updated_text[32];
    const char *values[7];
    int lengths[7];
    int formats[7];
    PGresult *result;
    int rc;

    if (vault == NULL || new_phrase == NULL || new_len == 0) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid phrase change argument");
    }
    if (!vault->unlocked || vault->dek == NULL) {
        return fail(err, OYAMA_VAULT_ERR_LOCKED, "vault is locked");
    }

    kek = sodium_malloc(OYAMA_VAULT_KEY_BYTES);
    if (kek == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate vault key material");
    }
    randombytes_buf(salt, sizeof(salt));
    rc = oyama_vault_derive_key(new_phrase, new_len, salt,
                                vault->kdf_opslimit, vault->kdf_memlimit,
                                kek, err);
    if (rc != OYAMA_VAULT_OK) {
        goto change_cleanup;
    }
    (void)oyama_vault_build_meta_aad(salt, vault->kdf_opslimit,
                                     vault->kdf_memlimit, vault->alg,
                                     meta_aad, &meta_aad_len);
    rc = oyama_vault_wrap_dek(vault->dek, kek, meta_aad, meta_aad_len,
                              vault->alg, nonce, &nonce_len, wrapped,
                              &wrapped_len, err);
    if (rc != OYAMA_VAULT_OK) {
        goto change_cleanup;
    }

    (void)snprintf(ops_text, sizeof(ops_text), "%llu",
                   (unsigned long long)vault->kdf_opslimit);
    (void)snprintf(mem_text, sizeof(mem_text), "%llu",
                   (unsigned long long)vault->kdf_memlimit);
    (void)snprintf(updated_text, sizeof(updated_text), "%llu",
                   (unsigned long long)now_ms());
    values[0] = (const char *)salt;
    values[1] = ops_text;
    values[2] = mem_text;
    values[3] = alg_name(vault->alg);
    values[4] = (const char *)nonce;
    values[5] = (const char *)wrapped;
    values[6] = updated_text;
    lengths[0] = (int)sizeof(salt);
    lengths[1] = (int)strlen(ops_text);
    lengths[2] = (int)strlen(mem_text);
    lengths[3] = (int)strlen(alg_name(vault->alg));
    lengths[4] = (int)nonce_len;
    lengths[5] = (int)wrapped_len;
    lengths[6] = (int)strlen(updated_text);
    formats[0] = 1;
    formats[1] = 0;
    formats[2] = 0;
    formats[3] = 0;
    formats[4] = 1;
    formats[5] = 1;
    formats[6] = 0;

    result = PQexecParams(
        vault->conn,
        "UPDATE vault_meta SET kdf_salt = $1, kdf_opslimit = $2, "
        "kdf_memlimit = $3, record_algorithm = $4, wrap_nonce = $5, "
        "wrapped_dek = $6, updated_at = $7 WHERE id = true",
        7, NULL, values, lengths, formats, 0);
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK) {
        rc = result_error(vault, result, err,
                          "cannot update vault metadata");
        goto change_cleanup;
    }
    if (strcmp(PQcmdTuples(result), "1") != 0) {
        PQclear(result);
        rc = fail(err, OYAMA_VAULT_ERR_NOT_INITIALIZED,
                  "vault metadata disappeared");
        goto change_cleanup;
    }
    PQclear(result);
    rc = succeed(err);

change_cleanup:
    if (kek != NULL) {
        sodium_memzero(kek, OYAMA_VAULT_KEY_BYTES);
        sodium_free(kek);
    }
    sodium_memzero(salt, sizeof(salt));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(wrapped, sizeof(wrapped));
    sodium_memzero(meta_aad, sizeof(meta_aad));
    return rc;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int parse_uuid(const char *text, unsigned char out[16]) {
    size_t i;
    size_t byte_index = 0;

    if (text == NULL || strlen(text) != 36) {
        return 0;
    }
    i = 0;
    while (i < 36) {
        int hi;
        int lo;
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') {
                return 0;
            }
            ++i;
            continue;
        }
        if (i + 1 >= 36 || byte_index >= 16) {
            return 0;
        }
        hi = hex_value(text[i]);
        lo = hex_value(text[i + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[byte_index++] = (unsigned char)((hi << 4) | lo);
        i += 2;
    }
    return byte_index == 16;
}

static void format_uuid(const unsigned char in[16], char out[37]) {
    static const char hex[] = "0123456789abcdef";
    size_t i;
    size_t pos = 0;

    for (i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out[pos++] = '-';
        }
        out[pos++] = hex[in[i] >> 4];
        out[pos++] = hex[in[i] & 0x0f];
    }
    out[pos] = '\0';
}

static void put_u32_be(unsigned char *out, uint32_t value) {
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

static void put_u64_be(unsigned char *out, uint64_t value) {
    out[0] = (unsigned char)(value >> 56);
    out[1] = (unsigned char)(value >> 48);
    out[2] = (unsigned char)(value >> 40);
    out[3] = (unsigned char)(value >> 32);
    out[4] = (unsigned char)(value >> 24);
    out[5] = (unsigned char)(value >> 16);
    out[6] = (unsigned char)(value >> 8);
    out[7] = (unsigned char)value;
}

static uint32_t get_u32_be(const unsigned char *in) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

static uint64_t get_u64_be(const unsigned char *in) {
    return ((uint64_t)in[0] << 56) | ((uint64_t)in[1] << 48) |
           ((uint64_t)in[2] << 40) | ((uint64_t)in[3] << 32) |
           ((uint64_t)in[4] << 24) | ((uint64_t)in[5] << 16) |
           ((uint64_t)in[6] << 8) | (uint64_t)in[7];
}

static int require_unlocked(oyama_vault *vault, oyama_error *err) {
    if (vault == NULL || vault->conn == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "vault handle is not open");
    }
    if (!vault->unlocked || vault->dek == NULL) {
        return fail(err, OYAMA_VAULT_ERR_LOCKED, "vault is locked");
    }
    return OYAMA_VAULT_OK;
}

static int build_envelope(const char *kind, uint64_t updated_at_ms,
                          const void *payload, size_t payload_len,
                          unsigned char **out, size_t *out_len,
                          oyama_error *err) {
    size_t kind_len;
    size_t total;
    unsigned char *envelope;
    size_t pos = 0;

    if (out == NULL || out_len == NULL || kind == NULL ||
        (payload == NULL && payload_len != 0)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record envelope argument");
    }
    *out = NULL;
    *out_len = 0;
    kind_len = strlen(kind);
    if (kind_len == 0 || kind_len > 63 || payload_len > UINT32_MAX ||
        kind_len > UINT32_MAX) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record kind or payload is invalid");
    }
    if (payload_len > SIZE_MAX - 4 - kind_len - 8 - 4) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record is too large");
    }
    total = 4 + kind_len + 8 + 4 + payload_len;
    envelope = sodium_malloc(total);
    if (envelope == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate record envelope");
    }
    put_u32_be(envelope + pos, (uint32_t)kind_len);
    pos += 4;
    memcpy(envelope + pos, kind, kind_len);
    pos += kind_len;
    put_u64_be(envelope + pos, updated_at_ms);
    pos += 8;
    put_u32_be(envelope + pos, (uint32_t)payload_len);
    pos += 4;
    if (payload_len != 0) {
        memcpy(envelope + pos, payload, payload_len);
    }
    *out = envelope;
    *out_len = total;
    return succeed(err);
}

static int parse_envelope(const unsigned char *envelope, size_t envelope_len,
                          const char **kind, size_t *kind_len,
                          uint64_t *updated_at_ms,
                          const unsigned char **payload, size_t *payload_len,
                          oyama_error *err) {
    size_t pos;
    uint32_t klen;
    uint32_t plen;
    size_t expected;

    if (envelope == NULL || kind == NULL || kind_len == NULL ||
        updated_at_ms == NULL || payload == NULL || payload_len == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid record envelope");
    }
    if (envelope_len < 4 + 8 + 4) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record envelope is truncated");
    }
    klen = get_u32_be(envelope);
    if (klen == 0 || klen > 63) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record kind length is invalid");
    }
    if (envelope_len < 4 + (size_t)klen + 8 + 4) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record envelope is truncated");
    }
    if (memchr(envelope + 4, '\0', klen) != NULL) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record kind contains NUL");
    }
    pos = 4 + (size_t)klen;
    *kind = (const char *)(envelope + 4);
    *kind_len = klen;
    *updated_at_ms = get_u64_be(envelope + pos);
    pos += 8;
    plen = get_u32_be(envelope + pos);
    pos += 4;
    expected = pos + (size_t)plen;
    if (expected != envelope_len) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record envelope length is invalid");
    }
    *payload = envelope + pos;
    *payload_len = plen;
    return succeed(err);
}

static int read_bytea(PGresult *result, int row, int column,
                      unsigned char **out, size_t *out_len, oyama_error *err,
                      const char *what) {
    const char *text;
    unsigned char *decoded;
    size_t decoded_len = 0;

    if (out == NULL || out_len == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "invalid bytea argument");
    }
    *out = NULL;
    *out_len = 0;
    if (PQgetisnull(result, row, column)) {
        return fail(err, OYAMA_VAULT_ERR_CORRUPT, what);
    }
    text = PQgetvalue(result, row, column);
    decoded = PQunescapeBytea((const unsigned char *)text, &decoded_len);
    if (decoded == NULL) {
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot decode database blob");
    }
    *out = decoded;
    *out_len = decoded_len;
    return OYAMA_VAULT_OK;
}

int oyama_vault_put(oyama_vault *vault, const char *id, const char *kind,
                    uint64_t updated_at_ms, const void *payload,
                    size_t payload_len, oyama_error *err) {
    unsigned char uuid[16];
    char canonical[37];
    unsigned char *envelope = NULL;
    size_t envelope_len = 0;
    unsigned char *blob = NULL;
    size_t blob_len = 0;
    PGresult *result;
    const char *values[2];
    int lengths[2];
    int formats[2];
    int rc;

    rc = require_unlocked(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (id == NULL || kind == NULL || (payload == NULL && payload_len != 0) ||
        !parse_uuid(id, uuid)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record id, kind, or payload is invalid");
    }
    if (payload_len > INT_MAX) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record payload is too large");
    }
    format_uuid(uuid, canonical);
    rc = build_envelope(kind, updated_at_ms, payload, payload_len, &envelope,
                        &envelope_len, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    rc = oyama_vault_seal_record(uuid, envelope, envelope_len, vault->dek,
                                 vault->alg, &blob, &blob_len, err);
    sodium_memzero(envelope, envelope_len);
    sodium_free(envelope);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (blob_len > INT_MAX) {
        oyama_vault_secure_free(blob, blob_len);
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "encrypted record is too large");
    }

    values[0] = canonical;
    values[1] = (const char *)blob;
    lengths[0] = 0;
    lengths[1] = (int)blob_len;
    formats[0] = 0;
    formats[1] = 1;
    result = PQexecParams(
        vault->conn,
        "INSERT INTO vault_records(id, blob) VALUES($1::uuid, $2) "
        "ON CONFLICT (id) DO UPDATE SET blob = EXCLUDED.blob",
        2, NULL, values, lengths, formats, 0);
    if (result == NULL) {
        oyama_vault_secure_free(blob, blob_len);
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        rc = result_error(vault, result, err, "cannot store vault record");
    } else {
        rc = succeed(err);
        PQclear(result);
    }
    oyama_vault_secure_free(blob, blob_len);
    return rc;
}

int oyama_vault_get(oyama_vault *vault, const char *id, oyama_record *out,
                    oyama_error *err) {
    unsigned char uuid[16];
    char canonical[37];
    const char *values[1];
    PGresult *result;
    unsigned char *blob = NULL;
    size_t blob_len = 0;
    unsigned char *envelope = NULL;
    size_t envelope_len = 0;
    const char *kind = NULL;
    size_t kind_len = 0;
    uint64_t updated_at_ms = 0;
    const unsigned char *payload = NULL;
    size_t payload_len = 0;
    unsigned char *payload_copy;
    int rc;

    if (out == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record output is required");
    }
    memset(out, 0, sizeof(*out));
    rc = require_unlocked(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (id == NULL || !parse_uuid(id, uuid)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record id is invalid");
    }
    format_uuid(uuid, canonical);
    values[0] = canonical;
    result = PQexecParams(vault->conn,
                          "SELECT blob FROM vault_records WHERE id = $1::uuid",
                          1, NULL, values, NULL, NULL, 0);
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error(vault, result, err, "cannot read vault record");
    }
    if (PQntuples(result) == 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_NOT_FOUND, "record not found");
    }
    if (PQntuples(result) != 1) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record id is not unique");
    }
    rc = read_bytea(result, 0, 0, &blob, &blob_len, err,
                    "record blob is null");
    PQclear(result);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    rc = oyama_vault_open_record(uuid, blob, blob_len, vault->dek, vault->alg,
                                 &envelope, &envelope_len, err);
    PQfreemem(blob);
    if (rc != OYAMA_VAULT_OK) {
        if (rc == OYAMA_VAULT_ERR_NOMEM) {
            return rc;
        }
        return fail(err, OYAMA_VAULT_ERR_CORRUPT,
                    "record authentication failed");
    }
    rc = parse_envelope(envelope, envelope_len, &kind, &kind_len,
                        &updated_at_ms, &payload, &payload_len, err);
    if (rc != OYAMA_VAULT_OK) {
        sodium_memzero(envelope, envelope_len);
        sodium_free(envelope);
        return rc;
    }
    payload_copy = sodium_malloc(payload_len == 0 ? 1 : payload_len);
    if (payload_copy == NULL) {
        sodium_memzero(envelope, envelope_len);
        sodium_free(envelope);
        return fail(err, OYAMA_VAULT_ERR_NOMEM,
                    "cannot allocate record payload");
    }
    if (payload_len != 0) {
        memcpy(payload_copy, payload, payload_len);
    }
    memcpy(out->id, canonical, sizeof(out->id));
    memcpy(out->kind, kind, kind_len);
    out->kind[kind_len] = '\0';
    out->updated_at_ms = updated_at_ms;
    out->payload.data = payload_copy;
    out->payload.len = payload_len;
    sodium_memzero(envelope, envelope_len);
    sodium_free(envelope);
    return succeed(err);
}

int oyama_vault_remove(oyama_vault *vault, const char *id, oyama_error *err) {
    unsigned char uuid[16];
    char canonical[37];
    const char *values[1];
    PGresult *result;
    char *tuples;
    int rc;

    rc = require_unlocked(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (id == NULL || !parse_uuid(id, uuid)) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "record id is invalid");
    }
    format_uuid(uuid, canonical);
    values[0] = canonical;
    result = PQexecParams(vault->conn,
                          "DELETE FROM vault_records WHERE id = $1::uuid", 1,
                          NULL, values, NULL, NULL, 0);
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        return result_error(vault, result, err, "cannot remove vault record");
    }
    tuples = PQcmdTuples(result);
    if (tuples == NULL || strcmp(tuples, "0") == 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_NOT_FOUND, "record not found");
    }
    if (strcmp(tuples, "1") != 0) {
        PQclear(result);
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    "unexpected delete count");
    }
    PQclear(result);
    return succeed(err);
}

int oyama_vault_scan(oyama_vault *vault, const char *kind_or_null,
                     oyama_vault_scan_cb callback, void *user_data,
                     oyama_error *err) {
    PGresult *result;
    int rows;
    int i;
    int rc = OYAMA_VAULT_OK;

    rc = require_unlocked(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    if (callback == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "scan callback is required");
    }
    result = PQexec(vault->conn,
                    "SELECT id, blob FROM vault_records ORDER BY id");
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        return result_error(vault, result, err, "cannot scan vault records");
    }
    rows = PQntuples(result);
    for (i = 0; i < rows; ++i) {
        const char *id_text = PQgetvalue(result, i, 0);
        unsigned char uuid[16];
        unsigned char *blob = NULL;
        size_t blob_len = 0;
        unsigned char *envelope = NULL;
        size_t envelope_len = 0;
        const char *kind = NULL;
        size_t kind_len = 0;
        uint64_t updated_at_ms = 0;
        const unsigned char *payload = NULL;
        size_t payload_len = 0;
        char kind_buffer[64];
        int matches;
        int stop;

        if (!parse_uuid(id_text, uuid)) {
            fail(err, OYAMA_VAULT_ERR_CORRUPT,
                 "vault record has an invalid UUID");
            rc = OYAMA_VAULT_ERR_CORRUPT;
            break;
        }
        rc = read_bytea(result, i, 1, &blob, &blob_len, err,
                        "record blob is null");
        if (rc != OYAMA_VAULT_OK) {
            break;
        }
        rc = oyama_vault_open_record(uuid, blob, blob_len, vault->dek,
                                     vault->alg, &envelope, &envelope_len,
                                     err);
        PQfreemem(blob);
        if (rc != OYAMA_VAULT_OK) {
            if (rc != OYAMA_VAULT_ERR_NOMEM) {
                fail(err, OYAMA_VAULT_ERR_CORRUPT,
                     "record authentication failed");
                rc = OYAMA_VAULT_ERR_CORRUPT;
            }
            break;
        }
        rc = parse_envelope(envelope, envelope_len, &kind, &kind_len,
                            &updated_at_ms, &payload, &payload_len, err);
        if (rc != OYAMA_VAULT_OK) {
            sodium_memzero(envelope, envelope_len);
            sodium_free(envelope);
            break;
        }
        matches = kind_or_null == NULL ||
                  (strlen(kind_or_null) == kind_len &&
                   memcmp(kind_or_null, kind, kind_len) == 0);
        stop = 0;
        if (matches) {
            memcpy(kind_buffer, kind, kind_len);
            kind_buffer[kind_len] = '\0';
            stop = callback(user_data, id_text, kind_buffer, updated_at_ms,
                            payload, payload_len);
        }
        sodium_memzero(envelope, envelope_len);
        sodium_free(envelope);
        if (stop != 0) {
            break;
        }
    }
    PQclear(result);
    return rc;
}

int oyama_vault_clear(oyama_vault *vault, oyama_error *err) {
    PGresult *result;
    int rc;

    rc = require_unlocked(vault, err);
    if (rc != OYAMA_VAULT_OK) {
        return rc;
    }
    result = PQexec(vault->conn, "DELETE FROM vault_records");
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        return result_error(vault, result, err, "cannot clear vault records");
    }
    PQclear(result);
    return succeed(err);
}

int oyama_vault_begin(oyama_vault *vault, oyama_error *err) {
    PGresult *result;

    if (vault == NULL || vault->conn == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "vault handle is not open");
    }
    if (vault->in_transaction) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "transaction already open");
    }
    result = PQexec(vault->conn, "BEGIN");
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        return result_error(vault, result, err, "cannot begin transaction");
    }
    PQclear(result);
    vault->in_transaction = 1;
    return succeed(err);
}

int oyama_vault_commit(oyama_vault *vault, oyama_error *err) {
    PGresult *result;
    int rc;

    if (vault == NULL || vault->conn == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "vault handle is not open");
    }
    if (!vault->in_transaction) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "no transaction is open");
    }
    result = PQexec(vault->conn, "COMMIT");
    vault->in_transaction = 0;
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        rc = result_error(vault, result, err, "cannot commit transaction");
        return rc;
    }
    PQclear(result);
    return succeed(err);
}

int oyama_vault_rollback(oyama_vault *vault, oyama_error *err) {
    PGresult *result;

    if (vault == NULL || vault->conn == NULL) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "vault handle is not open");
    }
    if (!vault->in_transaction) {
        return fail(err, OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                    "no transaction is open");
    }
    result = PQexec(vault->conn, "ROLLBACK");
    vault->in_transaction = 0;
    if (result == NULL) {
        return fail(err, OYAMA_VAULT_ERR_DATABASE,
                    PQerrorMessage(vault->conn));
    }
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        return result_error(vault, result, err, "cannot roll back transaction");
    }
    PQclear(result);
    return succeed(err);
}
