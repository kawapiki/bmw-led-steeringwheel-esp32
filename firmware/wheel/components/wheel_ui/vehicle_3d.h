#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool vehicle_3d_init(void);
const lv_image_dsc_t *vehicle_3d_frame(uint64_t now_ms, bool boot,
                                       float boot_progress, uint8_t open_mask,
                                       uint8_t known_mask, uint8_t lights_valid,
                                       uint8_t lights_on);
void vehicle_3d_pause(void);
uint32_t vehicle_3d_generation(void);
#ifdef __cplusplus
}
#endif
