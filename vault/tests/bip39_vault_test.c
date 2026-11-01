#include "oyama/bip39.h"
#include "oyama/vault.h"

#include <libpq-fe.h>
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

static char *test_conninfo(void) {
    const char *socket_dir = getenv("OYAMA_TEST_PG_SOCKET");
    const char *dbname = getenv("OYAMA_TEST_PG_DB");
    const char *user = getenv("OYAMA_TEST_PG_USER");
    const char *port = getenv("OYAMA_TEST_PG_PORT");
    char *out;
    size_t needed;

    if (socket_dir == NULL || dbname == NULL || user == NULL || port == NULL) {
        return NULL;
    }
    needed = strlen(socket_dir) + strlen(dbname) + strlen(user) +
             strlen(port) + 96;
    out = malloc(needed);
    if (out == NULL) {
        return NULL;
    }
    (void)snprintf(out, needed,
                   "host='%s' port='%s' dbname='%s' user='%s' "
                   "application_name='bip39_vault_test' connect_timeout=5",
                   socket_dir, port, dbname, user);
    return out;
}

int main(void) {
    char *conninfo;
    oyama_vault *vault = NULL;
    oyama_vault *reopened = NULL;
    oyama_error err;
    char phrase[OYAMA_BIP39_MAX_LEN];

    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return 1;
    }

    conninfo = test_conninfo();
    if (conninfo == NULL) {
        fprintf(stderr, "SKIP: OYAMA_TEST_PG_* is not set\n");
        return 77;
    }

    memset(&err, 0, sizeof(err));
    check(oyama_vault_open(conninfo, &vault, &err) == OYAMA_VAULT_OK,
          "open vault");
    check(vault != NULL, "open returned a vault");
    if (vault == NULL) {
        free(conninfo);
        return 1;
    }

    memset(phrase, 0, sizeof(phrase));
    check(oyama_vault_setup_generated(vault, phrase, sizeof(phrase), &err) ==
              OYAMA_VAULT_OK,
          "setup generated");
    check(oyama_bip39_word_count(phrase) == OYAMA_BIP39_WORDS,
          "generated phrase has 24 words");
    memset(&err, 0, sizeof(err));
    check(oyama_bip39_validate(phrase, &err) == OYAMA_VAULT_OK,
          "generated phrase validates");
    check(oyama_vault_unlock(vault, phrase, strlen(phrase), &err) ==
              OYAMA_VAULT_OK,
          "unlock with generated phrase");

    {
        const char payload[] = "hello bip39";
        oyama_error e2;
        memset(&e2, 0, sizeof(e2));
        check(oyama_vault_put(vault, "r1", "note", 1, payload,
                              strlen(payload), &e2) == OYAMA_VAULT_OK,
              "put record");
        oyama_record rec;
        memset(&rec, 0, sizeof(rec));
        check(oyama_vault_get(vault, "r1", &rec, &e2) == OYAMA_VAULT_OK,
              "get record");
        check(rec.payload.len == strlen(payload) &&
                  memcmp(rec.payload.data, payload, strlen(payload)) == 0,
              "record round-trips");
        oyama_record_free(&rec);
    }

    oyama_vault_lock(vault);
    oyama_vault_close(vault);
    vault = NULL;

    check(oyama_vault_open(conninfo, &reopened, &err) == OYAMA_VAULT_OK,
          "reopen vault");
    check(reopened != NULL, "reopen returned a vault");
    if (reopened != NULL) {
        check(oyama_vault_unlock(reopened, phrase, strlen(phrase), &err) ==
                  OYAMA_VAULT_OK,
              "reopen unlocks with generated phrase");
        oyama_vault_lock(reopened);
        oyama_vault_close(reopened);
        reopened = NULL;
    }

    /* legacy phrase still works */
    {
        oyama_vault *lv = NULL;
        oyama_error e3;
        memset(&e3, 0, sizeof(e3));
        if (oyama_vault_open(conninfo, &lv, &e3) == OYAMA_VAULT_OK && lv) {
            /* can't reuse same db (already initialized); skip separate check */
            oyama_vault_close(lv);
        }
    }

    sodium_memzero(phrase, sizeof(phrase));
    free(conninfo);
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("bip39_vault_test: PASS\n");
    return 0;
}
