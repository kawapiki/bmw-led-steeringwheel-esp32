#include "wheel_ui.h"
#include "demo.h"
#include "display_port.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "wheel_core.h"
#include <stdio.h>
#include <string.h>
static lv_obj_t *title, *body, *bar, *footer;
static unsigned page;
static uint64_t last_ui, opened;
static bool armed;
static const char *names[] = {
    "DEMO / RPM",    "LED strips", "Buttons / haptics",
    "Motion sensor", "Bluetooth",  "Performance",
    "Update",        "About",      "Update mode",
    "Forget Wi-Fi"};
static void status(demo_state_t *s, void *a) {
  (void)a;
  uint64_t now = demo_ms();
  s->ui_period_us = (now - last_ui) * 1000;
  s->ui_heartbeat = now;
  last_ui = now;
}
static void intent(intent_kind_t kind) {
  demo_state_t s;
  demo_get(&s);
  intent_t v = {.kind = kind,
                .release = s.candidate_release,
                .generation = s.candidate_generation,
                .standalone = s.standalone};
  memcpy(v.digest, s.candidate_digest, 32);
  xQueueSend(demo_intents, &v, 0);
}
static void led_mode(demo_state_t *s, void *a) {
  (void)a;
  s->led_mode = (s->led_mode + 1) % 5;
}
static void mode(demo_state_t *s, void *a) {
  (void)a;
  s->standalone = !s->standalone;
}
static void haptic(demo_state_t *s, void *a) {
  (void)a;
  s->haptic_test = true;
}
static void slide(void *obj, int32_t x) { lv_obj_set_x(obj, x); }
static void transition(void) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, body);
  lv_anim_set_exec_cb(&a, slide);
  lv_anim_set_values(&a, 28, 12);
  lv_anim_set_duration(&a, 140);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}
static void run(void *a) {
  (void)a;
  display_port_start();
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x07101c), 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xe9f3ff), 0);
  title = lv_label_create(screen);
  lv_obj_set_pos(title, 12, 8);
  body = lv_label_create(screen);
  lv_obj_set_pos(body, 12, 40);
  lv_obj_set_width(body, 296);
  lv_obj_set_height(body, 102);
  lv_label_set_long_mode(body, LV_LABEL_LONG_CLIP);
  bar = lv_bar_create(screen);
  lv_obj_set_pos(bar, 12, 112);
  lv_obj_set_size(bar, 296, 14);
  lv_bar_set_range(bar, 0, 7000);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x20d7ac), LV_PART_INDICATOR);
  lv_obj_set_style_anim_duration(bar, 160, 0);
  footer = lv_label_create(screen);
  lv_obj_set_pos(footer, 12, 150);
  lv_label_set_text(footer, "K1 next   K2 action   hold K1 back");
  uint64_t drawn = 0;
  uint32_t offer_seen = 0;
  for (;;) {
    demo_state_t s;
    demo_get(&s);
    if (s.candidate_generation != offer_seen || !s.offer) {
      offer_seen = s.candidate_generation;
      armed = false;
      opened = demo_ms();
    }
    demo_key_t key;
    while (xQueueReceive(demo_keys, &key, 0) == pdTRUE) {
      if (key == KEY_NEXT) {
        page = (page + 1) % 10;
        transition();
        opened = demo_ms();
        armed = false;
      } else if (key == KEY_BACK) {
        intent(INTENT_CANCEL);
        page = 0;
        armed = false;
      } else if (page == 9 && key == KEY_SELECT) {
        intent(INTENT_FORGET);
      } else if (page == 8 && key == KEY_SELECT) {
        demo_edit(mode, NULL);
      } else if (page == 1 && key == KEY_SELECT) {
        demo_edit(led_mode, NULL);
      } else if (page == 2 && key == KEY_SELECT) {
        demo_edit(haptic, NULL);
      } else if (page == 6 && key == KEY_SELECT) {
        intent(s.maintenance && s.network[0] && !s.ap_password[0]
                   ? INTENT_CHECK
                   : INTENT_SERVICE);
        opened = demo_ms();
        armed = false;
      } else if (page == 6 && key == KEY_CONFIRM && armed && s.offer) {
        intent(INTENT_CONFIRM);
        armed = false;
      }
    }
    if (page == 6 && s.offer && !s.pressed[1] && demo_ms() > opened + 50)
      armed = true;
    if (demo_ms() - drawn >= (s.writing ? 200 : 33)) {
      drawn = demo_ms();
      char text[240];
      lv_label_set_text(title, names[page]);
      lv_obj_set_style_opa(bar, page == 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      switch (page) {
      case 0:
        snprintf(text, sizeof(text),
                 "%lu RPM\nSimulated engine - no CAN control",
                 (unsigned long)s.rpm);
        lv_bar_set_value(bar, s.rpm, LV_ANIM_ON);
        break;
      case 1:
        snprintf(text, sizeof(text),
                 "2 x 23 RPM + 2 button LEDs\nK2: cycle chain / pixel / color "
                 "test\n10%% brightness limit");
        break;
      case 2:
        snprintf(text, sizeof(text),
                 "K1: %lu %s   K2: %lu %s\n20 ms haptic / bounded duty",
                 (unsigned long)s.buttons[0], s.pressed[0] ? "DOWN" : "UP",
                 (unsigned long)s.buttons[1], s.pressed[1] ? "DOWN" : "UP");
        break;
      case 3:
        if (s.sensor_ok)
          snprintf(
              text, sizeof(text),
              "Fusion %s / cal %u%u%u%u\nE deg %.0f %.0f %.0f\nA m/s2 %.1f "
              "%.1f %.1f\nG deg/s %.0f %.0f %.0f\nM uT %.0f %.0f %.0f",
              s.orientation_valid ? "ready" : "uncal", s.calibration >> 6,
              (s.calibration >> 4) & 3, (s.calibration >> 2) & 3,
              s.calibration & 3, s.euler[0] / 16., s.euler[1] / 16.,
              s.euler[2] / 16., s.accel[0] / 100., s.accel[1] / 100.,
              s.accel[2] / 100., s.gyro[0] / 16., s.gyro[1] / 16.,
              s.gyro[2] / 16., s.mag[0] / 16., s.mag[1] / 16., s.mag[2] / 16.);
        else
          snprintf(text, sizeof(text),
                   "Sensor unavailable\nIdentity / wiring not verified");
        break;
      case 4: {
        bool fresh = telemetry_is_fresh(s.link_secure, s.app_compatible,
                                        s.telemetry_received, demo_ms());
        snprintf(
            text, sizeof(text),
            "%s / app %s\n%s seq %lu / RPM %lu\nAge %llu ms; recovery separate",
            s.link_secure ? "Bonded link" : "Disconnected",
            s.app_compatible ? "compatible" : "unknown",
            fresh ? "CURRENT" : "STALE", (unsigned long)s.ble_sequence,
            (unsigned long)s.ble_rpm,
            (unsigned long long)(s.telemetry_received
                                     ? demo_ms() - s.telemetry_received
                                     : 0));
        break;
      }
      case 5:
        snprintf(text, sizeof(text),
                 "Flushes %lu / last %lu us\nUI period %lu us / drops "
                 "%lu\nHeap minimum %lu",
                 (unsigned long)s.flushes, (unsigned long)s.flush_us,
                 (unsigned long)s.ui_period_us, (unsigned long)s.input_drops,
                 (unsigned long)esp_get_minimum_free_heap_size());
        break;
      case 6:
        snprintf(text, sizeof(text), "%s\n%.74s\n%u%% / installed %lu\n%s",
                 s.network, s.update, s.progress,
                 (unsigned long)s.installed_release,
                 s.offer ? "Hold K2 to confirm" : s.ap_password);
        break;
      case 8:
        snprintf(text, sizeof(text),
                 "%s\nK2 changes mode explicitly\nPaired mode requires gateway "
                 "first",
                 s.standalone ? "STANDALONE wheel only"
                              : "PAIRED system update");
        break;
      case 9:
        snprintf(text, sizeof(text),
                 "K2: forget saved Wi-Fi\nBond / recovery identity retained");
        break;
      default: {
        const esp_app_desc_t *app = esp_app_get_description();
        snprintf(text, sizeof(text),
                 "%.24s / release %lu\nBuild %02x%02x%02x%02x / reset %d\nIDF "
                 "%.24s\nBoard "
                 "wiring unverified",
                 app->version, (unsigned long)s.installed_release,
                 app->app_elf_sha256[0], app->app_elf_sha256[1],
                 app->app_elf_sha256[2], app->app_elf_sha256[3],
                 esp_reset_reason(), app->idf_ver);
        break;
      }
      }
      lv_label_set_text(body, text);
    }
    lv_timer_handler();
    demo_edit(status, NULL);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void wheel_ui_start(void) {
  configASSERT(xTaskCreatePinnedToCore(run, "wheel_ui", 6144, NULL, 5, NULL,
                                       1) == pdPASS);
}
