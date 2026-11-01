#include "process_hardening.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/resource.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace oyama {
namespace {

void warn_failure(const char* operation) {
    const int error = errno;
    std::fprintf(stderr, "warning: %s failed: %s\n", operation,
                 std::strerror(error));
}

} // namespace

void harden_process_for_secrets() {
    struct rlimit core_limit {};
    core_limit.rlim_cur = 0;
    core_limit.rlim_max = 0;
    if (::setrlimit(RLIMIT_CORE, &core_limit) != 0) {
        warn_failure("setrlimit(RLIMIT_CORE)");
    }

#if defined(__linux__)
    if (::prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
        warn_failure("prctl(PR_SET_DUMPABLE)");
    }
#endif
}

} // namespace oyama
