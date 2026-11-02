#include "oyama/security/keyproto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int main(void) {
    int op;
    /* round-trip every op */
    for (op = 1; op <= 9; ++op) {
        oyama_key_header h;
        unsigned char buf[16];
        oyama_key_header out;
        memset(&h, 0, sizeof(h));
        h.magic = OYAMA_KEY_MAGIC;
        h.version = OYAMA_KEY_PROTO_VERSION;
        h.op = (uint16_t)op;
        h.request_id = 42;
        h.payload_len = 100;
        check(oyama_key_encode_header(buf, sizeof(buf), &h) == 0,
              "encode ok");
        check(oyama_key_decode_header(buf, sizeof(buf), &out) == 0,
              "decode ok");
        check(out.magic == h.magic && out.version == h.version &&
                  out.op == h.op && out.request_id == h.request_id &&
                  out.payload_len == h.payload_len,
              "round-trip fields");
        check(oyama_key_frame_valid(&out) == 1, "frame valid");
    }

    /* bad magic */
    {
        oyama_key_header h;
        unsigned char buf[16];
        oyama_key_header out;
        memset(&h, 0, sizeof(h));
        h.magic = 0xDEADBEEF;
        h.version = OYAMA_KEY_PROTO_VERSION;
        h.op = 1;
        h.payload_len = 0;
        oyama_key_encode_header(buf, 16, &h);
        oyama_key_decode_header(buf, 16, &out);
        check(oyama_key_frame_valid(&out) == 0, "bad magic rejected");
    }

    /* wrong version */
    {
        oyama_key_header h;
        unsigned char buf[16];
        oyama_key_header out;
        memset(&h, 0, sizeof(h));
        h.magic = OYAMA_KEY_MAGIC;
        h.version = 99;
        h.op = 1;
        h.payload_len = 0;
        oyama_key_encode_header(buf, 16, &h);
        oyama_key_decode_header(buf, 16, &out);
        check(oyama_key_frame_valid(&out) == 0, "wrong version rejected");
    }

    /* oversized payload_len */
    {
        oyama_key_header h;
        unsigned char buf[16];
        oyama_key_header out;
        memset(&h, 0, sizeof(h));
        h.magic = OYAMA_KEY_MAGIC;
        h.version = OYAMA_KEY_PROTO_VERSION;
        h.op = 1;
        h.payload_len = OYAMA_KEY_MAX_FRAME + 1;
        oyama_key_encode_header(buf, 16, &h);
        oyama_key_decode_header(buf, 16, &out);
        check(oyama_key_frame_valid(&out) == 0, "oversized payload rejected");
    }

    /* truncated header */
    {
        unsigned char buf[16];
        oyama_key_header out;
        memset(buf, 0, sizeof(buf));
        check(oyama_key_decode_header(buf, 15, &out) != 0,
              "truncated header rejected");
    }

    /* invalid op */
    {
        oyama_key_header h;
        unsigned char buf[16];
        oyama_key_header out;
        memset(&h, 0, sizeof(h));
        h.magic = OYAMA_KEY_MAGIC;
        h.version = OYAMA_KEY_PROTO_VERSION;
        h.op = 99;
        h.payload_len = 0;
        oyama_key_encode_header(buf, 16, &h);
        oyama_key_decode_header(buf, 16, &out);
        check(oyama_key_frame_valid(&out) == 0, "invalid op rejected");
    }

    /* NULL handling */
    {
        unsigned char buf[16];
        oyama_key_header h;
        check(oyama_key_encode_header(NULL, 16, &h) != 0, "encode null buf");
        check(oyama_key_encode_header(buf, 16, NULL) != 0, "encode null h");
        check(oyama_key_decode_header(NULL, 16, &h) != 0, "decode null buf");
        check(oyama_key_decode_header(buf, 16, NULL) != 0, "decode null h");
        check(oyama_key_frame_valid(NULL) == 0, "frame_valid null");
    }

    /* fuzz: 1000 random buffers, no crash */
    {
        unsigned char buf[16];
        int i;
        for (i = 0; i < 1000; ++i) {
            int j;
            for (j = 0; j < 16; ++j) {
                buf[j] = (unsigned char)(rand() & 0xFF);
            }
            oyama_key_header out;
            if (oyama_key_decode_header(buf, 16, &out) == 0) {
                oyama_key_frame_valid(&out);
            }
        }
    }

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("keyproto_test: PASS\n");
    return 0;
}
