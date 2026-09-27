#pragma once
#include <stdint.h>
#include "esp_err.h"
/* Only the effects task may call these after startup. */
esp_err_t wheel_led_init(void);
void wheel_led_set(unsigned chain, unsigned pixel, uint8_t r, uint8_t g, uint8_t b);
esp_err_t wheel_led_refresh(unsigned chain);
