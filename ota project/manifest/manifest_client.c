#include "manifest_client.h"
#include "config/ota_config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if OTA_USE_CRT_BUNDLE
#include "esp_crt_bundle.h"
#endif

static const char *TAG = "MANIFEST";

static bool json_extract_string(const char *json, const char *key, char *out, size_t out_sz)
{
    if (!json || !key || !out || out_sz == 0) return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return false;

    p = strchr(p, ':');
    if (!p) return false;
    p++;

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\"') return false;
    p++;

    const char *end = strchr(p, '\"');
    if (!end) return false;

    size_t n = (size_t)(end - p);
    if (n >= out_sz) n = out_sz - 1;

    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

static bool json_extract_size_t(const char *json, const char *key, size_t *out_val)
{
    if (!json || !key || !out_val) return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return false;

    p = strchr(p, ':');
    if (!p) return false;
    p++;

    while (*p && isspace((unsigned char)*p)) p++;

    // Parse integer
    char *endptr = NULL;
    unsigned long v = strtoul(p, &endptr, 10);
    if (endptr == p) return false;

    *out_val = (size_t)v;
    return true;
}

static esp_err_t fetch_text_https(const char *url, char *buf, size_t buf_sz)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
#if OTA_USE_CRT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = ROOT_CA_PEM,
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200)
    {
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int r = esp_http_client_read(client, buf, (int)buf_sz - 1);
    if (r < 0)
    {
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    buf[r] = '\0';

    esp_http_client_cleanup(client);
    return ESP_OK;
}

bool manifest_fetch(ota_manifest_t *m, char *err_msg, size_t err_sz)
{
    if (!m) return false;
    memset(m, 0, sizeof(*m));
    if (err_msg && err_sz) err_msg[0] = '\0';

    char json[1400] = {0};
    esp_err_t err = fetch_text_https(OTA_MANIFEST_URL, json, sizeof(json));
    if (err != ESP_OK)
    {
        if (err_msg) snprintf(err_msg, err_sz, "manifest fetch failed");
        ESP_LOGE(TAG, "fetch failed: %d", (int)err);
        return false;
    }

    if (!json_extract_string(json, "version", m->version, sizeof(m->version)) ||
        !json_extract_string(json, "url", m->url, sizeof(m->url)) ||
        !json_extract_string(json, "sha256", m->sha256, sizeof(m->sha256)) ||
        !json_extract_size_t(json, "size", &m->size_bytes))
    {
        if (err_msg) snprintf(err_msg, err_sz, "manifest parse failed");
        ESP_LOGE(TAG, "parse failed: %s", json);
        return false;
    }

    // release_notes is optional
    json_extract_string(json, "release_notes", m->release_notes, sizeof(m->release_notes));

    // Basic sha length check
    if (strlen(m->sha256) != 64)
    {
        if (err_msg) snprintf(err_msg, err_sz, "invalid sha256 length");
        ESP_LOGE(TAG, "sha256 length invalid");
        return false;
    }

    ESP_LOGI(TAG, "Manifest: ver=%s size=%u url=%s", m->version, (unsigned)m->size_bytes, m->url);
    return true;
}
