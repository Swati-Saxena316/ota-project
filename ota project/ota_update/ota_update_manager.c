#include "ota_update_manager.h"
#include "config/ota_config.h"
#include "manifest/manifest_client.h"
#include "security/sha256_util.h"
#include "storage/ota_diag.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#if OTA_USE_CRT_BUNDLE
#include "esp_crt_bundle.h"
#endif

static const char *TAG = "OTA_UPD10";

static ota_update_info_t g_info;
static TaskHandle_t g_task = NULL;

/* ---------- SemVer compare ---------- */
static void parse_semver(const char *v, int *a, int *b, int *c)
{
    *a = *b = *c = 0;
    if (!v) return;
    if (v[0] == 'v' || v[0] == 'V') v++;
    sscanf(v, "%d.%d.%d", a, b, c);
}

static int semver_cmp(const char *v1, const char *v2)
{
    int a1,b1,c1,a2,b2,c2;
    parse_semver(v1, &a1, &b1, &c1);
    parse_semver(v2, &a2, &b2, &c2);
    if (a1 != a2) return (a1 > a2) ? 1 : -1;
    if (b1 != b2) return (b1 > b2) ? 1 : -1;
    if (c1 != c2) return (c1 > c2) ? 1 : -1;
    return 0;
}

static void set_fail(ota_update_error_t e, const char *msg)
{
    g_info.status = OTA_UPD_FAILED;
    g_info.error = e;
    strncpy(g_info.last_error, msg ? msg : "error", sizeof(g_info.last_error) - 1);
    g_info.last_error[sizeof(g_info.last_error) - 1] = '\0';
}

/* ---------- Streaming OTA Task ---------- */
static void ota_task(void *arg)
{
    (void)arg;

    g_info.status = OTA_UPD_RUNNING;
    g_info.error = OTA_ERR_NONE;
    g_info.progress_percent = 0;
    g_info.bytes_written = 0;
    g_info.total_size = 0;
    g_info.remote_ver[0] = '\0';
    g_info.last_error[0] = '\0';

    const esp_app_desc_t *app = esp_app_get_description();
    snprintf(g_info.current_ver, sizeof(g_info.current_ver), "%s", app->version);

    // 1) Fetch manifest
    ota_manifest_t mf;
    char m_err[64];
    if (!manifest_fetch(&mf, m_err, sizeof(m_err)))
    {
        set_fail(OTA_ERR_MANIFEST_FETCH, m_err);
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, NULL, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    snprintf(g_info.remote_ver, sizeof(g_info.remote_ver), "%s", mf.version);
    g_info.total_size = (int)mf.size_bytes;

    // Record attempted version early (useful even if it fails)
    ota_diag_record_attempt(mf.version);

    // 2) Version check (prevent downgrade)
    if (semver_cmp(mf.version, g_info.current_ver) <= 0)
    {
        g_info.status = OTA_UPD_NO_UPDATE;
        g_info.error  = OTA_ERR_VERSION_NO_UPGRADE;
        ota_diag_record_result(OTA_DIAG_STATUS_NO_UPDATE, (uint16_t)g_info.error, mf.version, app->version);

        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 3) Open HTTPS firmware URL
    esp_http_client_config_t cfg = {
        .url = mf.url,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
#if OTA_USE_CRT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = ROOT_CA_PEM,
#endif
        .buffer_size = 4096,
        .buffer_size_tx = 1024
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
    {
        set_fail(OTA_ERR_HTTP_OPEN, "http init failed");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        set_fail(OTA_ERR_HTTP_OPEN, "http open failed");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 4) Prepare OTA partition
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_fail(OTA_ERR_OTA_BEGIN, "no update partition");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    esp_ota_handle_t ota_handle = 0;
    err = esp_ota_begin(update_part, mf.size_bytes, &ota_handle);
    if (err != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_fail(OTA_ERR_OTA_BEGIN, "esp_ota_begin failed");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 5) Streaming loop (4KB chunks) + SHA256
    uint8_t buf[4096];
    sha256_ctx_t sha;
    sha256_init(&sha);

    int total_written = 0;
    bool ok = true;

    while (1)
    {
        int r = esp_http_client_read(client, (char*)buf, sizeof(buf));
        if (r < 0)
        {
            ok = false;
            set_fail(OTA_ERR_HTTP_READ, "http read failed");
            break;
        }
        if (r == 0) break; // EOF

        sha256_update(&sha, buf, (size_t)r);

        err = esp_ota_write(ota_handle, buf, r);
        if (err != ESP_OK)
        {
            ok = false;
            set_fail(OTA_ERR_OTA_WRITE, "esp_ota_write failed");
            break;
        }

        total_written += r;

        g_info.bytes_written = total_written;
        if (mf.size_bytes > 0)
        {
            int pct = (int)((total_written * 100LL) / (long long)mf.size_bytes);
            if (pct > 100) pct = 100;
            if (pct < 0) pct = 0;
            g_info.progress_percent = pct;
        }

        if (mf.size_bytes > 0 && (size_t)total_written > mf.size_bytes)
        {
            ok = false;
            set_fail(OTA_ERR_SIZE_MISMATCH, "download bigger than manifest size");
            break;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!ok)
    {
        sha256_free(&sha);
        esp_ota_abort(ota_handle);
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 6) Size validation
    if ((size_t)total_written != mf.size_bytes)
    {
        sha256_free(&sha);
        esp_ota_abort(ota_handle);
        set_fail(OTA_ERR_SIZE_MISMATCH, "size mismatch vs manifest");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 7) SHA256 validation
    uint8_t hash32[32];
    char hash_hex[65];
    sha256_final(&sha, hash32);
    sha256_free(&sha);
    sha256_to_hex(hash32, hash_hex);

    if (!sha256_hex_equal(hash_hex, mf.sha256))
    {
        esp_ota_abort(ota_handle);
        set_fail(OTA_ERR_SHA256_MISMATCH, "sha256 mismatch");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 8) End OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        set_fail(OTA_ERR_OTA_END, "esp_ota_end failed");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 9) Set boot partition
    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK)
    {
        set_fail(OTA_ERR_SET_BOOT, "set boot partition failed");
        ota_diag_record_result(OTA_DIAG_STATUS_FAILED, (uint16_t)g_info.error, mf.version, NULL);
        g_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 10) Persist “success attempt” BEFORE reboot (installed version will be confirmed at boot)
    g_info.progress_percent = 100;
    g_info.status = OTA_UPD_SUCCESS;
    g_info.error = OTA_ERR_NONE;

    ota_diag_record_result(OTA_DIAG_STATUS_SUCCESS, 0, mf.version, NULL);

    ESP_LOGI(TAG, "OTA SUCCESS -> rebooting");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

/* ---------- Public API ---------- */
void ota_update_init(void)
{
    memset(&g_info, 0, sizeof(g_info));
    g_info.status = OTA_UPD_IDLE;
    g_info.error = OTA_ERR_NONE;
}

void ota_update_start(void)
{
    if (g_task != NULL)
    {
        ESP_LOGW(TAG, "OTA already running");
        return;
    }

    xTaskCreate(ota_task, "ota_stream_task", 8192, NULL, 5, &g_task);
}

ota_update_info_t ota_update_get_info(void)
{
    return g_info;
}

bool ota_update_is_running(void)
{
    return (g_info.status == OTA_UPD_RUNNING);
}
