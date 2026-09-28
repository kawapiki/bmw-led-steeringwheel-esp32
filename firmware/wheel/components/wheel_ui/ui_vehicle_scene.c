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
    s->frame = 0;
  }
  if (now < s->started_ms || now - s->started_ms >= UI_VEHICLE_BOOT_MS) {
    s->finished = true;
    return false;
  }
  s->frame = (unsigned)((now - s->started_ms) / UI_VEHICLE_FRAME_MS);
  return true;
}
bool ui_vehicle_scene_interrupt(ui_vehicle_scene_t *s) {
  if (!s->initialized || s->finished)
    return false;
  s->finished = true;
  return true;
}
