#include "ble_link.h"
#include "demo.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "service_wifi.h"
#include "update_manager.h"
static void health(void *a) {
  (void)a;
  vTaskDelay(pdMS_TO_TICKS(3000));
  update_boot_validate(esp_get_free_heap_size() > 20000);
  for (;;) {
    ESP_LOGI("gateway", "CAN disabled; vehicle state unknown; recovery %s",
             ble_link_secure() ? "authenticated" : "disconnected");
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  demo_state_init();
  service_wifi_init();
  ble_link_start(false);
  update_manager_start(false);
  configASSERT(xTaskCreatePinnedToCore(health, "health", 3072, NULL, 1, NULL,
                                       0) == pdPASS);
}
