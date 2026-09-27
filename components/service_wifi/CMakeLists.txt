idf_component_register(SRCS "service_wifi.c" INCLUDE_DIRS "include" REQUIRES contracts esp_wifi esp_event esp_netif esp_http_server nvs_flash lwip esp_timer)
