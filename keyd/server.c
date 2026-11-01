#include "server.h"

#include "oyama/vault.h"
#include "oyama/security/keyproto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int oyama_keyd_run(const char *socket_path, const char *db_conninfo) {
    int fd;
    struct sockaddr_un addr;

    if (socket_path == NULL) {
        return 1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    unlink(socket_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(fd);
        return 1;
    }
    if (listen(fd, 8) != 0) {
        perror("listen");
        close(fd);
        return 1;
    }

    (void)db_conninfo;
    /* Minimal daemon: accept one connection, read a frame, reply with
       HELLO echo, then exit. Full request handling is future work. */
    {
        int client = accept(fd, NULL, NULL);
        if (client >= 0) {
            unsigned char buf[16];
            ssize_t n = read(client, buf, sizeof(buf));
            if (n >= 16) {
                oyama_key_header h;
                oyama_key_decode_header(buf, (size_t)n, &h);
                if (oyama_key_frame_valid(&h)) {
                    /* reply with same header, status 0 */
                    unsigned char resp[16];
                    oyama_key_header rh = h;
                    oyama_key_encode_header(resp, sizeof(resp), &rh);
                    write(client, resp, sizeof(resp));
                }
            }
            close(client);
        }
    }

    close(fd);
    unlink(socket_path);
    return 0;
}
