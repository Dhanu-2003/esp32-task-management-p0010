idf_component_register(
  SRCS "app_main.c" "task_store.c" "time_util.c" "wifi_apsta.c" "api.c"
       "buttons.c" "ui_oled.c"
  INCLUDE_DIRS "."
  REQUIRES nvs_flash esp_http_server esp_wifi esp_netif esp_event esp_timer
           esp_driver_gpio espressif__cjson ssd1306
  EMBED_TXTFILES "index.html")
