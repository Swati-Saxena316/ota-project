#include "provisioning_manager.h"

#include "config/provisioning_config.h"
#include "storage/wifi_nvs.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

static const char *TAG = "PROV";

static httpd_handle_t g_http = NULL;
static esp_netif_t *g_ap_netif = NULL;

static TaskHandle_t g_dns_task = NULL;
static TaskHandle_t g_timeout_task = NULL;

static volatile bool g_done = false;
static volatile bool g_failed = false;

static volatile int64_t g_start_ms = 0;

// simple helpers
static int64_t now_ms(void)
{
    return (int64_t)(esp_timer_get_time() / 1000);
}

/* ---------------- Captive Portal HTML ---------------- */

static const char *HTML_PAGE =
"<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32 WiFi Setup</title></head><body>"
"<h2>ESP32 WiFi Provisioning</h2>"
"<form method='POST' action='/save'>"
"<label>SSID</label><br><input name='ssid' maxlength='32' required><br><br>"
"<label>Password</label><br><input name='pass' type='password' maxlength='64' required><br><br>"
"<button type='submit'>Save</button>"
"</form><hr>"
"<h3>Network Test</h3>"
"<form method='POST' action='/test'>"
"<label>SSID</label><br><input name='ssid' maxlength='32' required><br><br>"
"<label>Password</label><br><input name='pass' type='password' maxlength='64' required><br><br>"
"<button type='submit'>Test Connect</button>"
"</form>"
"<p><a href='/status'>Status</a></p>"
"</body></html>";

/* ---------------- HTTP Utils ---------------- */

static void url_decode_inplace(char *s)
{
    // Minimal decode: '+' -> ' ', %xx
    char *o = s;
    while (*s)
    {
        if (*s == '+') { *o++ = ' '; s++; }
        else if (*s == '%' && s[1] && s[2])
        {
            char hex[3] = { s[1], s[2], 0 };
            *o++ = (char)strtol(hex, NULL, 16);
            s += 3;
        }
        else { *o++ = *s++; }
    }
    *o = 0;
}

static bool form_get_field(const char *body, const char *key, char *out, size_t out_sz)
{
    if (!body || !key || !out || out_sz == 0) return false;

    char pat[40];
    snprintf(pat, sizeof(pat), "%s=", key);

    const char *p = strstr(body, pat);
    if (!p) return false;
    p += strlen(pat);

    const char *end = strchr(p, '&');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    if (n >= out_sz) n = out_sz - 1;

    memcpy(out, p, n);
    out[n] = '\0';
    url_decode_inplace(out);
    return out[0] != '\0';
}

/* ---------------- HTTP Handlers ---------------- */

static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handle_status(httpd_req_t *req)
{
    char buf[256];
    int64_t elapsed = now_ms() - g_start_ms;
    int64_t left = (PROV_TIMEOUT_MS - elapsed);
    if (left < 0) left = 0;

    snprintf(buf, sizeof(buf),
             "{ \"done\": %s, \"failed\": %s, \"ms_left\": %lld }",
             g_done ? "true" : "false",
             g_failed ? "true" : "false",
             (long long)left);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handle_save(httpd_req_t *req)
{
    char body[512] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { g_failed = true; return ESP_FAIL; }
    body[len] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    if (!form_get_field(body, "ssid", ssid, sizeof(ssid)) ||
        !form_get_field(body, "pass", pass, sizeof(pass)))
    {
        httpd_resp_sendstr(req, "Invalid input");
        return ESP_OK;
    }

    if (!wifi_nvs_save_creds(ssid, pass))
    {
        g_failed = true;
        httpd_resp_sendstr(req, "Failed to save credentials");
        return ESP_OK;
    }

    g_done = true;
    httpd_resp_sendstr(req, "Saved! You can close this page. Device will connect.");
    ESP_LOGI(TAG, "Saved creds to NVS (ssid=%s)", ssid);
    return ESP_OK;
}

// Simple “network test”: attempt STA connect quickly with provided creds and report success/fail.
static esp_err_t handle_test(httpd_req_t *req)
{
    char body[512] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return ESP_FAIL;
    body[len] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    if (!form_get_field(body, "ssid", ssid, sizeof(ssid)) ||
        !form_get_field(body, "pass", pass, sizeof(pass)))
    {
        httpd_resp_sendstr(req, "Invalid input");
        return ESP_OK;
    }

    // Try connect in STA mode (temporary)
    wifi_config_t sta = {0};
    strncpy((char*)sta.sta.ssid, ssid, sizeof(sta.sta.ssid) - 1);
    strncpy((char*)sta.sta.password, pass, sizeof(sta.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_connect();

    // Wait up to 8 seconds for IP (simple test)
    bool ok = false;
    for (int i = 0; i < 16; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        // We check by querying STA netif IP (best-effort without extra event wiring)
        esp_netif_ip_info_t ip;
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip) == ESP_OK)
        {
            if (ip.ip.addr != 0) { ok = true; break; }
        }
    }

    // Return to AP-only (keep portal alive)
    esp_wifi_disconnect();
    esp_wifi_set_mode(WIFI_MODE_AP);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, ok ? "TEST_OK: Connected and got IP" : "TEST_FAIL: Could not get IP");
    return ESP_OK;
}

/* ---------------- HTTP Server ---------------- */

static void http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;   // helps captive portal style paths

    ESP_ERROR_CHECK(httpd_start(&g_http, &cfg));

    httpd_uri_t root = {.uri="/", .method=HTTP_GET, .handler=handle_root};
    httpd_uri_t status = {.uri="/status", .method=HTTP_GET, .handler=handle_status};
    httpd_uri_t save = {.uri="/save", .method=HTTP_POST, .handler=handle_save};
    httpd_uri_t test = {.uri="/test", .method=HTTP_POST, .handler=handle_test};

    httpd_register_uri_handler(g_http, &root);
    httpd_register_uri_handler(g_http, &status);
    httpd_register_uri_handler(g_http, &save);
    httpd_register_uri_handler(g_http, &test);

    // Wildcard handler: any unknown path redirects to "/"
    httpd_uri_t any = {.uri="/*", .method=HTTP_GET, .handler=handle_root};
    httpd_register_uri_handler(g_http, &any);

    ESP_LOGI(TAG, "HTTP portal started");
}

/* ---------------- SoftAP + Static IP ---------------- */

static void softap_start(void)
{
    wifi_config_t ap_cfg = {0};

    strncpy((char*)ap_cfg.ap.ssid, PROV_AP_SSID, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = 0;
    strncpy((char*)ap_cfg.ap.password, PROV_AP_PASSWORD, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.channel = PROV_AP_CHANNEL;
    ap_cfg.ap.max_connection = PROV_AP_MAX_CONN;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(PROV_AP_PASSWORD) == 0)
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    // Create AP netif (static IP)
    g_ap_netif = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t ip;
    ip.ip.addr = ipaddr_addr(PROV_AP_IP_ADDR);
    ip.gw.addr = ipaddr_addr(PROV_AP_GW_ADDR);
    ip.netmask.addr = ipaddr_addr(PROV_AP_NETMASK);

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(g_ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(g_ap_netif, &ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(g_ap_netif));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started SSID=%s IP=%s", PROV_AP_SSID, PROV_AP_IP_ADDR);
}

/* ---------------- Minimal DNS Server (wildcard A record) ----------------
   Replies to any query with A=PROV_AP_IP_ADDR.
*/

#pragma pack(push, 1)
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_hdr_t;
#pragma pack(pop)

static int dns_skip_qname(const uint8_t *msg, int len, int off)
{
    while (off < len && msg[off] != 0)
    {
        off += (int)msg[off] + 1;
    }
    return (off < len) ? off + 1 : len;
}

static void dns_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { g_failed = true; vTaskDelete(NULL); }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROV_DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        g_failed = true;
        vTaskDelete(NULL);
    }

    uint8_t rx[512];
    uint8_t tx[512];

    uint32_t ap_ip = ipaddr_addr(PROV_AP_IP_ADDR);

    while (!g_done && !g_failed)
    {
        struct sockaddr_in from = {0};
        socklen_t flen = sizeof(from);
        int r = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr*)&from, &flen);
        if (r <= 0) continue;

        if (r < (int)sizeof(dns_hdr_t)) continue;

        dns_hdr_t *h = (dns_hdr_t*)rx;
        uint16_t qd = ntohs(h->qdcount);
        if (qd == 0) continue;

        // Copy question section
        int off = sizeof(dns_hdr_t);
        int qname_end = dns_skip_qname(rx, r, off);
        if (qname_end + 4 > r) continue; // QTYPE+QCLASS

        int qlen = (qname_end + 4) - off;

        // Build response
        memset(tx, 0, sizeof(tx));
        dns_hdr_t *rh = (dns_hdr_t*)tx;
        rh->id = h->id;
        rh->flags = htons(0x8180); // standard query response, No error
        rh->qdcount = htons(1);
        rh->ancount = htons(1);

        int toff = sizeof(dns_hdr_t);
        memcpy(&tx[toff], &rx[off], qlen);
        toff += qlen;

        // Answer: name pointer to 0x0c
        tx[toff++] = 0xC0; tx[toff++] = 0x0C;
        // TYPE A, CLASS IN
        tx[toff++] = 0x00; tx[toff++] = 0x01;
        tx[toff++] = 0x00; tx[toff++] = 0x01;
        // TTL 60s
        tx[toff++] = 0x00; tx[toff++] = 0x00; tx[toff++] = 0x00; tx[toff++] = 0x3C;
        // RDLENGTH 4
        tx[toff++] = 0x00; tx[toff++] = 0x04;
        // RDATA IP
        memcpy(&tx[toff], &ap_ip, 4);
        toff += 4;

        sendto(sock, tx, toff, 0, (struct sockaddr*)&from, flen);
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ---------------- Timeout Task ---------------- */

static void timeout_task(void *arg)
{
    (void)arg;
    while (!g_done && !g_failed)
    {
        vTaskDelay(pdMS_TO_TICKS(250));
        if ((now_ms() - g_start_ms) > PROV_TIMEOUT_MS)
        {
            ESP_LOGE(TAG, "Provisioning timeout");
            g_failed = true;
            break;
        }
    }
    vTaskDelete(NULL);
}

/* ---------------- Public API ---------------- */

void provisioning_start(void)
{
    g_done = false;
    g_failed = false;

    g_start_ms = now_ms();

    softap_start();
    http_start();

    // DNS wildcard server (captive portal)
    xTaskCreate(dns_task, "prov_dns", 4096, NULL, 4, &g_dns_task);

    // Provisioning timeout
    xTaskCreate(timeout_task, "prov_to", 2048, NULL, 3, &g_timeout_task);

    ESP_LOGI(TAG, "Provisioning started (timeout=%d ms)", PROV_TIMEOUT_MS);
}

void provisioning_stop(void)
{
    if (g_http)
    {
        httpd_stop(g_http);
        g_http = NULL;
    }

    // Stop Wi-Fi
    esp_wifi_stop();

    // netif cleanup
    if (g_ap_netif)
    {
        esp_netif_destroy(g_ap_netif);
        g_ap_netif = NULL;
    }

    ESP_LOGI(TAG, "Provisioning stopped");
}

bool provisioning_is_done(void) { return g_done; }
bool provisioning_has_failed(void) { return g_failed; }
