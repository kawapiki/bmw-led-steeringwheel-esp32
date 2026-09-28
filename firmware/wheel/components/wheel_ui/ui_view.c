#include "ui_view.h"
#include "ui_graphics.h"
#include <stdio.h>
#include <string.h>
#define BG 0x090d1c
#define SURFACE 0x141b32
#define FG 0xf1f5ff
#define MUTED 0xa9b7d0
#define CYAN 0x35e4ff
#define AMBER 0xffc857
static lv_obj_t *title, *badge, *body, *bar, *footer, *value, *unit,
    *provenance;
static lv_obj_t *gear_value, *gear_unit, *rpm_unit, *coolant, *oil;
static bool last_overlay;
static lv_obj_t *rows[3], *selection, *qr_frame, *qr, *topline, *bottomline;
static ui_screen_t last_screen = (ui_screen_t)99;
static unsigned last_item = 99;
static bool qr_layout, qr_valid;
static char qr_password[17];
static const char *services[] = {
    "Update",        "LED test",     "Buttons / haptics",
    "Motion sensor", "Bluetooth",    "Performance",
    "Update mode",   "Forget Wi-Fi", "About"};
const char *ui_service_name(unsigned item) {
  return services[item % UI_SERVICE_COUNT];
}
static void show(lv_obj_t *o, bool visible) {
  if (visible == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
    return;
  if (visible)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void text(lv_obj_t *o, const char *s) {
  if (strcmp(lv_label_get_text(o), s))
    lv_label_set_text(o, s);
}
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, uint32_t color) {
  lv_obj_t *o = lv_label_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, w);
  lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
  lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
  return o;
}
static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h,
                      uint32_t color) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
void ui_view_create(lv_obj_t *screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(BG), 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(FG), 0);
  lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  ui_graphics_create(screen);
  topline = rect(screen, 12, 33, 296, 1, 0x26304b);
  bottomline = rect(screen, 12, 146, 296, 1, 0x26304b);
  title = label(screen, 12, 8, 230, FG);
  badge = label(screen, 240, 8, 68, FG);
  lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_RIGHT, 0);
  body = label(screen, 12, 44, 296, FG);
  lv_obj_set_style_max_height(body, 96, 0);
  bar = lv_bar_create(screen);
  lv_obj_set_pos(bar, 12, 44);
  lv_obj_set_size(bar, 296, 14);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x222b42), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(CYAN), LV_PART_INDICATOR);
  lv_obj_set_style_anim_duration(bar, 140, 0);
  lv_bar_set_range(bar, 0, 8000);

  value = label(screen, 12, 80, 162, FG);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_40, 0);
  unit = label(screen, 175, 107, 40, MUTED);
  text(unit, "rpm");
  provenance = label(screen, 226, 87, 82, MUTED);
  gear_value = label(screen, 143, 65, 34, FG);
  gear_unit = label(screen, 141, 101, 40, MUTED);
  rpm_unit = label(screen, 210, 108, 60, MUTED);
  coolant = label(screen, 20, 148, 138, CYAN);
  oil = label(screen, 181, 148, 136, 0xf35ac8);
  text(gear_unit, "Gear");
  text(rpm_unit, "rpm");
  footer = label(screen, 12, 153, 296, MUTED);
  selection = rect(screen, 12, 75, 296, 27, SURFACE);
  lv_obj_set_style_border_color(selection, lv_color_hex(CYAN), 0);
  lv_obj_set_style_border_side(selection, LV_BORDER_SIDE_LEFT, 0);
  lv_obj_set_style_border_width(selection, 3, 0);
  for (unsigned i = 0; i < 3; i++)
    rows[i] = label(screen, 26, 49 + i * 31, 274, i == 1 ? FG : MUTED);
  /* Keep the qualified 148px QR plus 8px extra white quiet-zone frame. */
  qr_frame = rect(screen, 0, 4, 164, 164, 0xffffff);
  qr = lv_qrcode_create(qr_frame);
  lv_qrcode_set_size(qr, 148);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_set_quiet_zone(qr, true);
  lv_obj_set_pos(qr, 8, 8);
  show(qr_frame, false);
}
static bool update_qr(const ui_view_state_t *s) {
  const char *password = s->password ? s->password : "";
  if (!password[0]) {
    if (qr_password[0]) {
      memset(qr_password, 0, sizeof(qr_password));
      lv_canvas_fill_bg(qr, lv_color_white(), LV_OPA_COVER);
      qr_valid = false;
    }
    return false;
  }
  if (s->nav.screen != UI_DETAIL || s->nav.item != 0 || s->offer || s->writing)
    return false;
  if (strcmp(qr_password, password)) {
    qr_valid = false;
    if (strlen(password) == 16 &&
        strspn(password, "abcdefghijklmnopqrstuvwxyz23456789") == 16) {
      char payload[80];
      int n = snprintf(payload, sizeof(payload),
                       "WIFI:T:WPA;S:BMW-Wheel;P:%s;;", password);
      qr_valid = n > 0 && n < (int)sizeof(payload) &&
                 lv_qrcode_update(qr, payload, n) == LV_RESULT_OK;
      memset(payload, 0, sizeof(payload));
    }
    snprintf(qr_password, sizeof(qr_password), "%s", password);
  }
  return qr_valid;
}
static void slide(void *obj, int32_t x) { lv_obj_set_x(obj, x); }

static void position(lv_obj_t *o, int x, int y, int width,
                     const lv_font_t *font, lv_text_align_t align) {
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, width);
  lv_obj_set_style_text_font(o, font, 0);
  lv_obj_set_style_text_align(o, align, 0);
}
void ui_view_render(const ui_view_state_t *s) {
  bool drive = s->nav.screen == UI_ENGINE, sport = s->nav.screen == UI_SHIFT,
       operational = drive || sport;
  bool menu = s->nav.screen == UI_MENU,
       update = s->nav.screen == UI_DETAIL && s->nav.item == 0;
  bool use_qr = update_qr(s), door = s->doors.visible;
  bool changed = last_screen != s->nav.screen || last_item != s->nav.item ||
                 qr_layout != use_qr || last_overlay != door;
  ui_graphics_render(s);
  if (changed) {
    lv_anim_delete(rows[1], slide);
    lv_obj_set_x(rows[1], 26);
    if (menu && !s->writing) {
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, rows[1]);
      lv_anim_set_exec_cb(&a, slide);
      lv_anim_set_values(&a, 34, 26);
      lv_anim_set_duration(&a, 140);
      lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
      lv_anim_start(&a);
    }
    show(value, operational);
    show(unit, operational);
    show(provenance, operational && !door);
    show(rpm_unit, operational && !door);
    show(gear_value, operational);
    show(gear_unit, operational);
    show(coolant, operational && !door);
    show(oil, operational && !door);
    show(body, door || (!operational && !menu));
    show(selection, menu);
    for (unsigned i = 0; i < 3; i++)
      show(rows[i], menu);
    show(qr_frame, use_qr);
    show(topline, !use_qr && !operational);
    show(bottomline, !use_qr && !operational);
    show(badge, !use_qr);
    show(footer, !operational || door);
    lv_obj_set_pos(title, use_qr ? 173 : 12, 8);
    lv_obj_set_width(title, use_qr ? 147 : 220);
    lv_obj_set_pos(body, use_qr ? 173 : 12, door ? 30 : 44);
    lv_obj_set_width(body, use_qr ? 147 : door ? 140 : 296);
    lv_obj_set_style_max_height(body, use_qr ? 101 : door ? 36 : 96, 0);
    lv_obj_set_pos(footer, use_qr ? 173 : 12, 153);
    lv_obj_set_width(footer, use_qr ? 147 : door ? 140 : 296);
    lv_obj_set_pos(bar, 12, 125);
    lv_obj_set_size(bar, 296, 10);
    lv_bar_set_range(bar, 0, 100);
    if (door) {
      position(value, 12, 64, 130, &lv_font_montserrat_40, LV_TEXT_ALIGN_LEFT);
      position(unit, 12, 107, 100, &lv_font_montserrat_14, LV_TEXT_ALIGN_LEFT);
      position(gear_value, 70, 120, 60, &lv_font_montserrat_28,
               LV_TEXT_ALIGN_LEFT);
      position(gear_unit, 12, 128, 50, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_LEFT);
    } else if (drive) {
      position(value, 20, 64, 120, &lv_font_montserrat_40,
               LV_TEXT_ALIGN_CENTER);
      position(unit, 43, 108, 75, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
      position(provenance, 185, 68, 113, &lv_font_montserrat_28,
               LV_TEXT_ALIGN_CENTER);
      position(rpm_unit, 205, 108, 75, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_CENTER);
      position(gear_value, 143, 64, 35, &lv_font_montserrat_20,
               LV_TEXT_ALIGN_CENTER);
      position(gear_unit, 140, 100, 41, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_CENTER);
    } else if (sport) {
      position(value, 103, 68, 113, &lv_font_montserrat_40,
               LV_TEXT_ALIGN_CENTER);
      position(unit, 120, 110, 80, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_CENTER);
      position(provenance, 237, 78, 75, &lv_font_montserrat_20,
               LV_TEXT_ALIGN_CENTER);
      position(rpm_unit, 237, 107, 75, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_CENTER);
      position(gear_value, 16, 77, 60, &lv_font_montserrat_28,
               LV_TEXT_ALIGN_LEFT);
      position(gear_unit, 16, 108, 50, &lv_font_montserrat_14,
               LV_TEXT_ALIGN_LEFT);
    }
    last_screen = s->nav.screen;
    last_item = s->nav.item;
    qr_layout = use_qr;
    last_overlay = door;
  }
  show(bar, update && s->writing && !use_qr);
  text(title, operational
                  ? (door              ? "Vehicle alert"
                     : s->doors.active ? (drive ? "Drive / Closure alert"
                                                : "Sport / Closure alert")
                     : drive           ? "Drive"
                                       : "Sport")
              : s->nav.screen == UI_GATEWAY ? "Gateway"
              : s->nav.screen == UI_DETAIL  ? ui_service_name(s->nav.item)
                                            : "Service");
  const char *state = s->reading.status == UI_DATA_OFFLINE        ? "Offline"
                      : s->reading.status == UI_DATA_INCOMPATIBLE ? "Version"
                      : s->reading.status == UI_DATA_STALE        ? "Stale"
                      : s->reading.status == UI_DATA_INVALID      ? "No data"
                      : s->reading.status == UI_DATA_DEMO         ? "Demo"
                                                                  : "Live";
  text(badge, s->nav.screen <= UI_GATEWAY ? state : update ? "OTA" : "Tools");
  if (operational) {
    char number[32], gear[4];
    if (s->reading.valid_fields & UI_VALID_SPEED)
      snprintf(number, sizeof(number), "%u",
               (s->reading.speed_dkph + 5u) / 10u);
    else
      strcpy(number, "--");
    text(value, number);
    text(unit, "km/h");
    if (s->reading.valid_fields & UI_VALID_RPM)
      snprintf(number, sizeof(number), "%lu", (unsigned long)s->reading.rpm);
    else
      strcpy(number, "--");
    text(provenance, number);
    ui_gear_text(gear, s->reading.gear,
                 (s->reading.valid_fields & UI_VALID_GEAR) != 0);
    text(gear_value, gear);
    if (s->reading.valid_fields & UI_VALID_COOLANT)
      snprintf(number, sizeof(number), "Coolant %d °C", s->reading.coolant_c);
    else
      strcpy(number, "Coolant --");
    text(coolant, number);
    if (s->reading.valid_fields & UI_VALID_OIL)
      snprintf(number, sizeof(number), "Oil %d °C", s->reading.oil_c);
    else
      strcpy(number, "Oil --");
    text(oil, number);
    if (door) {
      const char *message = "Multiple open";
      switch (s->doors.open) {
      case 1:
        message = "Front left open";
        break;
      case 2:
        message = "Front right open";
        break;
      case 4:
        message = "Rear left open";
        break;
      case 8:
        message = "Rear right open";
        break;
      case 16:
        message = "Trunk open";
        break;
      case 32:
        message = "Hood open";
        break;
      default:
        if (!(s->doors.open & 48))
          message = "Doors open";
        break;
      }
      text(body,
           s->doors.known == 63 ? message : "Last known /\nstate unknown");
      text(footer, "K1 / K2: dismiss");
    }
  } else if (menu) {
    text(rows[0], ui_service_name((s->nav.item + UI_SERVICE_COUNT - 1) %
                                  UI_SERVICE_COUNT));
    text(rows[1], ui_service_name(s->nav.item));
    text(rows[2], ui_service_name((s->nav.item + 1) % UI_SERVICE_COUNT));
  } else if (update && !use_qr && !s->offer && !s->writing && s->password &&
             s->password[0]) {
    char fallback[128];
    snprintf(fallback, sizeof(fallback),
             "Join BMW-Wheel\nPassword: %.16s\nThen open 192.168.4.1",
             s->password);
    text(body, fallback);
    memset(fallback, 0, sizeof(fallback));
  } else
    text(body, use_qr      ? "BMW-Wheel\nScan to join\nThen open:\n192.168.4.1"
               : s->detail ? s->detail
                           : "");
  if (update && s->writing)
    lv_bar_set_value(bar, s->progress, LV_ANIM_OFF);
  if (!door)
    text(footer, use_qr ? "Hold K1: back" : s->footer ? s->footer : "");
}
