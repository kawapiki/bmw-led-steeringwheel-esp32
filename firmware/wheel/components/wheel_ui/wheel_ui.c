#include "wheel_ui.h"
#include "cockpit.h"
#include "demo.h"
#include "display_port.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "ui_view.h"
#include "wheel_core.h"
#include <stdio.h>
#include <string.h>
static ui_nav_t nav;
static uint64_t last_ui;
static ui_confirmation_t confirmation;
static ui_door_state_t doors;
static uint64_t wait_started, wait_total;
static uint32_t flush_count;
static void profile_display(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FLUSH_WAIT_START)
    wait_started = esp_timer_get_time();
  else if (code == LV_EVENT_FLUSH_WAIT_FINISH)
    wait_total += esp_timer_get_time() - wait_started;
  else if (code == LV_EVENT_FLUSH_START)
    flush_count++;
}
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
static ui_offer_t offer(const demo_state_t *s) {
  ui_offer_t o = {.generation = s->candidate_generation,
                  .release = s->candidate_release,
                  .standalone = s->standalone};
  memcpy(o.digest, s->candidate_digest, 32);
  return o;
}
static void confirm(const demo_state_t *s) {
  ui_offer_t current = offer(s), shown;
  if (!ui_confirmation_take(&confirmation, &current, &shown))
    return;
  intent_t v = {.kind = INTENT_CONFIRM,
                .release = shown.release,
                .generation = shown.generation,
                .standalone = shown.standalone};
  memcpy(v.digest, shown.digest, 32);
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
static ui_reading_t reading(const demo_state_t *s) {
  ui_telemetry_t t = {
      .secure = s->link_secure,
      .compatible = s->app_compatible,
      .valid = s->telemetry_source != DEMO_SOURCE_NONE,
      .demo = s->telemetry_source == DEMO_SOURCE_GATEWAY_DEMO,
      .fresh = !s->maintenance && !s->writing &&
               telemetry_is_fresh(s->link_secure, s->app_compatible,
                                  s->telemetry_received, demo_ms()),
      .rpm = s->ble_rpm,
      .valid_fields = s->telemetry_valid & 31u,
      .speed_dkph = s->speed_dkph,
      .gear = s->gear,
      .closure_open = s->closure_open,
      .closure_known =
          (s->telemetry_valid >> COCKPIT_CLOSURE_SHIFT) & COCKPIT_CLOSURE_MASK,
      .coolant_c = s->coolant_c,
      .oil_c = s->oil_c};
  return ui_reading(&t);
}
static void detail(const demo_state_t *s, ui_reading_t r, char *text,
                   size_t cap) {
  if (nav.screen == UI_GATEWAY) {
    bool packet_current = r.status == UI_DATA_DEMO || r.status == UI_DATA_LIVE;
    const char *link = !s->link_secure      ? "Reconnecting"
                       : !s->app_compatible ? "Application incompatible"
                       : packet_current     ? "Authenticated / current"
                                            : "Connected / no fresh data";
    char rpm[16];
    if (r.available)
      snprintf(rpm, sizeof(rpm), "%lu", (unsigned long)r.rpm);
    else
      strcpy(rpm, "--");
    if (packet_current)
      snprintf(text, cap,
               "%s\nSource: %s\nRPM %s / age %llu ms\nCAN control disabled",
               link, r.status == UI_DATA_DEMO ? "gateway demo" : "vehicle", rpm,
               (unsigned long long)(demo_ms() - s->telemetry_received));
    else
      snprintf(text, cap,
               "%s\nRPM -- / source unavailable\nWaiting for gateway "
               "data\nRecovery link independent",
               link);
    return;
  }
  if (nav.screen == UI_SERVICE) {
    snprintf(text, cap,
             "Maintenance and diagnostics\nWi-Fi updates / component "
             "tests\n\nK2 opens the service menu");
    return;
  }
  if (nav.screen != UI_DETAIL) {
    text[0] = 0;
    return;
  }
  switch (nav.item) {
  case 0:
    snprintf(text, cap, "%.31s\n%.74s\n%u%% / installed %lu", s->network,
             s->update, s->progress, (unsigned long)s->installed_release);
    break;
  case 1:
    snprintf(text, cap,
             "2 x 23 RPM + 2 button LEDs\nMode %u / K2 cycles tests\nButton "
             "light: first pixel\n10%% brightness limit",
             s->led_mode);
    break;
  case 2:
    snprintf(text, cap,
             "K1 left: %lu %s\nK2 right: %lu %s\nK2 tests haptic feedback\n20 "
             "ms pulse / bounded duty",
             (unsigned long)s->buttons[0], s->pressed[0] ? "DOWN" : "UP",
             (unsigned long)s->buttons[1], s->pressed[1] ? "DOWN" : "UP");
    break;
  case 3:
    if (s->sensor_ok)
      snprintf(text, cap,
               "Fusion %s / cal %u%u%u%u\nE deg %.0f %.0f %.0f\nA m/s2 %.1f "
               "%.1f %.1f\nAxes not vehicle-qualified",
               s->orientation_valid ? "ready" : "uncal", s->calibration >> 6,
               (s->calibration >> 4) & 3, (s->calibration >> 2) & 3,
               s->calibration & 3, s->euler[0] / 16., s->euler[1] / 16.,
               s->euler[2] / 16., s->accel[0] / 100., s->accel[1] / 100.,
               s->accel[2] / 100.);
    else {
      snprintf(text, cap, "Sensor unavailable\nIdentity / wiring not verified");
    }
    break;
  case 4:
    snprintf(text, cap,
             "%s / app %s\n%s / sequence %lu\nRecovery protocol independent",
             s->link_secure ? "Bonded link" : "Disconnected",
             s->app_compatible ? "compatible" : "unknown",
             (r.status == UI_DATA_DEMO || r.status == UI_DATA_LIVE)
                 ? "CURRENT"
                 : "NO CURRENT DATA",
             (unsigned long)s->ble_sequence);
    break;
  case 5:
    snprintf(text, cap,
             "Flushes %lu / last %lu us\nUI period %lu us / drops %lu\nHeap "
             "minimum %lu",
             (unsigned long)s->flushes, (unsigned long)s->flush_us,
             (unsigned long)s->ui_period_us, (unsigned long)s->input_drops,
             (unsigned long)esp_get_minimum_free_heap_size());
    break;
  case 6:
    snprintf(text, cap,
             "%s\nK2 changes mode explicitly\nPaired: gateway updates first",
             s->standalone ? "STANDALONE wheel only" : "PAIRED system update");
    break;
  case 7:
    snprintf(text, cap,
             "K2: forget saved Wi-Fi\nBond / recovery identity retained");
    break;
  default: {
    const esp_app_desc_t *app = esp_app_get_description();
    snprintf(text, cap,
             "%.24s / release %lu\nBuild %02x%02x%02x%02x / reset %d\nIDF "
             "%.24s\nBoard wiring unverified",
             app->version, (unsigned long)s->installed_release,
             app->app_elf_sha256[0], app->app_elf_sha256[1],
             app->app_elf_sha256[2], app->app_elf_sha256[3], esp_reset_reason(),
             app->idf_ver);
    break;
  }
  }
}
static void run(void *a) {
  (void)a;
  lv_display_t *disp = display_port_start();
  lv_display_add_event_cb(disp, profile_display, LV_EVENT_ALL, NULL);
  ui_view_create(lv_screen_active());
  uint64_t drawn = 0, profile_window = esp_timer_get_time(), handler_total = 0;
  uint32_t loops = 0, handler_max = 0;
  ui_door_state_t logged_alert = {0};
  for (;;) {
    demo_state_t s;
    demo_get(&s);
    ui_reading_t current = reading(&s);
    ui_door_update(&doors, &current, nav.screen, s.maintenance || s.writing);
    if (!s.offer)
      ui_confirmation_show(&confirmation, NULL, demo_ms());
    demo_key_t key;
    while (xQueueReceive(demo_keys, &key, 0) == pdTRUE) {
      if (doors.visible &&
          (key == KEY_NEXT || key == KEY_SELECT || key == KEY_BACK)) {
        ui_door_acknowledge(&doors);
        drawn = 0;
        continue;
      }
      ui_nav_t previous = nav;
      if (key == KEY_BACK) {
        if (nav.screen == UI_DETAIL && nav.item == 0)
          intent(INTENT_CANCEL);
        ui_nav_key(&nav, UI_BACK);
      } else if (key == KEY_NEXT)
        ui_nav_key(&nav, UI_NEXT);
      else if (key == KEY_SELECT) {
        if (nav.screen != UI_DETAIL)
          ui_nav_key(&nav, UI_SELECT);
        else if (nav.item == 7)
          intent(INTENT_FORGET);
        else if (nav.item == 6)
          demo_edit(mode, NULL);
        else if (nav.item == 1)
          demo_edit(led_mode, NULL);
        else if (nav.item == 2)
          demo_edit(haptic, NULL);
        else if (nav.item == 0) {
          intent(s.maintenance && s.network[0] && !s.ap_password[0]
                     ? INTENT_CHECK
                     : INTENT_SERVICE);
          ui_confirmation_show(&confirmation, NULL, demo_ms());
        }
      } else if (key == KEY_CONFIRM && nav.screen == UI_DETAIL &&
                 nav.item == 0 && s.offer)
        confirm(&s);
      if (previous.screen != nav.screen || previous.item != nav.item) {
        ui_confirmation_show(&confirmation, NULL, demo_ms());
        drawn = 0;
      }
    }
    ui_door_update(&doors, &current, nav.screen, s.maintenance || s.writing);
    if (logged_alert.open != doors.open || logged_alert.known != doors.known ||
        logged_alert.visible != doors.visible) {
      ESP_LOGI("ui_alert", "open=%02x known=%02x visible=%u suppressed=%u",
               doors.open, doors.known, doors.visible,
               s.maintenance || s.writing);
      logged_alert = doors;
    }
    ui_confirmation_release(&confirmation, !s.pressed[1], demo_ms());
    if (!drawn || demo_ms() - drawn >= (s.writing ? 200 : 33)) {
      drawn = demo_ms();
      char body[256];
      ui_reading_t r = reading(&s);
      detail(&s, r, body, sizeof(body));
      const char *hint = (nav.screen == UI_ENGINE || nav.screen == UI_SHIFT)
                             ? "K1 next  /  Gateway data"
                         : nav.screen == UI_GATEWAY ? "K1 next  /  hold K1 home"
                         : nav.screen == UI_SERVICE ? "K1 next  /  K2 open"
                         : nav.screen == UI_MENU
                             ? "K1 next  K2 open  Hold K1 back"
                             : "K2 action  /  hold K1 back";
      if (nav.screen == UI_DETAIL && nav.item == 0)
        hint = s.writing ? "Hold K1: cancel"
               : s.offer ? "Hold K2 install  Hold K1 back"
                         : "K2 check  /  hold K1 back";
      ui_view_state_t v = {.nav = nav,
                           .reading = r,
                           .doors = doors,
                           .detail = body,
                           .footer = hint,
                           .password = s.ap_password,
                           .writing = s.writing,
                           .offer = s.offer,
                           .progress = s.progress};
      ui_view_render(&v);
      ui_offer_t shown = offer(&s);
      ui_confirmation_show(
          &confirmation,
          nav.screen == UI_DETAIL && nav.item == 0 && s.offer ? &shown : NULL,
          demo_ms());
    }
    int64_t handler_start = esp_timer_get_time();
    lv_timer_handler();
    uint32_t handler_us = esp_timer_get_time() - handler_start;
    handler_total += handler_us;
    if (handler_us > handler_max)
      handler_max = handler_us;
    loops++;
    demo_edit(status, NULL);
    uint64_t now = esp_timer_get_time();
    if (now - profile_window >= 5000000) {
      ESP_LOGI("ui_perf",
               "page=%u item=%u loops=%lu period_avg_us=%llu "
               "handler_avg_us=%llu handler_max_us=%lu "
               "flush_wait_total_us=%llu flushes=%lu heap_min=%lu",
               nav.screen, nav.item, (unsigned long)loops,
               (unsigned long long)((now - profile_window) / loops),
               (unsigned long long)(handler_total / loops),
               (unsigned long)handler_max, (unsigned long long)wait_total,
               (unsigned long)flush_count,
               (unsigned long)esp_get_minimum_free_heap_size());
      ESP_LOGI("ui_data",
               "secure=%u seq=%lu rpm=%lu src=%u age=%llu valid=%03x "
               "speed_dkph=%u gear=%02x water=%d oil=%d open=%02x",
               s.link_secure, (unsigned long)s.ble_sequence,
               (unsigned long)s.ble_rpm, s.telemetry_source,
               (unsigned long long)(s.telemetry_received &&
                                            now / 1000 >= s.telemetry_received
                                        ? now / 1000 - s.telemetry_received
                                        : 0),
               s.telemetry_valid, s.speed_dkph, s.gear, s.coolant_c, s.oil_c,
               s.closure_open);
      profile_window = esp_timer_get_time();
      handler_total = 0;
      handler_max = 0;
      loops = 0;
      wait_total = 0;
      flush_count = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void wheel_ui_start(void) {
  configASSERT(xTaskCreatePinnedToCore(run, "wheel_ui", 6144, NULL, 5, NULL,
                                       1) == pdPASS);
}
