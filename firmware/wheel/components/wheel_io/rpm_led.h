#pragma once
#include <stdbool.h>
#include <stdint.h>
#define RPM_LED_COUNT 23u
#define RPM_LED_FULL_SCALE 6500u
#define RPM_LED_IDLE_LIMIT 1200u
typedef struct {
  uint8_t r, g, b;
} rpm_led_color_t;
typedef struct {
  float peak_rpm, fall_velocity;
  uint32_t previous_ms;
  bool initialized, shift;
} rpm_led_state_t;
/* Pure, bounded, allocation-free. Output excludes the two button pixels.
 * invalid/suppressed input resets the peak and clears all RPM pixels. */
void rpm_led_frame(rpm_led_state_t *state, uint32_t rpm, bool valid,
                   uint32_t now_ms, rpm_led_color_t pixels[RPM_LED_COUNT]);
