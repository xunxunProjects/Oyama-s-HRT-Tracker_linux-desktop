#ifndef OYAMA_SECURITY_MEM_H
#define OYAMA_SECURITY_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void oyama_secure_zero(void *ptr, size_t len);
void *oyama_secure_alloc(size_t len);
void oyama_secure_free(void *ptr, size_t len);
int oyama_mlock(const void *ptr, size_t len);
int oyama_munlock(const void *ptr, size_t len);
int oyama_disable_core_dumps(void);
int oyama_mark_dontdump(void *ptr, size_t len);
int oyama_release_free_pages(void);

#ifdef __cplusplus
}
#endif

#endif
