#include "ble_link.h"
#include "demo.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "motion.h"
#include "nvs_flash.h"
#include "service_wifi.h"
#include "update_manager.h"
#include "wheel_io.h"
#include "wheel_ui.h"
static void rpm(demo_state_t *s, void *a) {
  (void)a;
  uint32_t t = demo_ms() % 18000;
  s->rpm = t < 12000   ? 800 + t * 6200 / 12000
           : t < 14000 ? 7000
                       : 7000 - (t - 14000) * 6200 / 4000;
}
static void source(void *a) {
  (void)a;
  uint64_t end = demo_ms() + 10000;
  while (demo_ms() < end) {
    demo_edit(rpm, NULL);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  demo_state_t s;
  demo_get(&s);
  update_boot_validate(s.flushes > 0 && demo_ms() - s.input_heartbeat < 100 &&
                       demo_ms() - s.ui_heartbeat < 100 &&
                       esp_get_free_heap_size() > 20000);
  for (;;) {
    demo_edit(rpm, NULL);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  demo_state_init();
  wheel_io_start();
  wheel_ui_start();
  motion_start();
  service_wifi_init();
  ble_link_start(true);
  update_manager_start(true);
  configASSERT(xTaskCreatePinnedToCore(source, "demo_source", 3072, NULL, 3,
                                       NULL, 0) == pdPASS);
}
