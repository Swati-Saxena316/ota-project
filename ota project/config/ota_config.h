#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

// Manifest URL (HTTPS)
#define OTA_MANIFEST_URL   "https://your-domain.com/firmware/manifest.json"

// Using ESP-IDF cert bundle
#define OTA_USE_CRT_BUNDLE  1

#if !OTA_USE_CRT_BUNDLE
// If you disable CRT bundle, define ROOT_CA_PEM string here.
static const char *ROOT_CA_PEM = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";
#endif

// Network timeouts
#define OTA_HTTP_TIMEOUT_MS   30000

#endif
