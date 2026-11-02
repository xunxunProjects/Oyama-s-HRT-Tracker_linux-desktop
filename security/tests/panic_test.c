#include "oyama/security/panic.h"

#include "oyama/vault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int injected = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static void cb(void *user_data) {
    (void)user_data;
    ++injected;
}

int main(void) {
    oyama_panic *p = NULL;
    oyama_error err;
    memset(&err, 0, sizeof(err));

    check(oyama_panic_create(&p, &err) == OYAMA_VAULT_OK, "create");
    check(p != NULL, "create returns non-null");
    if (p == NULL) {
        return 1;
    }

    oyama_panic_set_callback(p, cb, NULL);

    /* invalid hotkey rejected */
    check(oyama_panic_set_hotkey(p, "") != OYAMA_VAULT_OK,
          "empty hotkey rejected");
    check(oyama_panic_set_hotkey(p, "ctrl+alt+p") == OYAMA_VAULT_OK,
          "valid hotkey accepted");

    /* idempotent start/stop */
    check(oyama_panic_start(p) == OYAMA_VAULT_OK, "start");
    check(oyama_panic_start(p) == OYAMA_VAULT_OK, "start idempotent");
    check(oyama_panic_stop(p) == OYAMA_VAULT_OK, "stop");
    check(oyama_panic_stop(p) == OYAMA_VAULT_OK, "stop idempotent");

    /* inject fires callback */
    injected = 0;
    oyama_panic_start(p);
    oyama_panic_test_inject(p);
    check(injected == 1, "inject fires callback");
    oyama_panic_stop(p);

    oyama_panic_destroy(p);

    /* destroy NULL safe */
    oyama_panic_destroy(NULL);

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("panic_test: PASS\n");
    return 0;
}
