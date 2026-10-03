// TaskDeck on ESP32 — step-2 scaffold placeholder.
// Step 3 replaces this with: NVS init -> task_store -> Wi-Fi APSTA -> HTTP server.
// Step 4 adds: SSD1306 OLED + buttons.
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "taskdeck";

void app_main(void)
{
    ESP_LOGI(TAG, "TASKDECK scaffold up (step 2). Core lands in step 3.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "TASKDECK heartbeat");
    }
}
