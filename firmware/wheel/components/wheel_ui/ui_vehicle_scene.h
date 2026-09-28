#pragma once
#include <stdbool.h>
#include <stdint.h>
#define UI_VEHICLE_BOOT_MS 5000u
typedef struct {
  uint64_t started_ms;
  float progress;
  bool initialized, finished;
} ui_vehicle_scene_t;
/* One monotonic-time step; never queues frames or delays the UI task. */
bool ui_vehicle_scene_step(ui_vehicle_scene_t *scene, uint64_t now,
                           bool blocked);
/* Returns true only when this input was consumed to skip an active intro. */
bool ui_vehicle_scene_interrupt(ui_vehicle_scene_t *scene);

/* Briefly retain a just-closed alert scene to show physical panel closure. */
typedef struct {
  uint64_t until_ms;
  uint8_t previous_open;
  bool previous_visible;
} ui_vehicle_closing_t;
bool ui_vehicle_closing_step(ui_vehicle_closing_t *state, uint64_t now,
                             bool allowed, bool visible, uint8_t open,
                             uint8_t known);
