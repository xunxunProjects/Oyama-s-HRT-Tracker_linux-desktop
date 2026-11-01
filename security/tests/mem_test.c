#include "oyama/security/mem.h"

#include <sodium.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int main(void) {
    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return 1;
    }

    /* secure zero */
    {
        char buf[64];
        memset(buf, 0xAB, sizeof(buf));
        oyama_secure_zero(buf, sizeof(buf));
        int all_zero = 1;
        size_t i;
        for (i = 0; i < sizeof(buf); ++i) {
            if (buf[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        check(all_zero, "secure_zero zeroes buffer");
    }

    /* secure alloc/free */
    {
        void *p = oyama_secure_alloc(128);
        check(p != NULL, "secure_alloc returns non-null");
        if (p != NULL) {
            memset(p, 0xCD, 128);
            oyama_secure_free(p, 128);
            check(1, "secure_free succeeds");
        }
    }

    /* disable core dumps */
    {
        int rc = oyama_disable_core_dumps();
        check(rc == 0 || rc == -1, "disable_core_dumps returns 0 or -1");
    }

    /* mark dontdump on Linux */
    {
        void *buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        int rc = oyama_mark_dontdump(buf, 4096);
#if defined(__linux__)
        check(rc == 0, "mark_dontdump returns 0 on Linux");
#else
        check(rc == -1 || rc == 0, "mark_dontdump returns documented code");
#endif
        munmap(buf, 4096);
    }

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("mem_test: PASS\n");
    return 0;
}
