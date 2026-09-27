#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "wheel_core.h"
#include "wheel_board.h"

static void demo_task(void *unused)
{
    (void)unused;
    const TickType_t period = pdMS_TO_TICKS(1000);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint32_t phase = (uint32_t)(esp_timer_get_time() / 1000) % 18000u;
        uint32_t rpm = phase < 12000 ? 800 + phase * 6200u / 12000u :
                       phase < 14000 ? 7000 : 7000 - (phase - 14000) * 6200u / 4000u;
        ESP_LOGI("demo", "rpm=%" PRIu32 " rpm_mask=0x%06" PRIx32 " heap=%" PRIu32,
                 rpm, rpm_mask(rpm), esp_get_free_heap_size());
        vTaskDelayUntil(&last, period);
    }
}

void app_main(void)
{
    /* Outputs remain quiescent during foundation bring-up. */
    gpio_config_t motor = {
        .pin_bit_mask = 1ULL << WHEEL_MOTOR,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&motor));
    ESP_ERROR_CHECK(gpio_set_level(WHEEL_MOTOR, 0));
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI("wheel", "Wheel demo foundation %s; IDF %s; core=%d",
             app->version, app->idf_ver, xPortGetCoreID());
    ESP_LOGW("wheel", "Foundation only: display/IO/network OTA not implemented yet");
    configASSERT(xTaskCreatePinnedToCore(demo_task, "demo_source", 4096, NULL, 3, NULL, 0) == pdPASS);
}
