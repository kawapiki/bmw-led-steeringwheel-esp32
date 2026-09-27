#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
  uint8_t bytes[152];
  size_t size;
} recovery_message_t;
void ble_link_start(bool wheel);
bool ble_link_secure(void);
bool ble_link_send(const recovery_message_t *message);
bool ble_link_receive(recovery_message_t *message);
void ble_link_status(const uint8_t *data, size_t size);
bool ble_link_read(uint8_t *data, size_t *size);

void ble_link_poll_telemetry(void);
bool ble_link_ready(void);
