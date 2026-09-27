#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
  INTENT_SERVICE,
  INTENT_CHECK,
  INTENT_CONFIRM,
  INTENT_CANCEL,
  INTENT_FORGET
} intent_kind_t;
typedef struct {
  intent_kind_t kind;
  uint32_t release, generation;
  uint8_t digest[32];
  bool standalone;
} intent_t;
typedef enum { KEY_NEXT, KEY_SELECT, KEY_BACK, KEY_CONFIRM } demo_key_t;
typedef struct {
  uint32_t rpm, buttons[2], input_drops, flushes, flush_us, ui_period_us;
  uint64_t input_heartbeat, ui_heartbeat, telemetry_received;
  bool pressed[2], sensor_ok, link_secure, maintenance, writing, offer;
  uint8_t calibration, progress, led_mode;
  bool haptic_test, io_quiet, standalone;
  uint32_t ble_sequence, ble_rpm;
  bool app_compatible;
  uint32_t candidate_release, candidate_generation, installed_release,
      update_phase;
  uint8_t candidate_digest[32];
  bool telemetry_fresh, orientation_valid;
  int16_t accel[3], gyro[3], mag[3], euler[3];
  char network[48], update[96], ap_password[17];
} demo_state_t;
extern QueueHandle_t demo_keys, demo_intents;
void demo_state_init(void);
void demo_get(demo_state_t *out);
void demo_edit(void (*fn)(demo_state_t *, void *), void *arg);
uint64_t demo_ms(void);

void demo_clear(void *memory, size_t length);

const char *demo_build_identity(void);
