#include "ota_diag.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "OTA_DIAG";

#define OTA_DIAG_NS              "ota_diag"
#define KEY_LAST_STATUS          "last_status"      // u8
#define KEY_LAST_ERROR           "last_error"       // u16 stored as u32
#define KEY_LAST_ATTEMPT_VER     "attempt_ver"      // str
#define KEY_LAST_INSTALLED_VER   "installed_ver"    // str
#define KEY_ROLLBACK_SEEN        "rollback_seen"    // u8
#define KEY_BOOT_COUNT           "boot_count"       // u32

static bool nvs_open_ns(nvs_handle_t *out)
{
    if (!out) return false;
    if (nvs_open(OTA_DIAG_NS, NVS_READWRITE, out) != ESP_OK) return false;
    return true;
}

bool ota_diag_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return false;
    return true;
}

static void nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v)
{
    (void)nvs_set_u8(h, key, v);
}

static void nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v)
{
    (void)nvs_set_u32(h, key, v);
}

static void nvs_set_str_safe(nvs_handle_t h, const char *key, const char *s)
{
    if (!s) s = "";
    (void)nvs_set_str(h, key, s);
}

static uint8_t nvs_get_u8_def(nvs_handle_t h, const char *key, uint8_t def)
{
    uint8_t v = def;
    (void)nvs_get_u8(h, key, &v);
    return v;
}

static uint32_t nvs_get_u32_def(nvs_handle_t h, const char *key, uint32_t def)
{
    uint32_t v = def;
    (void)nvs_get_u32(h, key, &v);
    return v;
}

static void nvs_get_str_def(nvs_handle_t h, const char *key, char *out, size_t out_sz, const char *def)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    size_t needed = 0;
    esp_err_t e = nvs_get_str(h, key, NULL, &needed);
    if (e != ESP_OK || needed == 0 || needed > out_sz)
    {
        snprintf(out, out_sz, "%s", def ? def : "");
        return;
    }
    e = nvs_get_str(h, key, out, &needed);
    if (e != ESP_OK) snprintf(out, out_sz, "%s", def ? def : "");
}

void ota_diag_record_attempt(const char *attempt_version)
{
    if (!ota_diag_init()) return;

    nvs_handle_t h;
    if (!nvs_open_ns(&h)) return;

    nvs_set_str_safe(h, KEY_LAST_ATTEMPT_VER, attempt_version);

    // Also clear rollback flag for a new attempt (optional but useful)
    nvs_set_u8(h, KEY_ROLLBACK_SEEN, 0);

    (void)nvs_commit(h);
    nvs_close(h);
}

void ota_diag_record_result(ota_diag_status_t status,
                            uint16_t error_code,
                            const char *attempt_version,
                            const char *installed_version)
{
    if (!ota_diag_init()) return;

    nvs_handle_t h;
    if (!nvs_open_ns(&h)) return;

    nvs_set_u8(h, KEY_LAST_STATUS, (uint8_t)status);
    nvs_set_u32(h, KEY_LAST_ERROR, (uint32_t)error_code);

    if (attempt_version) nvs_set_str_safe(h, KEY_LAST_ATTEMPT_VER, attempt_version);
    if (installed_version) nvs_set_str_safe(h, KEY_LAST_INSTALLED_VER, installed_version);

    (void)nvs_commit(h);
    nvs_close(h);
}

bool ota_diag_get_last(ota_diag_record_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    if (!ota_diag_init()) return false;

    nvs_handle_t h;
    if (nvs_open(OTA_DIAG_NS, NVS_READONLY, &h) != ESP_OK) return false;

    out->last_status = (ota_diag_status_t)nvs_get_u8_def(h, KEY_LAST_STATUS, OTA_DIAG_STATUS_UNKNOWN);
    out->rollback_seen = nvs_get_u8_def(h, KEY_ROLLBACK_SEEN, 0);
    out->boot_count = nvs_get_u32_def(h, KEY_BOOT_COUNT, 0);

    uint32_t err_u32 = 0;
    (void)nvs_get_u32(h, KEY_LAST_ERROR, &err_u32);
    out->last_error = (uint16_t)err_u32;

    nvs_get_str_def(h, KEY_LAST_ATTEMPT_VER, out->last_attempt_ver, sizeof(out->last_attempt_ver), "");
    nvs_get_str_def(h, KEY_LAST_INSTALLED_VER, out->last_installed_ver, sizeof(out->last_installed_ver), "");

    nvs_close(h);
    return true;
}

static void increment_boot_count(void)
{
    if (!ota_diag_init()) return;

    nvs_handle_t h;
    if (!nvs_open_ns(&h)) return;

    uint32_t bc = nvs_get_u32_def(h, KEY_BOOT_COUNT, 0);
    bc++;
    nvs_set_u32(h, KEY_BOOT_COUNT, bc);

    (void)nvs_commit(h);
    nvs_close(h);
}

void ota_diag_boot_check_and_update(void)
{
    increment_boot_count();

    // Detect rollback: ESP-IDF provides last invalid partition pointer if rollback happened previously.
    const esp_partition_t *last_invalid = esp_ota_get_last_invalid_partition();
    if (last_invalid)
    {
        ESP_LOGW(TAG, "Rollback detected (last invalid partition present).");

        // Record rollback flag and mark last status as failed (error code 0xFFFF used by us for rollback)
        if (ota_diag_init())
        {
            nvs_handle_t h;
            if (nvs_open_ns(&h))
            {
                nvs_set_u8(h, KEY_ROLLBACK_SEEN, 1);
                nvs_set_u8(h, KEY_LAST_STATUS, (uint8_t)OTA_DIAG_STATUS_FAILED);
                nvs_set_u32(h, KEY_LAST_ERROR, 0xFFFF); // rollback sentinel if you want
                (void)nvs_commit(h);
                nvs_close(h);
            }
        }
    }

    // If this image is pending verify, mark it valid and store installed version.
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK)
    {
        if (state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGI(TAG, "Image pending verify: marking valid & cancel rollback");
            esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
            if (e == ESP_OK)
            {
                const esp_app_desc_t *app = esp_app_get_description();
                ota_diag_record_result(OTA_DIAG_STATUS_SUCCESS, 0, NULL, app->version);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to mark valid: %s", esp_err_to_name(e));
            }
        }
    }
}

const char* ota_diag_status_str(ota_diag_status_t s)
{
    switch (s)
    {
        case OTA_DIAG_STATUS_NO_UPDATE: return "NO_UPDATE";
        case OTA_DIAG_STATUS_SUCCESS:   return "SUCCESS";
        case OTA_DIAG_STATUS_FAILED:    return "FAILED";
        default:                        return "UNKNOWN";
    }
}

// Short error strings for LCD (keep them very short)
const char* ota_diag_error_short_str(uint16_t err)
{
    // 0xFFFF used above for rollback sentinel
    if (err == 0xFFFF) return "ROLLBACK";

    switch (err)
    {
        case 0:   return "OK";
        case 1:   return "MANIFEST";
        case 2:   return "MANIFEST";
        case 3:   return "NO_UPG";
        case 4:   return "HTTP_OPEN";
        case 5:   return "HTTP_READ";
        case 6:   return "SIZE";
        case 7:   return "SHA";
        case 8:   return "OTA_BEGIN";
        case 9:   return "OTA_WRITE";
        case 10:  return "OTA_END";
        case 11:  return "SET_BOOT";
        default:  return "ERR";
    }
}
