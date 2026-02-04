#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "ota_states.h"

void ota_init(void);
void ota_set_state(ota_state_t state);
ota_state_t ota_get_state(void);

#endif
