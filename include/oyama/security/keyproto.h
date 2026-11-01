#ifndef OYAMA_SECURITY_KEYPROTO_H
#define OYAMA_SECURITY_KEYPROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OYAMA_KEY_PROTO_VERSION 1
#define OYAMA_KEY_MAX_FRAME 1048576
#define OYAMA_KEY_MAGIC 0x59454B4Fu

enum oyama_key_op {
    OYAMA_KEY_OP_HELLO   = 1,
    OYAMA_KEY_OP_UNLOCK  = 2,
    OYAMA_KEY_OP_LOCK    = 3,
    OYAMA_KEY_OP_PUT     = 4,
    OYAMA_KEY_OP_GET     = 5,
    OYAMA_KEY_OP_REMOVE  = 6,
    OYAMA_KEY_OP_SCAN    = 7,
    OYAMA_KEY_OP_STATUS  = 8,
    OYAMA_KEY_OP_PANIC   = 9
};

typedef struct oyama_key_header {
    uint32_t magic;
    uint16_t version;
    uint16_t op;
    uint32_t request_id;
    uint32_t payload_len;
} oyama_key_header;

int oyama_key_encode_header(unsigned char *buf, size_t buf_len,
                            const oyama_key_header *h);
int oyama_key_decode_header(const unsigned char *buf, size_t buf_len,
                            oyama_key_header *h);
int oyama_key_frame_valid(const oyama_key_header *h);

#ifdef __cplusplus
}
#endif

#endif
