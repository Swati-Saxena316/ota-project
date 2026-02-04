#ifndef OTA_DIAG_H
#define OTA_DIAG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    OTA_DIAG_STATUS_UNKNOWN = 0,
    OTA_DIAG_STATUS_NO_UPDATE = 1,
    OTA_DIAG_STATUS_SUCCESS = 2,
    OTA_DIAG_STATUS_FAILED = 3
} ota_diag_status_t;

typedef struct {
    ota_diag_status_t last_status;
    uint16_t last_error;                // ota_update_error_t (stored as number)
    char last_attempt_ver[32];
    char last_installed_ver[32];
    uint8_t rollback_seen;
    uint32_t boot_count;
} ota_diag_record_t;

// Call once at startup (safe to call multiple times)
bool ota_diag_init(void);

// Boot-time checks:
// - detect rollback (if any)
// - if image is PENDING_VERIFY, mark valid & cancel rollback
// - store installed version on success
void ota_diag_boot_check_and_update(void);

// Record lifecycle events
void ota_diag_record_attempt(const char *attempt_version);
void ota_diag_record_result(ota_diag_status_t status,
                            uint16_t error_code,
                            const char *attempt_version,
                            const char *installed_version);

// Read last record
bool ota_diag_get_last(ota_diag_record_t *out);

// Human-friendly strings for LCD/debug
const char* ota_diag_status_str(ota_diag_status_t s);
const char* ota_diag_error_short_str(uint16_t err);  // short messages for LCD

#endif
