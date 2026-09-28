#include "ui_vehicle_scene.h"
bool ui_vehicle_scene_step(ui_vehicle_scene_t *s, uint64_t now, bool blocked) {
  if (s->finished)
    return false;
  if (blocked) {
    s->finished = true;
    return false;
  }
  if (!s->initialized) {
    s->initialized = true;
    s->started_ms = now;
    s->progress = 0;
  }
  if (now < s->started_ms || now - s->started_ms >= UI_VEHICLE_BOOT_MS) {
    s->finished = true;
    return false;
  }
  s->progress = (float)(now - s->started_ms) / (float)UI_VEHICLE_BOOT_MS;
  return true;
}
bool ui_vehicle_scene_interrupt(ui_vehicle_scene_t *s) {
  if (!s->initialized || s->finished)
    return false;
  s->finished = true;
  return true;
}

bool ui_vehicle_closing_step(ui_vehicle_closing_t *s, uint64_t now,
                             bool allowed, bool visible, uint8_t open,
                             uint8_t known) {
  if (!allowed) {
    *s = (ui_vehicle_closing_t){0};
    return false;
  }
  if (s->previous_visible && s->previous_open && !open && known == 63)
    s->until_ms = now + 1000;
  if (open || now >= s->until_ms)
    s->until_ms = 0;
  s->previous_open = open;
  s->previous_visible = visible;
  return s->until_ms != 0;
}
