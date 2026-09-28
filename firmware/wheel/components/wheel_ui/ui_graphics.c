#include "ui_graphics.h"
#include <string.h>
#define BG 0x090d1c
#define INACTIVE 0x263448
#define CYAN 0x35e4ff
#define MAGENTA 0xf35ac8
#define MUTED 0xa9b7d0
#define AMBER 0xffc857
static lv_obj_t *graphics[3], *lamps;
static ui_reading_t data;
static ui_door_state_t doors;
static ui_screen_t mode = (ui_screen_t)99;
static bool overlay, image_overlay;
static void line(lv_layer_t *layer, int x1, int y1, int x2, int y2,
                 uint32_t color, int width) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1 = (lv_point_precise_t){x1, y1};
  d.p2 = (lv_point_precise_t){x2, y2};
  d.color = lv_color_hex(color);
  d.width = width;
  lv_draw_line(layer, &d);
}
static void path(lv_layer_t *layer, int ox, int oy, const lv_point_t *p,
                 unsigned n, uint32_t color, int width) {
  for (unsigned i = 1; i < n; i++)
    line(layer, ox + p[i - 1].x, oy + p[i - 1].y, ox + p[i].x, oy + p[i].y,
         color, width);
}
static void box(lv_layer_t *layer, int x, int y, int w, int h, uint32_t color) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(color);
  d.bg_opa = LV_OPA_COVER;
  lv_area_t a = {x, y, x + w - 1, y + h - 1};
  lv_draw_rect(layer, &d, &a);
}
static void text(lv_layer_t *layer, int x, int y, int w, const char *s,
                 uint32_t color) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = s;
  d.font = &lv_font_montserrat_14;
  d.color = lv_color_hex(color);
  d.text_local = 1;
  lv_area_t a = {x, y, x + w - 1, y + 16};
  lv_draw_label(layer, &d, &a);
}
static void arc(lv_layer_t *layer, int cx, int cy, int radius, int from, int to,
                uint32_t color, int width) {
  if (to <= from)
    return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.center = (lv_point_t){cx, cy};
  d.radius = radius;
  d.start_angle = from;
  d.end_angle = to;
  d.color = lv_color_hex(color);
  d.width = width;
  lv_draw_arc(layer, &d);
}
static void draw_lamps(lv_event_t *e) {
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lamps, &a);
  for (unsigned i = 0; i < 6; ++i) {
    int x = a.x1 + i * 37, y = a.y1;
    bool known = (data.lights_valid & (1u << i)) != 0;
    bool on = (data.lights_on & (1u << i)) != 0;
    uint32_t color = !known || !on ? MUTED
                     : i == 1      ? 0x4488ff
                     : i == 5      ? 0xff445e
                                   : 0x48e589;
    if (!known)
      color = INACTIVE;
    if (i < 2) {
      arc(layer, x + 9, y + 10, 7, 270, 450, color, 2);
      line(layer, x + 9, y + 3, x + 9, y + 17, color, 2);
      for (int row = 0; row < 3; row++)
        line(layer, x + 1, y + 5 + row * 5, x + 6,
             y + 5 + row * 5 + (i == 0 ? 2 : 0), color, 1);
    } else if (i == 2) {
      arc(layer, x + 9, y + 10, 7, 0, 360, color, 2);
      arc(layer, x + 9, y + 10, 4, 0, 360, color, 1);
    } else if (i == 3 || i == 4) {
      int tip = x + (i == 3 ? 1 : 17), tail = x + (i == 3 ? 17 : 1),
          shoulder = x + 9;
      line(layer, tip, y + 10, tail, y + 10, color, 2);
      line(layer, tip, y + 10, shoulder, y + 3, color, 2);
      line(layer, tip, y + 10, shoulder, y + 17, color, 2);
    } else {
      arc(layer, x + 9, y + 10, 7, 0, 360, color, 2);
      line(layer, x + 9, y + 6, x + 9, y + 11, color, 2);
      box(layer, x + 8, y + 14, 2, 2, color);
    }
    if (!known)
      text(layer, x + 21, y + 2, 12, "?", AMBER);
  }
}
/* Rendering and invalidation share the same clamped, integer endpoint. */
static int gauge_angle(uint32_t value, bool rpm) {
  uint32_t maximum = rpm ? 8000u : 2400u;
  if (value > maximum)
    value = maximum;
  return 155 + (230u * value) / maximum;
}
static void invalidate_gauge_delta(unsigned id, uint32_t previous,
                                   uint32_t next) {
  if (!lv_obj_is_visible(graphics[id]))
    return;
  int first = gauge_angle(previous, id == 1), last = gauge_angle(next, id == 1);
  if (first == last)
    return;
  /* Large discontinuities redraw the whole dial; incremental clips are for
   * ordinary adjacent samples, not seek/reconnect jumps across tick sectors. */
  if (first > last + 24 || last > first + 24) {
    lv_obj_invalidate(graphics[id]);
    return;
  }
  if (first > last) {
    int swap = first;
    first = last;
    last = swap;
  }
  /* Match LVGL9.4's lv_arc invalidation, including the 360-degree boundary. */
  if (first > 360)
    first -= 360;
  if (last > 360)
    last -= 360;
  lv_area_t object, changed;
  lv_obj_get_coords(graphics[id], &object);
  lv_draw_arc_get_area(object.x1 + 72, object.y1 + 67, 67, first, last, 6,
                       false, &changed);
  /* Cover the software renderer's antialiased edge and integer rounding. */
  changed.x1 -= 4;
  changed.y1 -= 4;
  changed.x2 += 4;
  changed.y2 += 4;
  /* A clipped diagonal one-pixel tick can lose its antialiased endpoint in
   * the software renderer. Keep every intersected tick's full bounds together.
   */
  for (unsigned pass = 0; pass < 11; pass++) {
    bool expanded = false;
    for (int angle = 155; angle <= 385; angle += 23) {
      int sn = lv_trigo_sin(angle), cs = lv_trigo_cos(angle);
      int x1 = object.x1 + 72 + (56 * cs) / 32768,
          x2 = object.x1 + 72 + (60 * cs) / 32768;
      int y1 = object.y1 + 67 + (56 * sn) / 32768,
          y2 = object.y1 + 67 + (60 * sn) / 32768;
      lv_area_t tick = {LV_MIN(x1, x2) - 2, LV_MIN(y1, y2) - 2,
                        LV_MAX(x1, x2) + 2, LV_MAX(y1, y2) + 2};
      if (changed.x1 > tick.x2 || changed.x2 < tick.x1 ||
          changed.y1 > tick.y2 || changed.y2 < tick.y1)
        continue;
      lv_area_t before = changed;
      changed.x1 = LV_MIN(changed.x1, tick.x1);
      changed.y1 = LV_MIN(changed.y1, tick.y1);
      changed.x2 = LV_MAX(changed.x2, tick.x2);
      changed.y2 = LV_MAX(changed.y2, tick.y2);
      if (memcmp(&before, &changed, sizeof(changed)))
        expanded = true;
    }
    if (!expanded)
      break;
  }
  lv_obj_invalidate_area(graphics[id], &changed);
}
static void gauge(lv_layer_t *layer, int x, int y, bool rpm) {
  int cx = x + 72, cy = y + 67;
  bool valid = (data.valid_fields & (rpm ? UI_VALID_RPM : UI_VALID_SPEED)) != 0;
  uint32_t v = rpm ? data.rpm : data.speed_dkph;
  arc(layer, cx, cy, 67, 155, 385, INACTIVE, 6);
  if (valid)
    arc(layer, cx, cy, 67, 155, gauge_angle(v, rpm), rpm ? MAGENTA : CYAN, 6);
  for (int a = 155; a <= 385; a += 23) {
    int sn = lv_trigo_sin(a), cs = lv_trigo_cos(a);
    line(layer, cx + (56 * cs) / 32768, cy + (56 * sn) / 32768,
         cx + (60 * cs) / 32768, cy + (60 * sn) / 32768, MUTED, 1);
  }
  text(layer, x + 10, y + 100, 24, "0", MUTED);
  text(layer, x + 112, y + 100, 32, rpm ? "8" : "240", MUTED);
}
static void sport_rail(lv_layer_t *layer, int x, int y) {
  const lv_point_t outline[] = {{0, 21},   {26, 0},  {299, 0},
                                {299, 18}, {32, 18}, {12, 33}};
  path(layer, x, y, outline, 6, INACTIVE, 2);
  unsigned on = (data.valid_fields & UI_VALID_RPM)
                    ? (data.rpm > 8000 ? 30 : data.rpm * 30 / 8000)
                    : 0;
  for (unsigned i = 0; i < 30; i++)
    box(layer, x + 30 + i * 9, y + 3, 7, 12,
        i < on ? (i >= 24 ? MAGENTA : CYAN) : INACTIVE);
  text(layer, x + 30, y + 21, 20, "0", MUTED);
  text(layer, x + 94, y + 21, 20, "2", MUTED);
  text(layer, x + 157, y + 21, 20, "4", MUTED);
  text(layer, x + 220, y + 21, 20, "6", MUTED);
  text(layer, x + 282, y + 21, 20, "8", MUTED);
}
static void sport_frame(lv_layer_t *layer, int x, int y) {
  const lv_point_t left[] = {{10, 4}, {61, 4}, {79, 21}, {79, 59}};
  const lv_point_t midl[] = {{84, 58}, {84, 22}, {107, 1}};
  const lv_point_t midr[] = {{212, 1}, {236, 22}, {236, 58}};
  const lv_point_t right[] = {{241, 4}, {310, 4}, {310, 59}, {260, 59}};
  path(layer, x, y, left, 4, INACTIVE, 2);
  path(layer, x, y, midl, 3, 0x264c63, 2);
  path(layer, x, y, midr, 3, 0x513250, 2);
  path(layer, x, y, right, 4, INACTIVE, 2);
}
static void temperature(lv_layer_t *layer, int x, int y) {
  /* Original tiny thermometer and oil-drop contours; numeric labels are
   * separate. */
  line(layer, x + 7, y + 4, x + 7, y + 15, CYAN, 2);
  box(layer, x + 4, y + 14, 7, 5, CYAN);
  const lv_point_t drop[] = {{171, 1},  {165, 11}, {165, 15}, {169, 18},
                             {174, 17}, {177, 12}, {171, 1}};
  path(layer, x, y, drop, 7, MAGENTA, 2);
  for (unsigned i = 0; i < 2; i++) {
    bool valid = data.valid_fields & (i ? UI_VALID_OIL : UI_VALID_COOLANT);
    int v = i ? data.oil_c : data.coolant_c;
    int n = v < 0 ? 0 : v > 150 ? 130 : v * 130 / 150;
    box(layer, x + 20 + i * 160, y + 24, 130, 2, INACTIVE);
    if (valid && n)
      box(layer, x + 20 + i * 160, y + 24, n, 2, i ? MAGENTA : CYAN);
  }
}
static void car(lv_layer_t *layer, int x, int y) {
  const int cx = x + 80, top = y + 17;
  const lv_point_t body[] = {{-16, 0},  {16, 0},   {23, 12},
                             {26, 80},  {20, 109}, {-20, 109},
                             {-26, 80}, {-23, 12}, {-16, 0}};
  box(layer, cx - 20, top + 12, 40, 84, 0x182839);
  path(layer, cx, top, body, 9, MUTED, 2);
  const lv_point_t glass[] = {
      {-18, 23}, {18, 23}, {15, 41}, {-15, 41}, {-18, 23}};
  const lv_point_t rear[] = {
      {-15, 79}, {15, 79}, {18, 92}, {-18, 92}, {-15, 79}};
  path(layer, cx, top, glass, 5, 0x52788d, 2);
  path(layer, cx, top, rear, 5, 0x52788d, 2);
  const lv_point_t roof[] = {
      {-15, 45}, {15, 45}, {15, 73}, {-15, 73}, {-15, 45}};
  path(layer, cx, top, roof, 5, INACTIVE, 1);
  line(layer, cx - 14, top + 5, cx + 14, top + 5,
       (doors.open & 32) ? AMBER : 0xf1f5ff, 3);
  line(layer, cx - 16, top + 103, cx + 16, top + 103,
       (doors.open & 16) ? AMBER : MAGENTA, 3);
  text(layer, cx - 20, y, 52, (doors.known & 32) ? "Hood" : "Hood?",
       (doors.open & 32) || !(doors.known & 32) ? AMBER : MUTED);
  text(layer, cx - 20, top + 110, 52, (doors.known & 16) ? "Boot" : "Boot?",
       (doors.open & 16) || !(doors.known & 16) ? AMBER : MUTED);
  const char *names[] = {"FL", "FR", "RL", "RR"};
  for (unsigned i = 0; i < 4; i++) {
    int side = (i & 1) ? 1 : -1, dy = i < 2 ? 29 : 59, hinge = cx + side * 25;
    bool open = doors.open & (1u << i), known = doors.known & (1u << i);
    uint32_t color = open || !known ? AMBER : INACTIVE;
    if (open) {
      lv_point_t p[] = {
          {0, 0}, {side * 28, 11}, {side * 26, 27}, {0, 23}, {0, 0}};
      path(layer, hinge, top + dy, p, 5, color, 2);
    } else
      line(layer, hinge, top + dy, hinge, top + dy + 24, color, 2);
    text(layer, x + ((i & 1) ? 144 : 0), top + dy + 3, 23, names[i],
         open || !known ? AMBER : MUTED);
    if (!known)
      text(layer, hinge - 4, top + dy + 3, 10, "?", AMBER);
  }
}
static void draw(lv_event_t *e) {
  unsigned id = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  lv_area_t a;
  lv_obj_get_coords(graphics[id], &a);
  lv_layer_t *layer = lv_event_get_layer(e);
  if (overlay) {
    if (id == 2)
      car(layer, a.x1, a.y1);
    return;
  }
  if (id == 2)
    temperature(layer, a.x1, a.y1);
  else if (mode == UI_ENGINE)
    gauge(layer, a.x1, a.y1, id == 1);
  else if (id == 0)
    sport_rail(layer, a.x1, a.y1);
  else
    sport_frame(layer, a.x1, a.y1);
}
void ui_graphics_create(lv_obj_t *screen) {
  lamps = lv_obj_create(screen);
  lv_obj_remove_style_all(lamps);
  lv_obj_set_pos(lamps, 12, 3);
  lv_obj_set_size(lamps, 222, 21);
  lv_obj_remove_flag(lamps, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(lamps, draw_lamps, LV_EVENT_DRAW_MAIN, NULL);
  lv_obj_add_flag(lamps, LV_OBJ_FLAG_HIDDEN);
  for (unsigned i = 0; i < 3; i++) {
    graphics[i] = lv_obj_create(screen);
    lv_obj_remove_style_all(graphics[i]);
    lv_obj_remove_flag(graphics[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(graphics[i], draw, LV_EVENT_DRAW_MAIN,
                        (void *)(uintptr_t)i);
    lv_obj_add_flag(graphics[i], LV_OBJ_FLAG_HIDDEN);
  }
}
static void visible(lv_obj_t *o, bool v) {
  if (v)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
void ui_graphics_render(const ui_view_state_t *s, bool vehicle_image) {
  bool operational = s->nav.screen == UI_ENGINE || s->nav.screen == UI_SHIFT;
  bool changed = mode != s->nav.screen || overlay != s->doors.visible ||
                 image_overlay != vehicle_image;
  if (changed) {
    visible(lamps, operational);
    mode = s->nav.screen;
    overlay = s->doors.visible;
    image_overlay = vehicle_image;
    visible(graphics[0], operational && !overlay);
    visible(graphics[1], operational && !overlay);
    visible(graphics[2], operational && !(overlay && image_overlay));
    if (overlay) {
      lv_obj_set_pos(graphics[2], 153, 22);
      lv_obj_set_size(graphics[2], 167, 146);
    } else {
      lv_obj_set_pos(graphics[2], 0, 142);
      lv_obj_set_size(graphics[2], 320, 30);
      if (mode == UI_ENGINE) {
        lv_obj_set_pos(graphics[0], 8, 24);
        lv_obj_set_size(graphics[0], 145, 116);
        lv_obj_set_pos(graphics[1], 169, 24);
        lv_obj_set_size(graphics[1], 151, 116);
      } else {
        lv_obj_set_pos(graphics[0], 10, 26);
        lv_obj_set_size(graphics[0], 300, 39);
        lv_obj_set_pos(graphics[1], 0, 62);
        lv_obj_set_size(graphics[1], 320, 63);
      }
    }
  }
  bool validity_changed = data.valid_fields != s->reading.valid_fields;
  if (mode == UI_ENGINE && !overlay) {
    for (unsigned id = 0; id < 2; id++) {
      unsigned field = id == 0 ? UI_VALID_SPEED : UI_VALID_RPM;
      if (changed || ((data.valid_fields ^ s->reading.valid_fields) & field))
        lv_obj_invalidate(graphics[id]);
      else if (s->reading.valid_fields & field)
        invalidate_gauge_delta(id, id == 0 ? data.speed_dkph : data.rpm,
                               id == 0 ? s->reading.speed_dkph
                                       : s->reading.rpm);
    }
  } else {
    if (changed || validity_changed || data.rpm != s->reading.rpm)
      lv_obj_invalidate(graphics[0]);
    /* The angular centre frame stays static between layout changes. */
    if (changed)
      lv_obj_invalidate(graphics[1]);
  }
  if (changed ||
      (overlay ? doors.open != s->doors.open || doors.known != s->doors.known
               : validity_changed || data.coolant_c != s->reading.coolant_c ||
                     data.oil_c != s->reading.oil_c))
    lv_obj_invalidate(graphics[2]);
  if (changed || data.lights_valid != s->reading.lights_valid ||
      data.lights_on != s->reading.lights_on)
    lv_obj_invalidate(lamps);
  data = s->reading;
  doors = s->doors;
}
