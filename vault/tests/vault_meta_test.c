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
                   "application_name='vault_meta_test' connect_timeout=5",
                   socket_dir, port, dbname, user);
    return out;
}

static int run_sql(PGconn *conn, const char *sql) {
    PGresult *result = PQexec(conn, sql);
    ExecStatusType status;
    int ok;

    if (result == NULL) {
        fprintf(stderr, "SQL failed: %s\n%s\n", sql, PQerrorMessage(conn));
        return 0;
    }
    status = PQresultStatus(result);
    ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    if (!ok) {
        fprintf(stderr, "SQL failed: %s\n%s\n", sql, PQresultErrorMessage(result));
    }
    PQclear(result);
    return ok;
}

static int state_is(oyama_vault *vault, oyama_vault_state expected,
                    const char *what) {
    oyama_error err;
    oyama_vault_state actual = OYAMA_VAULT_UNINITIALIZED;
    int rc;

    memset(&err, 0, sizeof(err));
    rc = oyama_vault_get_state(vault, &actual, &err);
    check(rc == OYAMA_VAULT_OK && actual == expected, what);
    return rc == OYAMA_VAULT_OK && actual == expected;
}

int main(void) {
    char *conninfo = test_conninfo();
    PGconn *admin = NULL;
    oyama_vault *vault = NULL;
    oyama_vault *reopened = NULL;
    oyama_error err;
    oyama_vault_kdf_params low = {1, 8388608};

    if (conninfo == NULL) {
        fprintf(stderr, "SKIP: OYAMA_TEST_PG_* is not set\n");
        return 77;
    }

    admin = PQconnectdb(conninfo);
    if (admin == NULL || PQstatus(admin) != CONNECTION_OK) {
        fprintf(stderr, "cannot connect to test database: %s\n",
                admin == NULL ? "out of memory" : PQerrorMessage(admin));
        if (admin != NULL) {
            PQfinish(admin);
        }
        free(conninfo);
        return 1;
    }

    memset(&err, 0, sizeof(err));
    check(oyama_vault_open(conninfo, &vault, &err) == OYAMA_VAULT_OK,
          "open vault");
    check(vault != NULL, "open returned a vault");
    if (vault == NULL) {
        PQfinish(admin);
        free(conninfo);
        return 1;
    }

    check(state_is(vault, OYAMA_VAULT_UNINITIALIZED, "fresh vault"),
          "fresh vault state");
    check(oyama_vault_setup_with_params(
              vault, "correct horse battery staple", 28, &low,
              &err) == OYAMA_VAULT_OK,
          "setup with low KDF parameters");
    check(state_is(vault, OYAMA_VAULT_LOCKED, "setup leaves vault locked"),
          "setup locked state");
    check(oyama_vault_setup(vault, "again", 5, &err) ==
              OYAMA_VAULT_ERR_ALREADY_INITIALIZED,
          "duplicate setup is rejected");
    check(oyama_vault_unlock(vault, "wrong", 5, &err) ==
              OYAMA_VAULT_ERR_BAD_PASSPHRASE,
          "wrong phrase is rejected");
    check(oyama_vault_unlock(vault, "correct horse battery staple", 28,
                             &err) == OYAMA_VAULT_OK,
          "correct phrase unlocks");
    check(state_is(vault, OYAMA_VAULT_UNLOCKED, "unlocked state"),
          "unlocked state");

    oyama_vault_lock(vault);
    check(state_is(vault, OYAMA_VAULT_LOCKED, "lock returns to locked"),
          "locked after lock");
    oyama_vault_close(vault);
    vault = NULL;

    check(oyama_vault_open(conninfo, &reopened, &err) == OYAMA_VAULT_OK,
          "reopen vault");
    check(reopened != NULL, "reopen returned a vault");
    if (reopened == NULL) {
        PQfinish(admin);
        free(conninfo);
        return 1;
    }
    check(state_is(reopened, OYAMA_VAULT_LOCKED, "reopen is locked"),
          "reopen locked state");
    check(oyama_vault_unlock(reopened, "correct horse battery staple", 28,
                             &err) == OYAMA_VAULT_OK,
          "reopen unlocks");

    check(oyama_vault_change_phrase(reopened, "new phrase", 10, &err) ==
              OYAMA_VAULT_OK,
          "change phrase");
    oyama_vault_lock(reopened);
    check(oyama_vault_unlock(reopened, "correct horse battery staple", 28,
                             &err) == OYAMA_VAULT_ERR_BAD_PASSPHRASE,
          "old phrase is rejected after rotation");
    check(oyama_vault_unlock(reopened, "new phrase", 10, &err) ==
              OYAMA_VAULT_OK,
          "new phrase unlocks");

    oyama_vault_lock(reopened);
    check(run_sql(admin,
                  "UPDATE vault_meta SET wrapped_dek = "
                  "set_byte(wrapped_dek, 0, "
                  "get_byte(wrapped_dek, 0) # 255)"),
          "tamper wrapped_dek");
    check(oyama_vault_unlock(reopened, "new phrase", 10, &err) !=
              OYAMA_VAULT_OK,
          "tampered wrapped DEK is rejected");
    check(run_sql(admin,
                  "UPDATE vault_meta SET wrapped_dek = "
                  "set_byte(wrapped_dek, 0, "
                  "get_byte(wrapped_dek, 0) # 255)"),
          "restore wrapped_dek");

    check(run_sql(admin,
                  "UPDATE vault_meta SET kdf_salt = "
                  "set_byte(kdf_salt, 0, get_byte(kdf_salt, 0) # 255)"),
          "tamper kdf_salt");
    check(oyama_vault_unlock(reopened, "new phrase", 10, &err) !=
              OYAMA_VAULT_OK,
          "tampered KDF salt is rejected");
    check(run_sql(admin,
                  "UPDATE vault_meta SET kdf_salt = "
                  "set_byte(kdf_salt, 0, get_byte(kdf_salt, 0) # 255)"),
          "restore kdf_salt");
    check(oyama_vault_unlock(reopened, "new phrase", 10, &err) ==
              OYAMA_VAULT_OK,
          "restored metadata unlocks");

    check(oyama_vault_put(reopened,
                          "00000000-0000-0000-0000-000000000009",
                          "dose_event", 1, "{}", 2, &err) ==
              OYAMA_VAULT_OK,
          "orphan record fixture");
    oyama_vault_lock(reopened);
    check(run_sql(admin, "DELETE FROM vault_meta"), "delete metadata");
    oyama_vault_state orphan_state = OYAMA_VAULT_UNINITIALIZED;
    check(oyama_vault_get_state(reopened, &orphan_state, &err) !=
              OYAMA_VAULT_OK &&
              err.code == OYAMA_VAULT_ERR_CORRUPT,
          "records without metadata are corrupt");
    check(oyama_vault_setup(reopened, "new phrase", 10, &err) !=
              OYAMA_VAULT_OK,
          "setup refuses records without metadata");

    oyama_vault_lock(NULL);
    oyama_vault_close(NULL);
    oyama_vault_close(reopened);
    PQfinish(admin);
    free(conninfo);
    sodium_memzero(&err, sizeof(err));

    if (failures != 0) {
        fprintf(stderr, "vault_meta_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("vault_meta_test: ok\n");
    return 0;
}
