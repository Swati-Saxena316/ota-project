#ifndef PROVISIONING_CONFIG_H
#define PROVISIONING_CONFIG_H

// SoftAP config required by spec: SSID, PASSWORD, IP_ADDRESS, Timeout :contentReference[oaicite:1]{index=1}
#define PROV_AP_SSID            "ESP32_SETUP"
#define PROV_AP_PASSWORD        "12345678"     // >=8 for WPA2
#define PROV_AP_CHANNEL         1
#define PROV_AP_MAX_CONN        4

// Captive portal IP
#define PROV_AP_IP_ADDR         "192.168.4.1"
#define PROV_AP_NETMASK         "255.255.255.0"
#define PROV_AP_GW_ADDR         "192.168.4.1"

// Provisioning timeout (spec says provisioning < 5 minutes) :contentReference[oaicite:2]{index=2}
#define PROV_TIMEOUT_MS         (5 * 60 * 1000)

// DNS server port
#define PROV_DNS_PORT           53

#endif
