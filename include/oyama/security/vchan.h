#ifndef OYAMA_SECURITY_VCHAN_H
#define OYAMA_SECURITY_VCHAN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oyama_vchan oyama_vchan;

int oyama_vchan_connect(oyama_vchan **out, const char *endpoint);
int oyama_vchan_send(oyama_vchan *v, const void *data, size_t len);
int oyama_vchan_recv(oyama_vchan *v, void *buf, size_t len);
void oyama_vchan_close(oyama_vchan *v);

#ifdef __cplusplus
}
#endif

#endif
