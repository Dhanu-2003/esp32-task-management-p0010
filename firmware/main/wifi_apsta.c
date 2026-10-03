#include "wifi_apsta.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "time_util.h"

static const char *TAG = "wifi";

#define WIFI_NVS_NS "wifi"
#define MAX_SSID 32
#define MAX_PASS 64

static esp_netif_t *s_ap;
static esp_netif_t *s_sta;
static bool s_sta_connected = false;
static char s_sta_ip[16] = "";
static char s_ap_ip[16] = "192.168.4.1";
static char s_sta_ssid[MAX_SSID + 1] = "";
static int s_sta_fail_count = 0;

static void load_sta_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    ssid[0] = '\0';
    pass[0] = '\0';
    nvs_handle_t h;
    if (nvs_open(WIFI_NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t n = ssid_len;
    if (nvs_get_str(h, "ssid", ssid, &n) != ESP_OK) {
        ssid[0] = '\0';
    }
    n = pass_len;
    if (nvs_get_str(h, "pass", pass, &n) != ESP_OK) {
        pass[0] = '\0';
    }
    nvs_close(h);
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_sta_ssid[0]) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        s_sta_fail_count = 0;
        ESP_LOGI(TAG, "STA connected to %s", s_sta_ssid);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        s_sta_ip[0] = '\0';
        time_util_set_synced(false);
        if (s_sta_ssid[0] && ++s_sta_fail_count < 20) {
            // Reconnect a few times, then stay quiet on AP only.
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_wifi_connect();
        } else if (s_sta_fail_count == 20) {
            ESP_LOGW(TAG, "STA giving up, AP-only mode");
        }
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
    snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&ev->ip_info.ip));
    s_sta_connected = true;
    ESP_LOGI(TAG, "STA got IP %s", s_sta_ip);
    // Best-effort clock sync; offline fallback time keeps working without it.
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = NULL;
    if (esp_netif_sntp_init(&cfg) == ESP_OK) {
        time_util_set_synced(true);
    }
}

esp_err_t wifi_apsta_start(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");
    s_ap = esp_netif_create_default_wifi_ap();
    s_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    // Not ESP_ERROR_CHECK: the ESP32-P4 has no Wi-Fi radio of its own. When no
    // Wi-Fi is available (or the coprocessor is missing) we must keep running on
    // the OLED/buttons instead of aborting the whole device at boot.
    esp_err_t err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        s_ap = NULL;
        s_sta = NULL;
        ESP_LOGW(TAG, "no Wi-Fi radio available (%s); running on-device UI only",
                 esp_err_to_name(err));
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                           on_wifi_event, NULL, NULL),
                        TAG, "wifi handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                           on_ip_event, NULL, NULL),
                        TAG, "ip handler failed");

    char sta_pass[MAX_PASS + 1] = "";
    load_sta_creds(s_sta_ssid, sizeof(s_sta_ssid), sta_pass, sizeof(sta_pass));

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, CONFIG_TASK_AP_SSID, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(CONFIG_TASK_AP_SSID);
    ap_cfg.ap.max_connection = CONFIG_TASK_AP_MAX_CONN;
    if (strlen(CONFIG_TASK_AP_PASS) > 0) {
        strncpy((char *)ap_cfg.ap.password, CONFIG_TASK_AP_PASS,
                sizeof(ap_cfg.ap.password) - 1);
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_cfg = {0};
    if (s_sta_ssid[0]) {
        strncpy((char *)sta_cfg.sta.ssid, s_sta_ssid, sizeof(sta_cfg.sta.ssid) - 1);
        strncpy((char *)sta_cfg.sta.password, sta_pass, sizeof(sta_cfg.sta.password) - 1);
    }
    memset(sta_pass, 0, sizeof(sta_pass));

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "ap config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "sta config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    ESP_LOGI(TAG, "AP up ssid=%s ip=%s%s", CONFIG_TASK_AP_SSID, s_ap_ip,
             s_sta_ssid[0] ? " (+STA)" : " (STA not configured)");
    return ESP_OK;
}

esp_err_t wifi_apsta_set_sta(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0] || strlen(ssid) > MAX_SSID) {
        return ESP_ERR_INVALID_ARG;
    }
    if (pass && strlen(pass) > MAX_PASS) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(WIFI_NVS_NS, NVS_READWRITE, &h));
    esp_err_t err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "pass", pass ? pass : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        return err;
    }
    strncpy(s_sta_ssid, ssid, sizeof(s_sta_ssid) - 1);
    s_sta_fail_count = 0;
    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    if (pass) {
        strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    return ESP_OK;
}

esp_err_t wifi_apsta_status_json(char **out_json)
{
    wifi_sta_list_t clients = {0};
    if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK) {
        memset(&clients, 0, sizeof(clients));
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "ap_ip", s_ap_ip);
    cJSON_AddNumberToObject(root, "ap_clients", clients.num);
    cJSON_AddStringToObject(root, "sta_ssid", s_sta_ssid);
    cJSON_AddBoolToObject(root, "sta_connected", s_sta_connected);
    cJSON_AddStringToObject(root, "sta_ip", s_sta_ip);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    *out_json = json;
    return ESP_OK;
}
