#pragma once
#include <stdbool.h>
#include <stdint.h>
#define UI_VEHICLE_BOOT_FRAMES 28u
#define UI_VEHICLE_FRAME_MS 50u
#define UI_VEHICLE_BOOT_MS (UI_VEHICLE_BOOT_FRAMES * UI_VEHICLE_FRAME_MS)
typedef struct {
  uint64_t started_ms;
  unsigned frame;
  bool initialized, finished;
} ui_vehicle_scene_t;
/* One monotonic-time step; never queues frames or delays the UI task. */
bool ui_vehicle_scene_step(ui_vehicle_scene_t *scene, uint64_t now,
                           bool blocked);
/* Returns true only when this input was consumed to skip an active intro. */
bool ui_vehicle_scene_interrupt(ui_vehicle_scene_t *scene);
