#include "process_hardening.h"

#include <cstdio>
#include <sys/resource.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

int main() {
    oyama::harden_process_for_secrets();

    struct rlimit core {};
    if (::getrlimit(RLIMIT_CORE, &core) != 0) {
        std::perror("getrlimit(RLIMIT_CORE)");
        return 1;
    }
    if (core.rlim_cur != 0 || core.rlim_max != 0) {
        std::fprintf(stderr,
                     "FAIL: core limit was not disabled (soft=%llu hard=%llu)\n",
                     static_cast<unsigned long long>(core.rlim_cur),
                     static_cast<unsigned long long>(core.rlim_max));
        return 1;
    }

#if defined(__linux__)
    const int dumpable = ::prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (dumpable != 0) {
        std::fprintf(stderr,
                     "FAIL: process is still dumpable (prctl=%d)\n",
                     dumpable);
        return 1;
    }
#endif

    std::puts("process_hardening_test: ok");
    return 0;
}
