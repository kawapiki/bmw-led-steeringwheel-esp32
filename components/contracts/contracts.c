#include "demo.h"
#include "esp_timer.h"
#include <string.h>
static demo_state_t state;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t demo_keys, demo_intents;
void demo_state_init(void) {
  demo_keys = xQueueCreate(16, sizeof(demo_key_t));
  demo_intents = xQueueCreate(8, sizeof(intent_t));
  configASSERT(demo_keys && demo_intents);
}
uint64_t demo_ms(void) { return esp_timer_get_time() / 1000; }
void demo_get(demo_state_t *out) {
  portENTER_CRITICAL(&lock);
  *out = state;
  portEXIT_CRITICAL(&lock);
}
void demo_edit(void (*fn)(demo_state_t *, void *), void *arg) {
  portENTER_CRITICAL(&lock);
  fn(&state, arg);
  portEXIT_CRITICAL(&lock);
}

void demo_clear(void *memory, size_t length) {
  volatile unsigned char *p = memory;
  while (length--)
    *p++ = 0;
}

#if CONFIG_IDF_TARGET_ESP32S3
static const char build_identity[64] = "BMWDEMO-ID:wheel_cvs8161:p1:c1:r1:END";
#else
static const char build_identity[64] =
    "BMWDEMO-ID:lilygo_xy32_v1_1:p1:c1:r1:END";
#endif
const char *demo_build_identity(void) { return build_identity; }
