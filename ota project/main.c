#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui/lcd_ui.h"
#include "network/wifi_manager.h"
#include "ota/ota_manager.h"
#include "ota/ota_state_machine.h"
#include "ota_update/ota_update_manager.h"

#include "storage/ota_diag.h"

void app_main(void)
{
    // Step 10: persistent OTA diagnostics + rollback/pending verify handling
    ota_diag_init();
    ota_diag_boot_check_and_update();

    lcd_init();
    wifi_manager_init();
    ota_init();
    ota_update_init();

    // Show last OTA result briefly
    ota_diag_record_t rec;
    if (ota_diag_get_last(&rec))
    {
        char msg[21];
        snprintf(msg, sizeof(msg), "Last: %s", ota_diag_status_str(rec.last_status));
        lcd_show_message(msg);
        vTaskDelay(pdMS_TO_TICKS(1200));

        if (rec.last_status == OTA_DIAG_STATUS_FAILED)
        {
            snprintf(msg, sizeof(msg), "Err: %s", ota_diag_error_short_str(rec.last_error));
            lcd_show_message(msg);
            vTaskDelay(pdMS_TO_TICKS(1200));
        }
    }

    while (1)
    {
        ota_state_machine_process();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
