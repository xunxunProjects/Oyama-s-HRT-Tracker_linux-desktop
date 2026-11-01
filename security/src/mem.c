#define _GNU_SOURCE

#include "oyama/security/mem.h"

#include <sodium.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__linux__)
#include <malloc.h>
#include <sys/prctl.h>
#endif

void oyama_secure_zero(void *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return;
    }
    sodium_memzero(ptr, len);
}

void *oyama_secure_alloc(size_t len) {
    size_t total;
    void *ptr;
    if (len == 0) {
        return NULL;
    }
    total = len + (size_t)sysconf(_SC_PAGESIZE);
    ptr = mmap(NULL, total, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    /* mlock; fail closed on failure. */
    if (mlock(ptr, len) != 0) {
        munmap(ptr, total);
        return NULL;
    }
    return ptr;
}

void oyama_secure_free(void *ptr, size_t len) {
    size_t total;
    size_t page;
    if (ptr == NULL) {
        return;
    }
    page = (size_t)sysconf(_SC_PAGESIZE);
    total = len + page;
    oyama_secure_zero(ptr, len);
    munlock(ptr, len);
    munmap(ptr, total);
}

int oyama_mlock(const void *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    return mlock(ptr, len);
}

int oyama_munlock(const void *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
    return munlock(ptr, len);
}

int oyama_disable_core_dumps(void) {
    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &rl) != 0) {
        return -1;
    }
#if defined(__linux__)
    if (prctl(PR_SET_DUMPABLE, 0) != 0) {
        return -1;
    }
#endif
    return 0;
}

int oyama_mark_dontdump(void *ptr, size_t len) {
    if (ptr == NULL || len == 0) {
        return -1;
    }
#if defined(MADV_DONTDUMP)
    if (madvise(ptr, len, MADV_DONTDUMP) != 0) {
        return -1;
    }
    return 0;
#else
    (void)ptr;
    (void)len;
    return -1;
#endif
}

int oyama_release_free_pages(void) {
#if defined(__linux__)
    malloc_trim(0);
#endif
    return 0;
}
