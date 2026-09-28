#pragma once
#include "lvgl.h"
#include "ui_vehicle_scene.h"
#include <stdbool.h>
#define UI_VEHICLE_ASSET_WIDTH 192u
#define UI_VEHICLE_ASSET_HEIGHT 104u
#define UI_VEHICLE_CLOSURE_FRAMES 64u
#define UI_VEHICLE_ASSET_BYTES                                                 \
  (UI_VEHICLE_ASSET_WIDTH * UI_VEHICLE_ASSET_HEIGHT * 2u)
/* Integrator-generated const RGB565 assets. Invalid indices return NULL.
 * The pixel data and descriptor remain alive in flash for the firmware
 * lifetime. */
const lv_image_dsc_t *ui_vehicle_asset(bool boot, unsigned frame);
static inline bool ui_vehicle_asset_valid(const lv_image_dsc_t *a) {
  return a && a->data && a->header.magic == LV_IMAGE_HEADER_MAGIC &&
         a->header.cf == LV_COLOR_FORMAT_RGB565 && a->header.flags == 0 &&
         a->header.w == UI_VEHICLE_ASSET_WIDTH &&
         a->header.h == UI_VEHICLE_ASSET_HEIGHT &&
         a->header.stride == UI_VEHICLE_ASSET_WIDTH * 2u &&
         a->data_size == UI_VEHICLE_ASSET_BYTES;
}
