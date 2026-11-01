#define _GNU_SOURCE

#include "oyama/security/panic.h"

#include "oyama/vault.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(OYAMA_PANIC_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <pthread.h>
#include <unistd.h>
#endif

struct oyama_panic {
    oyama_panic_cb cb;
    void *user_data;
    char hotkey[64];
    int started;
#if defined(OYAMA_PANIC_X11)
    Display *display;
    Window root;
    int keycode;
    unsigned int modifiers;
    pthread_t thread;
    int thread_running;
    int stop_flag;
#endif
};

int oyama_panic_create(oyama_panic **out, oyama_error *err) {
    oyama_panic *p;
    if (out == NULL) {
        if (err != NULL) {
            err->code = OYAMA_VAULT_ERR_INVALID_ARGUMENT;
            snprintf(err->message, sizeof(err->message), "invalid panic argument");
        }
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
        if (err != NULL) {
            err->code = OYAMA_VAULT_ERR_NOMEM;
            snprintf(err->message, sizeof(err->message), "out of memory");
        }
        return OYAMA_VAULT_ERR_NOMEM;
    }
    *out = p;
    if (err != NULL) {
        err->code = OYAMA_VAULT_OK;
        err->message[0] = '\0';
    }
    return OYAMA_VAULT_OK;
}

void oyama_panic_set_callback(oyama_panic *panic, oyama_panic_cb cb,
                              void *user_data) {
    if (panic == NULL) {
        return;
    }
    panic->cb = cb;
    panic->user_data = user_data;
}

int oyama_panic_set_hotkey(oyama_panic *panic, const char *accelerator) {
    if (panic == NULL || accelerator == NULL || accelerator[0] == '\0') {
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }
    snprintf(panic->hotkey, sizeof(panic->hotkey), "%s", accelerator);
    return OYAMA_VAULT_OK;
}

#if defined(OYAMA_PANIC_X11)
static void *panic_thread(void *arg) {
    oyama_panic *p = arg;
    XEvent ev;
    while (!p->stop_flag) {
        if (XPending(p->display) > 0) {
            XNextEvent(p->display, &ev);
            if (ev.type == KeyPress) {
                if (p->cb != NULL) {
                    p->cb(p->user_data);
                }
            }
        } else {
            usleep(10000);
        }
    }
    return NULL;
}
#endif

int oyama_panic_start(oyama_panic *panic) {
    if (panic == NULL) {
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }
#if defined(OYAMA_PANIC_X11)
    panic->display = XOpenDisplay(NULL);
    if (panic->display == NULL) {
        return OYAMA_VAULT_ERR_UNSUPPORTED;
    }
    panic->root = DefaultRootWindow(panic->display);
    panic->keycode = 0; /* would parse hotkey; simplified */
    panic->modifiers = AnyModifier;
    if (panic->keycode != 0) {
        XGrabKey(panic->display, panic->keycode, panic->modifiers, panic->root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    panic->stop_flag = 0;
    panic->thread_running = 1;
    if (pthread_create(&panic->thread, NULL, panic_thread, panic) != 0) {
        panic->thread_running = 0;
        XCloseDisplay(panic->display);
        panic->display = NULL;
        return OYAMA_VAULT_ERR_CALLBACK;
    }
#endif
    panic->started = 1;
    return OYAMA_VAULT_OK;
}

int oyama_panic_stop(oyama_panic *panic) {
    if (panic == NULL) {
        return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    }
#if defined(OYAMA_PANIC_X11)
    if (panic->thread_running) {
        panic->stop_flag = 1;
        pthread_join(panic->thread, NULL);
        panic->thread_running = 0;
    }
    if (panic->display != NULL) {
        if (panic->keycode != 0) {
            XUngrabKey(panic->display, panic->keycode, panic->modifiers,
                       panic->root);
        }
        XCloseDisplay(panic->display);
        panic->display = NULL;
    }
#endif
    panic->started = 0;
    return OYAMA_VAULT_OK;
}

void oyama_panic_destroy(oyama_panic *panic) {
    if (panic == NULL) {
        return;
    }
    oyama_panic_stop(panic);
    free(panic);
}

void oyama_panic_test_inject(oyama_panic *panic) {
    if (panic == NULL || panic->cb == NULL) {
        return;
    }
    panic->cb(panic->user_data);
}
