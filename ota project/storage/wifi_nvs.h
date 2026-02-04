#ifndef WIFI_NVS_H
#define WIFI_NVS_H

#include <stdbool.h>
#include <stddef.h>

bool wifi_nvs_save_creds(const char *ssid, const char *pass);
bool wifi_nvs_load_creds(char *ssid_out, size_t ssid_sz, char *pass_out, size_t pass_sz);
bool wifi_nvs_has_creds(void);
bool wifi_nvs_clear(void);

#endif
