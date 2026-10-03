// TaskDeck on ESP32 — step-4 features: OLED + buttons live alongside the web UI.
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "task_store.h"
#include "ui_oled.h"
#include "wifi_apsta.h"

static const char *TAG = "taskdeck";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(task_store_init());

    // Wi-Fi is optional: an ESP32-P4 has no radio unless a coprocessor is wired
    // in. Without it the device still runs the full OLED + buttons interface.
    esp_err_t wifi_err = wifi_apsta_start();
    bool web_up = false;
    if (wifi_err == ESP_OK) {
        ESP_ERROR_CHECK(http_server_start());
        web_up = true;
    } else {
        ESP_LOGW(TAG, "web UI disabled (no Wi-Fi); OLED + buttons only");
    }
    ui_oled_start(); // continues headless if no display is wired

    ESP_LOGI(TAG, "TASKDECK up ap=%s tasks=%d web=%s",
             web_up ? "192.168.4.1" : "off", task_store_count(),
             web_up ? "on" : "off");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "TASKDECK heartbeat tasks=%d", task_store_count());
    }
}
