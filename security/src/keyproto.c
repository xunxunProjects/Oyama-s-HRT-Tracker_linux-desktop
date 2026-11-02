#include "oyama/security/keyproto.h"

#include <string.h>

int oyama_key_encode_header(unsigned char *buf, size_t buf_len,
                            const oyama_key_header *h) {
    if (buf == NULL || h == NULL || buf_len < 16) {
        return -1;
    }
    buf[0] = (unsigned char)(h->magic);
    buf[1] = (unsigned char)(h->magic >> 8);
    buf[2] = (unsigned char)(h->magic >> 16);
    buf[3] = (unsigned char)(h->magic >> 24);
    buf[4] = (unsigned char)(h->version);
    buf[5] = (unsigned char)(h->version >> 8);
    buf[6] = (unsigned char)(h->op);
    buf[7] = (unsigned char)(h->op >> 8);
    buf[8] = (unsigned char)(h->request_id);
    buf[9] = (unsigned char)(h->request_id >> 8);
    buf[10] = (unsigned char)(h->request_id >> 16);
    buf[11] = (unsigned char)(h->request_id >> 24);
    buf[12] = (unsigned char)(h->payload_len);
    buf[13] = (unsigned char)(h->payload_len >> 8);
    buf[14] = (unsigned char)(h->payload_len >> 16);
    buf[15] = (unsigned char)(h->payload_len >> 24);
    return 0;
}

int oyama_key_decode_header(const unsigned char *buf, size_t buf_len,
                            oyama_key_header *h) {
    if (buf == NULL || h == NULL || buf_len < 16) {
        return -1;
    }
    h->magic = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
               ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    h->version = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    h->op = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
    h->request_id = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) |
                    ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
    h->payload_len = (uint32_t)buf[12] | ((uint32_t)buf[13] << 8) |
                     ((uint32_t)buf[14] << 16) | ((uint32_t)buf[15] << 24);
    return 0;
}

int oyama_key_frame_valid(const oyama_key_header *h) {
    if (h == NULL) return 0;
    if (h->magic != OYAMA_KEY_MAGIC) return 0;
    if (h->version != OYAMA_KEY_PROTO_VERSION) return 0;
    if (h->op < 1 || h->op > 9) return 0;
    if (h->payload_len > OYAMA_KEY_MAX_FRAME) return 0;
    return 1;
}
