#pragma once
#include "lvgl.h"
#include "ui_model.h"
typedef struct {
  ui_nav_t nav;
  ui_reading_t reading;
  ui_door_state_t doors;
  const char *detail, *footer, *password;
  bool writing, offer, boot;
  unsigned boot_frame;
  unsigned progress;
} ui_view_state_t;
void ui_view_create(lv_obj_t *screen);
void ui_view_render(const ui_view_state_t *state);
const char *ui_service_name(unsigned item);
