#define _GNU_SOURCE

#include "oyama/security/capture.h"

#include "oyama/vault.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_TOOLS "gnome-screenshot,spectacle,flameshot,scrot,maim,grim,slurp,ksnip,shutter"

struct oyama_capture_guard {
    oyama_capture_cb cb;
    void *user_data;
    char proc_root[4096];
    pthread_t thread;
    int thread_running;
    int stop_flag;
    int last_pid;
};

static int name_in_list(const char *name, const char *list) {
    /* comma-separated list */
    char buf[4096];
    char *tok;
    snprintf(buf, sizeof(buf), "%s", list);
    tok = strtok(buf, ",");
    while (tok != NULL) {
        while (*tok == ' ') tok++;
        if (strcmp(tok, name) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

static int is_screenshot_tool(const char *comm) {
    const char *extra = getenv("OYAMA_CAPTURE_TOOLS");
    if (name_in_list(comm, DEFAULT_TOOLS)) return 1;
    if (extra != NULL && name_in_list(comm, extra)) return 1;
    return 0;
}

static void poll_once(oyama_capture_guard *g) {
    DIR *d = opendir(g->proc_root);
    struct dirent *ent;
    int found = 0;
    int found_pid = 0;
    char tool[256] = {0};

    if (d == NULL) return;
    while ((ent = readdir(d)) != NULL) {
        char path[8192];
        char comm[256];
        FILE *f;
        int pid;
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        pid = atoi(ent->d_name);
        if (pid <= 0) continue;
        snprintf(path, sizeof(path), "%s/%s/comm", g->proc_root, ent->d_name);
        f = fopen(path, "r");
        if (f == NULL) continue;
        if (fgets(comm, sizeof(comm), f) == NULL) {
            fclose(f);
            continue;
        }
        fclose(f);
        /* strip newline */
        {
            char *nl = strchr(comm, '\n');
            if (nl) *nl = '\0';
        }
        if (is_screenshot_tool(comm)) {
            found = 1;
            found_pid = pid;
            snprintf(tool, sizeof(tool), "%s", comm);
            break;
        }
    }
    closedir(d);

    if (found && g->last_pid != found_pid) {
        g->last_pid = found_pid;
        if (g->cb) g->cb(tool, found_pid, g->user_data);
    } else if (!found) {
        g->last_pid = 0;
    }
}

static void *capture_thread(void *arg) {
    oyama_capture_guard *g = arg;
    int poll_ms = 100;
    const char *env = getenv("OYAMA_CAPTURE_POLL_MS");
    if (env) {
        int v = atoi(env);
        if (v > 0 && v < 10000) poll_ms = v;
    }
    while (!g->stop_flag) {
        poll_once(g);
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = (long)poll_ms * 1000000L;
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int oyama_capture_guard_create(oyama_capture_guard **out, void *user_data,
                               oyama_capture_cb cb, void *err) {
    oyama_capture_guard *g;
    (void)err;
    if (out == NULL) return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    g = calloc(1, sizeof(*g));
    if (g == NULL) return OYAMA_VAULT_ERR_NOMEM;
    g->cb = cb;
    g->user_data = user_data;
    snprintf(g->proc_root, sizeof(g->proc_root), "/proc");
    *out = g;
    return OYAMA_VAULT_OK;
}

void oyama_capture_guard_set_proc_root(oyama_capture_guard *guard,
                                       const char *root) {
    if (guard == NULL || root == NULL) return;
    snprintf(guard->proc_root, sizeof(guard->proc_root), "%s", root);
}

int oyama_capture_guard_start(oyama_capture_guard *guard) {
    if (guard == NULL) return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    if (guard->thread_running) return OYAMA_VAULT_OK;
    guard->stop_flag = 0;
    guard->last_pid = 0;
    if (pthread_create(&guard->thread, NULL, capture_thread, guard) != 0) {
        return OYAMA_VAULT_ERR_CALLBACK;
    }
    guard->thread_running = 1;
    return OYAMA_VAULT_OK;
}

int oyama_capture_guard_stop(oyama_capture_guard *guard) {
    if (guard == NULL) return OYAMA_VAULT_ERR_INVALID_ARGUMENT;
    if (guard->thread_running) {
        guard->stop_flag = 1;
        pthread_join(guard->thread, NULL);
        guard->thread_running = 0;
    }
    return OYAMA_VAULT_OK;
}

void oyama_capture_guard_destroy(oyama_capture_guard *guard) {
    if (guard == NULL) return;
    oyama_capture_guard_stop(guard);
    free(guard);
}

int oyama_capture_guard_is_best_effort(oyama_capture_guard *guard) {
    (void)guard;
    return 1;
}

void oyama_capture_guard_test_inject(oyama_capture_guard *guard,
                                     const char *tool, int pid) {
    if (guard == NULL || guard->cb == NULL) return;
    guard->cb(tool, pid, guard->user_data);
}
