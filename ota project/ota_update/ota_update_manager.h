#ifndef OTA_UPDATE_MANAGER_H
#define OTA_UPDATE_MANAGER_H

#include <stdbool.h>

typedef enum {
    OTA_UPD_IDLE = 0,
    OTA_UPD_RUNNING,
    OTA_UPD_NO_UPDATE,
    OTA_UPD_SUCCESS,
    OTA_UPD_FAILED
} ota_update_status_t;

typedef enum {
    OTA_ERR_NONE = 0,
    OTA_ERR_MANIFEST_FETCH = 1,
    OTA_ERR_MANIFEST_PARSE = 2,
    OTA_ERR_VERSION_NO_UPGRADE = 3,
    OTA_ERR_HTTP_OPEN = 4,
    OTA_ERR_HTTP_READ = 5,
    OTA_ERR_SIZE_MISMATCH = 6,
    OTA_ERR_SHA256_MISMATCH = 7,
    OTA_ERR_OTA_BEGIN = 8,
    OTA_ERR_OTA_WRITE = 9,
    OTA_ERR_OTA_END = 10,
    OTA_ERR_SET_BOOT = 11,
    OTA_ERR_ROLLBACK = 12
} ota_update_error_t;

typedef struct {
    ota_update_status_t status;
    ota_update_error_t  error;

    int progress_percent;     // 0..100
    int bytes_written;        // best-effort (int)
    int total_size;           // from manifest (int)

    char current_ver[32];
    char remote_ver[32];
    char last_error[64];
} ota_update_info_t;

void ota_update_init(void);
void ota_update_start(void);

ota_update_info_t ota_update_get_info(void);
bool ota_update_is_running(void);

#endif
