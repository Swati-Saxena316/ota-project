#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

void wifi_manager_init(void);
void wifi_manager_start(void);

bool wifi_credentials_available(void);
bool wifi_is_connected(void);
bool wifi_has_failed(void);

#endif
