#define _GNU_SOURCE

#include "oyama/security/vchan.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

struct oyama_vchan {
    int fd;
};

int oyama_vchan_connect(oyama_vchan **out, const char *endpoint) {
    int sv[2];
    oyama_vchan *v;
    signal(SIGPIPE, SIG_IGN);
    (void)endpoint;
    if (out == NULL) return -1;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return -1;
    v = calloc(1, sizeof(*v));
    if (v == NULL) { close(sv[0]); close(sv[1]); return -1; }
    v->fd = sv[0];
    /* sv[1] would be the other end; for a single-process fallback we keep
       sv[0] and close sv[1]. Real Xen transport uses libxenvchan. */
    close(sv[1]);
    *out = v;
    return 0;
}

int oyama_vchan_send(oyama_vchan *v, const void *data, size_t len) {
    if (v == NULL || data == NULL) return -1;
    if (write(v->fd, data, len) != (ssize_t)len) return -1;
    return 0;
}

int oyama_vchan_recv(oyama_vchan *v, void *buf, size_t len) {
    if (v == NULL || buf == NULL) return -1;
    if (read(v->fd, buf, len) != (ssize_t)len) return -1;
    return 0;
}

void oyama_vchan_close(oyama_vchan *v) {
    if (v == NULL) return;
    close(v->fd);
    free(v);
}
