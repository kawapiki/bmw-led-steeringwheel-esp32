idf_component_register(SRCS "ble_link.c" "telemetry_v1.c" "telemetry_cockpit.c" INCLUDE_DIRS "include" "../../.private" REQUIRES bt nvs_flash contracts wheel_core esp_timer)
