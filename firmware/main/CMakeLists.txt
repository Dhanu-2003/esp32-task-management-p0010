idf_component_register(
  SRCS "app_main.c" "task_store.c" "time_util.c" "wifi_apsta.c" "api.c"
  INCLUDE_DIRS "."
  REQUIRES nvs_flash esp_http_server esp_wifi esp_netif esp_event esp_timer espressif__cjson
  EMBED_TXTFILES "index.html")
