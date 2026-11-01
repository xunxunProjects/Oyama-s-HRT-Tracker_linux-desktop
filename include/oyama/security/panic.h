#ifndef OYAMA_SECURITY_PANIC_H
#define OYAMA_SECURITY_PANIC_H

#include "oyama/vault.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*oyama_panic_cb)(void *user_data);

typedef struct oyama_panic oyama_panic;

int oyama_panic_create(oyama_panic **out, oyama_error *err);
void oyama_panic_set_callback(oyama_panic *panic, oyama_panic_cb cb,
                              void *user_data);
int oyama_panic_set_hotkey(oyama_panic *panic, const char *accelerator);
int oyama_panic_start(oyama_panic *panic);
int oyama_panic_stop(oyama_panic *panic);
void oyama_panic_destroy(oyama_panic *panic);

/* Test-only injection (null backend). */
void oyama_panic_test_inject(oyama_panic *panic);

#ifdef __cplusplus
}
#endif

#endif
