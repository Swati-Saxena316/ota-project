#include "sha256_util.h"
#include <string.h>
#include <ctype.h>
#include "sha256_util.h"
#include <stdlib.h>


void sha256_init(sha256_ctx_t *c)
{
    if (!c) return;
    c->ctx = malloc(sizeof(mbedtls_sha256_context));
    mbedtls_sha256_context *ctx = (mbedtls_sha256_context*)c->ctx;
    mbedtls_sha256_init(ctx);
    mbedtls_sha256_starts(ctx, 0);
}

void sha256_update(sha256_ctx_t *c, const uint8_t *data, size_t len)
{
    if (!c || !c->ctx || !data || len == 0) return;
    mbedtls_sha256_update((mbedtls_sha256_context*)c->ctx, data, len);
}

void sha256_final(sha256_ctx_t *c, uint8_t out32[32])
{
    if (!c || !c->ctx || !out32) return;
    mbedtls_sha256_finish((mbedtls_sha256_context*)c->ctx, out32);
}

void sha256_free(sha256_ctx_t *c)
{
    if (!c || !c->ctx) return;
    mbedtls_sha256_free((mbedtls_sha256_context*)c->ctx);
    free(c->ctx);
    c->ctx = NULL;
}

void sha256_to_hex(const uint8_t hash32[32], char out_hex65[65])
{
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        out_hex65[i*2]     = hex[(hash32[i] >> 4) & 0xF];
        out_hex65[i*2 + 1] = hex[hash32[i] & 0xF];
    }
    out_hex65[64] = '\0';
}

static int hex_ci(char c)
{
    return tolower((unsigned char)c);
}

bool sha256_hex_equal(const char *a64, const char *b64)
{
    if (!a64 || !b64) return false;
    for (int i = 0; i < 64; i++)
    {
        if (hex_ci(a64[i]) != hex_ci(b64[i])) return false;
        if (a64[i] == '\0' || b64[i] == '\0') return false;
    }
    return true;
}
