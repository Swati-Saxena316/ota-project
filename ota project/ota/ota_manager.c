#include "ota_manager.h"
#include <stdio.h>

static ota_state_t current_state = OTA_STATE_IDLE;

void ota_init(void)
{
    current_state = OTA_STATE_IDLE;
}

void ota_set_state(ota_state_t state)
{
    if (current_state != state)
    {
        current_state = state;
        printf("[OTA] State changed to: %d\n", state);
    }
}

ota_state_t ota_get_state(void)
{
    return current_state;
}
