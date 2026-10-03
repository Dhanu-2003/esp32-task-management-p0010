// Wi-Fi APSTA: access point always on, plus optional STA client to home Wi-Fi.
#pragma once

#include "esp_err.h"

// Start Wi-Fi. AP uses Kconfig SSID/pass; STA uses credentials from NVS if set.
esp_err_t wifi_apsta_start(void);
// Store home-router credentials in NVS and (re)connect STA. ssid must be non-empty.
esp_err_t wifi_apsta_set_sta(const char *ssid, const char *pass);
// Caller frees *out_json with free(): {ap_ip, ap_clients, sta_ssid, sta_connected, sta_ip}.
esp_err_t wifi_apsta_status_json(char **out_json);
