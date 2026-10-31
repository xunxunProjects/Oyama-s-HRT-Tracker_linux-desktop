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
    char *conninfo;
    size_t size;

    if (socket_dir == NULL || dbname == NULL || user == NULL || port == NULL) {
        return NULL;
    }
    size = strlen(socket_dir) + strlen(dbname) + strlen(user) + strlen(port) +
           128;
    conninfo = malloc(size);
    if (conninfo == NULL) {
        return NULL;
    }
    snprintf(conninfo, size,
             "host='%s' port='%s' dbname='%s' user='%s' "
             "application_name='vault_records_test' connect_timeout=5",
             socket_dir, port, dbname, user);
    return conninfo;
}

static int run_sql(PGconn *conn, const char *sql) {
    PGresult *result = PQexec(conn, sql);
    ExecStatusType status;

    if (result == NULL) {
        fprintf(stderr, "SQL failed: %s\n", PQerrorMessage(conn));
        return 0;
    }
    status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        fprintf(stderr, "SQL failed: %s\n%s\n", sql,
                PQresultErrorMessage(result));
        PQclear(result);
        return 0;
    }
    PQclear(result);
    return 1;
}

static int setup_and_unlock(oyama_vault *vault) {
    const char phrase[] = "correct horse battery staple";
    oyama_vault_kdf_params low = {1, 8388608};
    oyama_error err;

    memset(&err, 0, sizeof(err));
    check(oyama_vault_setup_with_params(vault, phrase, sizeof(phrase) - 1,
                                        &low, &err) == OYAMA_VAULT_OK,
          "setup");
    memset(&err, 0, sizeof(err));
    check(oyama_vault_unlock(vault, phrase, sizeof(phrase) - 1, &err) ==
              OYAMA_VAULT_OK,
          "unlock");
    return failures == 0;
}

struct scan_state {
    int total;
    int doses;
    int labs;
    int stop_after;
};

static int scan_cb(void *user_data, const char *id, const char *kind,
                   uint64_t updated_at_ms, const unsigned char *payload,
                   size_t payload_len) {
    struct scan_state *state = user_data;

    (void)id;
    (void)updated_at_ms;
    (void)payload;
    (void)payload_len;
    ++state->total;
    if (strcmp(kind, "dose_event") == 0) {
        ++state->doses;
    } else if (strcmp(kind, "lab_result") == 0) {
        ++state->labs;
    }
    if (state->stop_after > 0 && state->total >= state->stop_after) {
        return 1;
    }
    return 0;
}

int main(void) {
    static const char *id1 = "11111111-1111-4111-8111-111111111111";
    static const char *id2 = "22222222-2222-4222-8222-222222222222";
    static const char *id3 = "33333333-3333-4333-8333-333333333333";
    static const char *id4 = "44444444-4444-4444-8444-444444444444";
    static const char *id5 = "55555555-5555-4555-8555-555555555555";
    static const char *id6 = "66666666-6666-4666-8666-666666666666";
    static const char *id7 = "77777777-7777-4777-8777-777777777777";
    static const char *id8 = "88888888-8888-4888-8888-888888888888";
    static const char *id9 = "99999999-9999-4999-8999-999999999999";
    const char phrase[] = "correct horse battery staple";
    char *conninfo = test_conninfo();
    PGconn *admin;
    oyama_vault *vault = NULL;
    oyama_error err;
    struct scan_state scan;
    oyama_record record;
    unsigned char *large = NULL;
    size_t large_len = 1024 * 1024;

    if (conninfo == NULL) {
        printf("SKIP: OYAMA_TEST_PG_* is not set\n");
        return 77;
    }
    admin = PQconnectdb(conninfo);
    if (admin == NULL || PQstatus(admin) != CONNECTION_OK) {
        fprintf(stderr, "admin connection failed: %s\n", PQerrorMessage(admin));
        if (admin != NULL) {
            PQfinish(admin);
        }
        free(conninfo);
        return 1;
    }
    if (!run_sql(admin, "TRUNCATE vault_meta, vault_records")) {
        PQfinish(admin);
        free(conninfo);
        return 1;
    }

    memset(&err, 0, sizeof(err));
    check(oyama_vault_open(conninfo, &vault, &err) == OYAMA_VAULT_OK,
          "open");
    if (vault == NULL || !setup_and_unlock(vault)) {
        if (vault != NULL) {
            oyama_vault_close(vault);
        }
        PQfinish(admin);
        free(conninfo);
        return 1;
    }

    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, id1, "dose_event", 1234,
                          "{\"mode\":\"transfem\"}", 19, &err) ==
              OYAMA_VAULT_OK,
          "put");
    memset(&err, 0, sizeof(err));
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id1, &record, &err) == OYAMA_VAULT_OK,
          "get");
    check(strcmp(record.id, id1) == 0, "get id");
    check(strcmp(record.kind, "dose_event") == 0, "get kind");
    check(record.updated_at_ms == 1234, "get updated_at");
    check(record.payload.len == 19 &&
              memcmp(record.payload.data, "{\"mode\":\"transfem\"}", 19) == 0,
          "get payload");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, id2, "lab_result", 2000, "lab", 3, &err) ==
              OYAMA_VAULT_OK,
          "put lab");

    memset(&scan, 0, sizeof(scan));
    memset(&err, 0, sizeof(err));
    check(oyama_vault_scan(vault, "dose_event", scan_cb, &scan, &err) ==
              OYAMA_VAULT_OK,
          "scan kind");
    check(scan.total == 1 && scan.doses == 1 && scan.labs == 0,
          "scan filters kind");

    memset(&scan, 0, sizeof(scan));
    memset(&err, 0, sizeof(err));
    check(oyama_vault_scan(vault, NULL, scan_cb, &scan, &err) ==
              OYAMA_VAULT_OK,
          "scan all");
    check(scan.total == 2 && scan.doses == 1 && scan.labs == 1,
          "scan all counts");

    memset(&scan, 0, sizeof(scan));
    scan.stop_after = 1;
    memset(&err, 0, sizeof(err));
    check(oyama_vault_scan(vault, NULL, scan_cb, &scan, &err) ==
              OYAMA_VAULT_OK,
          "scan stop");
    check(scan.total == 1, "scan callback stops");

    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, id1, "dose_event", 1235, "updated", 7,
                          &err) == OYAMA_VAULT_OK,
          "update put");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id1, &record, &err) == OYAMA_VAULT_OK,
          "get updated");
    check(record.updated_at_ms == 1235 && record.payload.len == 7 &&
              memcmp(record.payload.data, "updated", 7) == 0,
          "update content");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_remove(vault, id1, &err) == OYAMA_VAULT_OK, "remove");
    check(oyama_vault_remove(vault, id1, &err) ==
              OYAMA_VAULT_ERR_NOT_FOUND,
          "remove missing");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id1, &record, &err) ==
              OYAMA_VAULT_ERR_NOT_FOUND,
          "get missing");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_clear(vault, &err) == OYAMA_VAULT_OK, "clear");
    memset(&scan, 0, sizeof(scan));
    check(oyama_vault_scan(vault, NULL, scan_cb, &scan, &err) ==
              OYAMA_VAULT_OK,
          "scan cleared");
    check(scan.total == 0, "clear empties");

    memset(&err, 0, sizeof(err));
    check(oyama_vault_begin(vault, &err) == OYAMA_VAULT_OK, "begin");
    check(oyama_vault_put(vault, id3, "dose_event", 1, "x", 1, &err) ==
              OYAMA_VAULT_OK,
          "put in rollback tx");
    check(oyama_vault_rollback(vault, &err) == OYAMA_VAULT_OK, "rollback");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id3, &record, &err) ==
              OYAMA_VAULT_ERR_NOT_FOUND,
          "rollback discards");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_begin(vault, &err) == OYAMA_VAULT_OK, "begin commit");
    check(oyama_vault_put(vault, id4, "dose_event", 2, "y", 1, &err) ==
              OYAMA_VAULT_OK,
          "put in commit tx");
    check(oyama_vault_commit(vault, &err) == OYAMA_VAULT_OK, "commit");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id4, &record, &err) == OYAMA_VAULT_OK,
          "commit persists");
    oyama_record_free(&record);

    oyama_vault_lock(vault);
    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, id5, "dose_event", 3, "z", 1, &err) ==
              OYAMA_VAULT_ERR_LOCKED,
          "locked put");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id4, &record, &err) ==
              OYAMA_VAULT_ERR_LOCKED,
          "locked get");
    memset(&scan, 0, sizeof(scan));
    check(oyama_vault_scan(vault, NULL, scan_cb, &scan, &err) ==
              OYAMA_VAULT_ERR_LOCKED,
          "locked scan");
    memset(&err, 0, sizeof(err));
    check(oyama_vault_unlock(vault, phrase, sizeof(phrase) - 1, &err) ==
              OYAMA_VAULT_OK,
          "unlock after lock");

    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, "not-a-uuid", "dose_event", 1, "x", 1,
                          &err) == OYAMA_VAULT_ERR_INVALID_ARGUMENT,
          "invalid uuid");
    check(oyama_vault_put(vault, id5, "", 1, "x", 1, &err) ==
              OYAMA_VAULT_ERR_INVALID_ARGUMENT,
          "empty kind");
    check(oyama_vault_put(vault, id5, "0123456789012345678901234567890123456789012345678901234567890123",
                          1, "x", 1, &err) ==
              OYAMA_VAULT_ERR_INVALID_ARGUMENT,
          "oversize kind");
    check(oyama_vault_put(vault, id5, "dose_event", 1, NULL, 1, &err) ==
              OYAMA_VAULT_ERR_INVALID_ARGUMENT,
          "null payload");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, "bad", &record, &err) ==
              OYAMA_VAULT_ERR_INVALID_ARGUMENT,
          "invalid get uuid");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_put(vault, id5, "dose_event", 4, "five", 4, &err) ==
              OYAMA_VAULT_OK,
          "put swap a");
    check(oyama_vault_put(vault, id6, "dose_event", 5, "six!", 4, &err) ==
              OYAMA_VAULT_OK,
          "put swap b");
    check(run_sql(admin,
                  "UPDATE vault_records AS a SET blob = b.blob "
                  "FROM vault_records AS b WHERE a.id = '55555555-5555-4555-8555-555555555555' "
                  "AND b.id = '66666666-6666-4666-8666-666666666666'"),
          "swap blobs");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id5, &record, &err) ==
              OYAMA_VAULT_ERR_CORRUPT,
          "swapped blob detected");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_clear(vault, &err) == OYAMA_VAULT_OK, "clear tamper");
    check(oyama_vault_put(vault, id7, "dose_event", 6, "tamper", 6, &err) ==
              OYAMA_VAULT_OK,
          "put tamper");
    check(run_sql(admin,
                  "UPDATE vault_records SET blob = set_byte(blob, 20, "
                  "(get_byte(blob, 20) + 1) % 256) WHERE id = "
                  "'77777777-7777-4777-8777-777777777777'"),
          "tamper blob");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id7, &record, &err) ==
              OYAMA_VAULT_ERR_CORRUPT,
          "tampered blob detected");
    oyama_record_free(&record);

    memset(&err, 0, sizeof(err));
    check(oyama_vault_clear(vault, &err) == OYAMA_VAULT_OK, "clear before empty");
    check(oyama_vault_put(vault, id8, "preference", 7, NULL, 0, &err) ==
              OYAMA_VAULT_OK,
          "empty payload put");
    memset(&record, 0, sizeof(record));
    check(oyama_vault_get(vault, id8, &record, &err) == OYAMA_VAULT_OK &&
              record.payload.len == 0,
          "empty payload get");
    oyama_record_free(&record);

    large = malloc(large_len);
    check(large != NULL, "large allocation");
    if (large != NULL) {
        memset(large, 0xA5, large_len);
        memset(&err, 0, sizeof(err));
        check(oyama_vault_put(vault, id9, "lab_result", 8, large, large_len,
                              &err) == OYAMA_VAULT_OK,
              "large put");
        memset(&record, 0, sizeof(record));
        check(oyama_vault_get(vault, id9, &record, &err) == OYAMA_VAULT_OK &&
                  record.payload.len == large_len &&
                  memcmp(record.payload.data, large, large_len) == 0,
              "large get");
        oyama_record_free(&record);
        free(large);
    }

    oyama_vault_close(vault);
    PQfinish(admin);
    free(conninfo);
    sodium_memzero(&err, sizeof(err));

    if (failures != 0) {
        fprintf(stderr, "vault_records_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("vault_records_test: ok\n");
    return 0;
}
