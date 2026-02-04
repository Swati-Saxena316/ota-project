#include "ota_state_machine.h"
#include "ota_manager.h"
#include "ota_states.h"

#include "ui/lcd_ui.h"
#include "network/wifi_manager.h"
#include "provisioning/provisioning_manager.h"
#include "ota_update/ota_update_manager.h"
#include "storage/ota_diag.h"

#include <stdio.h>

static bool s_prov_started = false;
static bool s_wifi_started = false;
static bool s_ota_started  = false;

static void reset_session_flags(void)
{
    s_prov_started = false;
    s_wifi_started = false;
    s_ota_started  = false;
}

void ota_state_machine_process(void)
{
    ota_update_info_t upd = ota_update_get_info();

    switch (ota_get_state())
    {
        case OTA_STATE_IDLE:
            reset_session_flags();
            break;

        case OTA_STATE_CHECKING_WIFI:
            lcd_show_message("Checking WiFi");
            if (wifi_credentials_available())
                ota_set_state(OTA_STATE_CONNECTING);
            else
                ota_set_state(OTA_STATE_PROVISIONING);
            break;

        case OTA_STATE_PROVISIONING:
            lcd_show_message("Setup WiFi");
            if (!s_prov_started)
            {
                provisioning_start();
                s_prov_started = true;
            }

            if (provisioning_is_done())
            {
                provisioning_stop();
                s_prov_started = false;
                ota_set_state(OTA_STATE_CONNECTING);
            }
            else if (provisioning_has_failed())
            {
                provisioning_stop();
                s_prov_started = false;
                ota_set_state(OTA_STATE_FAILED);
            }
            break;

        case OTA_STATE_CONNECTING:
            lcd_show_message("Connecting WiFi");
            if (!s_wifi_started)
            {
                wifi_manager_start();
                s_wifi_started = true;
            }
            ota_set_state(OTA_STATE_FETCHING_MANIFEST);
            break;

        case OTA_STATE_FETCHING_MANIFEST:
            if (wifi_is_connected())
            {
                lcd_show_message("WiFi Connected");
                ota_set_state(OTA_STATE_DOWNLOADING);
            }
            else if (wifi_has_failed())
            {
                lcd_show_message("WiFi Failed");
                ota_set_state(OTA_STATE_FAILED);
            }
            else
            {
                lcd_show_message("WiFi Connecting");
            }
            break;

        case OTA_STATE_DOWNLOADING:
            if (!s_ota_started)
            {
                lcd_show_progress_bar(0, "Downloading");
                ota_update_start();
                s_ota_started = true;
                break;
            }

            if (upd.status == OTA_UPD_RUNNING)
            {
                lcd_show_progress_bar(upd.progress_percent, "Downloading");
                break;
            }

            if (upd.status == OTA_UPD_NO_UPDATE)
            {
                lcd_show_message("No Update");
                ota_set_state(OTA_STATE_SUCCESS);
            }
            else if (upd.status == OTA_UPD_SUCCESS)
            {
                lcd_show_message("Update OK");
                ota_set_state(OTA_STATE_SUCCESS); // reboot happens in update manager
            }
            else if (upd.status == OTA_UPD_FAILED)
            {
                char msg[21];
                snprintf(msg, sizeof(msg), "Fail: %s", ota_diag_error_short_str((uint16_t)upd.error));
                lcd_show_message(msg);
                ota_set_state(OTA_STATE_FAILED);
            }
            break;

        case OTA_STATE_SUCCESS:
            lcd_show_message("OTA Ready");
            break;

        case OTA_STATE_FAILED:
            // keep last message on LCD
            break;

        default:
            ota_set_state(OTA_STATE_FAILED);
            break;
    }
}
