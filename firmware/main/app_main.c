// TaskDeck on ESP32 — step-3 core: NVS -> task store -> Wi-Fi APSTA -> HTTP server.
// Step 4 adds: SSD1306 OLED + buttons.
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_server.h"
#include "nvs_flash.h"
#include "task_store.h"
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
    ESP_ERROR_CHECK(wifi_apsta_start());
    ESP_ERROR_CHECK(http_server_start());

    ESP_LOGI(TAG, "TASKDECK up ap=192.168.4.1 tasks=%d", task_store_count());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "TASKDECK heartbeat tasks=%d", task_store_count());
    }
}
