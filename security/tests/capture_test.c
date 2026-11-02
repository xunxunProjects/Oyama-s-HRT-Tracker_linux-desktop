#define _GNU_SOURCE

#include "oyama/security/capture.h"

#include "oyama/vault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int failures = 0;
static volatile int got_callback = 0;
static char got_tool[256];
static int got_pid = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static void cb(const char *tool, int pid, void *user_data) {
    (void)user_data;
    snprintf(got_tool, sizeof(got_tool), "%s", tool ? tool : "");
    got_pid = pid;
    got_callback = 1;
}

static int make_fake_proc(const char *root) {
    char path[4096];
    FILE *f;
    snprintf(path, sizeof(path), "%s", root);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/1234", root);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/1234/comm", root);
    f = fopen(path, "w");
    if (f == NULL) return -1;
    fputs("gnome-screenshot\n", f);
    fclose(f);

    /* non-matching process */
    snprintf(path, sizeof(path), "%s/5678", root);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/5678/comm", root);
    f = fopen(path, "w");
    if (f == NULL) return -1;
    fputs("bash\n", f);
    fclose(f);
    return 0;
}

static void cleanup_fake(const char *root) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/1234/comm", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/1234", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/5678/comm", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/5678", root);
    rmdir(path);
    rmdir(root);
}

int main(void) {
    char root[] = "/tmp/oyama-fakeproc-XXXXXX";
    if (mkdtemp(root) == NULL) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }
    if (make_fake_proc(root) != 0) {
        fprintf(stderr, "make_fake_proc failed\n");
        return 1;
    }

    setenv("OYAMA_CAPTURE_POLL_MS", "10", 1);

    /* Test 1: matching process triggers callback */
    {
        oyama_capture_guard *g = NULL;
        oyama_error err;
        memset(&err, 0, sizeof(err));
        got_callback = 0;
        got_tool[0] = '\0';
        got_pid = 0;

        if (oyama_capture_guard_create(&g, NULL, cb, &err) != OYAMA_VAULT_OK) {
            fprintf(stderr, "create failed: %s\n", err.message);
            ++failures;
        } else {
            oyama_capture_guard_set_proc_root(g, root);
            oyama_capture_guard_start(g);
            /* wait up to 1s */
            int i;
            for (i = 0; i < 100 && !got_callback; ++i) {
                usleep(10000);
            }
            check(got_callback == 1, "callback fires for gnome-screenshot");
            check(strcmp(got_tool, "gnome-screenshot") == 0,
                  "tool name is gnome-screenshot");
            check(got_pid == 1234, "pid is 1234");
            oyama_capture_guard_stop(g);
            oyama_capture_guard_destroy(g);
        }
    }

    /* Test 2: test inject */
    {
        oyama_capture_guard *g = NULL;
        oyama_error err;
        memset(&err, 0, sizeof(err));
        got_callback = 0;
        if (oyama_capture_guard_create(&g, NULL, cb, &err) == OYAMA_VAULT_OK) {
            oyama_capture_guard_test_inject(g, "flameshot", 99);
            check(got_callback == 1, "inject fires");
            check(strcmp(got_tool, "flameshot") == 0, "inject tool name");
            check(got_pid == 99, "inject pid");
            oyama_capture_guard_destroy(g);
        }
    }

    /* Test 3: repeated start/stop no crash */
    {
        oyama_capture_guard *g = NULL;
        oyama_error err;
        memset(&err, 0, sizeof(err));
        if (oyama_capture_guard_create(&g, NULL, cb, &err) == OYAMA_VAULT_OK) {
            int i;
            for (i = 0; i < 5; ++i) {
                oyama_capture_guard_start(g);
                oyama_capture_guard_stop(g);
            }
            check(1, "repeated start/stop no crash");
            oyama_capture_guard_destroy(g);
        }
    }

    cleanup_fake(root);

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("capture_test: PASS\n");
    return 0;
}
