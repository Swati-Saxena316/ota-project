#ifndef SHA256_UTIL_H
#define SHA256_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void *ctx;   // opaque
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *c);
void sha256_update(sha256_ctx_t *c, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx_t *c, uint8_t out32[32]);
void sha256_free(sha256_ctx_t *c);

// Convert 32-byte hash to lowercase hex string (65 bytes)
void sha256_to_hex(const uint8_t hash32[32], char out_hex65[65]);

// case-insensitive compare of 64 hex chars
bool sha256_hex_equal(const char *a64, const char *b64);

#endif
