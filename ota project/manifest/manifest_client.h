#ifndef MANIFEST_CLIENT_H
#define MANIFEST_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char version[32];
    char url[256];
    char sha256[65];          // hex string (64 chars + null)
    size_t size_bytes;
    char release_notes[256];
} ota_manifest_t;

// Fetches and parses OTA manifest from OTA_MANIFEST_URL
bool manifest_fetch(ota_manifest_t *out_manifest, char *err_msg, size_t err_sz);

#endif
