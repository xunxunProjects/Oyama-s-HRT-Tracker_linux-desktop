#ifndef OYAMA_VAULT_H
#define OYAMA_VAULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oyama_vault oyama_vault;

typedef struct oyama_error {
    int  code;
    char message[256];
} oyama_error;

typedef enum oyama_vault_status {
    OYAMA_VAULT_OK = 0,
    OYAMA_VAULT_ERR_INVALID_ARGUMENT = 1,
    OYAMA_VAULT_ERR_NOT_INITIALIZED = 2,
    OYAMA_VAULT_ERR_ALREADY_INITIALIZED = 3,
    OYAMA_VAULT_ERR_LOCKED = 4,
    OYAMA_VAULT_ERR_BAD_PASSPHRASE = 5,
    OYAMA_VAULT_ERR_CRYPTO = 6,
    OYAMA_VAULT_ERR_DATABASE = 7,
    OYAMA_VAULT_ERR_CORRUPT = 8,
    OYAMA_VAULT_ERR_UNSUPPORTED = 9,
    OYAMA_VAULT_ERR_NOT_FOUND = 10,
    OYAMA_VAULT_ERR_NOMEM = 11,
    OYAMA_VAULT_ERR_CALLBACK = 12
} oyama_vault_status;

typedef enum oyama_vault_state {
    OYAMA_VAULT_UNINITIALIZED = 0,
    OYAMA_VAULT_LOCKED = 1,
    OYAMA_VAULT_UNLOCKED = 2
} oyama_vault_state;

typedef struct oyama_vault_kdf_params {
    uint64_t opslimit;
    uint64_t memlimit;
} oyama_vault_kdf_params;

typedef struct oyama_buf {
    unsigned char *data;
    size_t         len;
} oyama_buf;

typedef struct oyama_record {
    char       id[37];
    char       kind[64];
    uint64_t   updated_at_ms;
    oyama_buf  payload;
} oyama_record;

typedef int (*oyama_vault_scan_cb)(
    void *user_data,
    const char *id,
    const char *kind,
    uint64_t updated_at_ms,
    const unsigned char *payload,
    size_t payload_len);

int oyama_vault_open(const char *conninfo, oyama_vault **out,
                     oyama_error *err);
void oyama_vault_close(oyama_vault *vault);

int oyama_vault_get_state(const oyama_vault *vault, oyama_vault_state *out,
                          oyama_error *err);

int oyama_vault_setup(oyama_vault *vault, const char *phrase,
                      size_t phrase_len, oyama_error *err);
int oyama_vault_setup_with_params(oyama_vault *vault, const char *phrase,
                                  size_t phrase_len,
                                  const oyama_vault_kdf_params *params,
                                  oyama_error *err);
int oyama_vault_unlock(oyama_vault *vault, const char *phrase,
                       size_t phrase_len, oyama_error *err);
int oyama_vault_change_phrase(oyama_vault *vault, const char *new_phrase,
                              size_t new_len, oyama_error *err);
void oyama_vault_lock(oyama_vault *vault);

int oyama_vault_put(oyama_vault *vault, const char *id, const char *kind,
                    uint64_t updated_at_ms, const void *payload,
                    size_t payload_len, oyama_error *err);
int oyama_vault_get(oyama_vault *vault, const char *id, oyama_record *out,
                    oyama_error *err);
int oyama_vault_remove(oyama_vault *vault, const char *id,
                       oyama_error *err);
int oyama_vault_scan(oyama_vault *vault, const char *kind_or_null,
                     oyama_vault_scan_cb callback, void *user_data,
                     oyama_error *err);
int oyama_vault_clear(oyama_vault *vault, oyama_error *err);

int oyama_vault_begin(oyama_vault *vault, oyama_error *err);
int oyama_vault_commit(oyama_vault *vault, oyama_error *err);
int oyama_vault_rollback(oyama_vault *vault, oyama_error *err);

void oyama_buf_free(oyama_buf *buf);
void oyama_record_free(oyama_record *record);
const char *oyama_vault_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif
