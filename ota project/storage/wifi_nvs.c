#include "wifi_nvs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define WIFI_NVS_NS     "wifi_creds"
#define WIFI_NVS_KEY_S  "ssid"
#define WIFI_NVS_KEY_P  "pass"

static bool nvs_ready(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    return ret == ESP_OK;
}

bool wifi_nvs_save_creds(const char *ssid, const char *pass)
{
    if (!ssid || !pass) return false;
    if (!nvs_ready()) return false;

    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t e1 = nvs_set_str(h, WIFI_NVS_KEY_S, ssid);
    esp_err_t e2 = nvs_set_str(h, WIFI_NVS_KEY_P, pass);
    esp_err_t ec = nvs_commit(h);

    nvs_close(h);
    return (e1 == ESP_OK && e2 == ESP_OK && ec == ESP_OK);
}

bool wifi_nvs_load_creds(char *ssid_out, size_t ssid_sz, char *pass_out, size_t pass_sz)
{
    if (!ssid_out || !pass_out || ssid_sz == 0 || pass_sz == 0) return false;
    ssid_out[0] = '\0';
    pass_out[0] = '\0';

    if (!nvs_ready()) return false;

    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t ssz = ssid_sz;
    size_t psz = pass_sz;

    esp_err_t e1 = nvs_get_str(h, WIFI_NVS_KEY_S, ssid_out, &ssz);
    esp_err_t e2 = nvs_get_str(h, WIFI_NVS_KEY_P, pass_out, &psz);

    nvs_close(h);
    return (e1 == ESP_OK && e2 == ESP_OK && ssid_out[0] != '\0');
}

bool wifi_nvs_has_creds(void)
{
    char s[32], p[64];
    return wifi_nvs_load_creds(s, sizeof(s), p, sizeof(p));
}

bool wifi_nvs_clear(void)
{
    if (!nvs_ready()) return false;

    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t e = nvs_erase_all(h);
    esp_err_t c = nvs_commit(h);
    nvs_close(h);

    return (e == ESP_OK && c == ESP_OK);
}
