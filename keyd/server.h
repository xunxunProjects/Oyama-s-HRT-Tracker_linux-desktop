#ifndef OYAMA_KEYD_SERVER_H
#define OYAMA_KEYD_SERVER_H

#include <stddef.h>

int oyama_keyd_run(const char *socket_path, const char *db_conninfo);

#endif
