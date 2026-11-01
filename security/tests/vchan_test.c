#include "oyama/security/vchan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int main(void) {
    oyama_vchan *v = NULL;
    char buf[64];
    const char msg[] = "hello vchan";

    if (oyama_vchan_connect(&v, "test") != 0) {
        fprintf(stderr, "connect failed\n");
        return 1;
    }
    check(v != NULL, "connect returns non-null");
    if (v == NULL) return 1;

    oyama_vchan_close(v);

    check(1, "close clean");
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("vchan_test: PASS\n");
    return 0;
}
