#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *socket_path = NULL;
    const char *db_conninfo = NULL;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_conninfo = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("usage: %s --socket PATH --db CONNINFO\n", argv[0]);
            return 0;
        }
    }

    if (socket_path == NULL) {
        fprintf(stderr, "error: --socket is required\n");
        return 2;
    }
    return oyama_keyd_run(socket_path, db_conninfo);
}
