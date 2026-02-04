#include "wifi_manager.h"
#include "storage/wifi_nvs.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

static bool wifi_connected = false;
static bool wifi_failed = false;
static int retry_count = 0;

#define MAX_WIFI_RETRY  3

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg; (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (retry_count < MAX_WIFI_RETRY)
        {
            retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Retrying WiFi (%d)", retry_count);
        }
        else
        {
            wifi_failed = true;
            ESP_LOGE(TAG, "WiFi connection failed");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;
        retry_count = 0;
        ESP_LOGI(TAG, "WiFi connected, IP acquired");
    }
}

void wifi_manager_init(void)
{
    // NVS init (safe)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // If creds exist in NVS, apply them to WiFi STA config now
    char ssid[32], pass[64];
    if (wifi_nvs_load_creds(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        wifi_config_t wifi_cfg = {0};
        strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char*)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password) - 1);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_LOGI(TAG, "Loaded WiFi creds from NVS (ssid=%s)", ssid);
    }
    else
    {
        ESP_LOGI(TAG, "No WiFi creds in NVS yet");
    }
}

bool wifi_credentials_available(void)
{
    return wifi_nvs_has_creds();
}

void wifi_manager_start(void)
{
    wifi_connected = false;
    wifi_failed = false;
    retry_count = 0;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi STA started");
}

bool wifi_is_connected(void) { return wifi_connected; }
bool wifi_has_failed(void) { return wifi_failed; }
