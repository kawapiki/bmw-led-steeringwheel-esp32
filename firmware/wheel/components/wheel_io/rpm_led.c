#include "rpm_led.h"
#include <string.h>
static unsigned count(uint32_t rpm) {
  if (!rpm)
    return 1; /* A valid stopped engine has an amber status pixel. */
  return (rpm * RPM_LED_COUNT + RPM_LED_FULL_SCALE - 1u) / RPM_LED_FULL_SCALE;
}
static rpm_led_color_t color(unsigned p, bool idle) {
  if (idle ||
      (p + 1u) * RPM_LED_FULL_SCALE <= RPM_LED_IDLE_LIMIT * RPM_LED_COUNT)
    return (rpm_led_color_t){25, 18, 0};
  if (p >= RPM_LED_COUNT - 4u) {
    unsigned red = p - (RPM_LED_COUNT - 4u);
    return (rpm_led_color_t){(uint8_t)(7u + red * 6u),
                             (uint8_t)(18u - red * 6u), 0};
  }
  return (rpm_led_color_t){0, (uint8_t)(8u + (p - 4u) * 17u / 14u), 0};
}
void rpm_led_frame(rpm_led_state_t *s, uint32_t rpm, bool valid, uint32_t now,
                   rpm_led_color_t pixels[RPM_LED_COUNT]) {
  memset(pixels, 0, sizeof(*pixels) * RPM_LED_COUNT);
  if (!valid) {
    memset(s, 0, sizeof(*s));
    return;
  }
  if (rpm > RPM_LED_FULL_SCALE)
    rpm = RPM_LED_FULL_SCALE;
  if (!s->initialized) {
    s->initialized = true;
    s->peak_rpm = (float)rpm;
    s->previous_ms = now;
    s->fall_velocity = 0;
  }
  uint32_t elapsed = now - s->previous_ms;
  s->previous_ms = now;
  if ((float)rpm >= s->peak_rpm || elapsed >= 2000u) {
    s->peak_rpm = (float)rpm;
    s->fall_velocity = 0;
  } else {
    /* Constant acceleration: slow start, then faster. Analytic integration
     * makes decay time-based rather than dependent on rendering cadence. */
    float dt = elapsed * 0.001f;
    s->peak_rpm -= s->fall_velocity * dt + 4500.0f * dt * dt;
    s->fall_velocity += 9000.0f * dt;
    if (s->peak_rpm <= (float)rpm) {
      s->peak_rpm = (float)rpm;
      s->fall_velocity = 0;
    }
  }
  if (rpm >= 6500u)
    s->shift = true;
  else if (rpm < 6350u)
    s->shift = false;
  unsigned filled = count(rpm);
  if (!s->shift || ((now / 125u) & 1u))
    for (unsigned p = 0; p < filled; ++p)
      pixels[p] = color(p, rpm <= RPM_LED_IDLE_LIMIT);
  unsigned peak = count((uint32_t)s->peak_rpm) - 1u;
  if (peak >= filled)
    pixels[peak] = color(peak, false);
}
