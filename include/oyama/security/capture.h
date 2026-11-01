#ifndef OYAMA_SECURITY_CAPTURE_H
#define OYAMA_SECURITY_CAPTURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*oyama_capture_cb)(const char *tool, int pid, void *user_data);

typedef struct oyama_capture_guard oyama_capture_guard;

int oyama_capture_guard_create(oyama_capture_guard **out, void *user_data,
                               oyama_capture_cb cb, void *err);
int oyama_capture_guard_start(oyama_capture_guard *guard);
int oyama_capture_guard_stop(oyama_capture_guard *guard);
void oyama_capture_guard_destroy(oyama_capture_guard *guard);
int oyama_capture_guard_is_best_effort(oyama_capture_guard *guard);

/* Test-only injection. */
void oyama_capture_guard_test_inject(oyama_capture_guard *guard,
                                    const char *tool, int pid);

/* Environment hook for tests: override the proc root. */
void oyama_capture_guard_set_proc_root(oyama_capture_guard *guard,
                                      const char *root);

#ifdef __cplusplus
}
#endif

#endif
