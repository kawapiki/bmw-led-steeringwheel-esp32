/* Host-only LVGL framebuffer capture. Production ui_view.c is compiled
 * unchanged. */
#include "ui_view.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[320 * 172];
static uint8_t drawbuf[15360];
static uint32_t flushed_pixels;
static unsigned equivalent_cases;
static uint16_t incremental_pixels[320 * 172];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
  flushed_pixels += (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
  uint16_t *src = (uint16_t *)p;
  for (int y = a->y1; y <= a->y2; y++)
    for (int x = a->x1; x <= a->x2; x++)
      pixels[y * 320 + x] = *src++;
  lv_display_flush_ready(d);
}
static void capture(const char *dir, const char *name, ui_view_state_t *s) {
  ui_view_render(s);
  lv_tick_inc(200);
  lv_timer_handler();
  lv_refr_now(NULL);
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  assert(f);
  fprintf(f, "P6\n320 172\n255\n");
  for (unsigned i = 0; i < 320 * 172; i++) {
    unsigned p = pixels[i];
    unsigned char rgb[3] = {(p >> 11) * 255 / 31, ((p >> 5) & 63) * 255 / 63,
                            (p & 31) * 255 / 31};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void equivalent(ui_view_state_t *s, bool small_step) {
  equivalent_cases++;
  flushed_pixels = 0;
  ui_view_render(s);
  lv_tick_inc(200);
  lv_timer_handler();
  lv_refr_now(NULL);
  uint32_t dirty_pixels = flushed_pixels;
  memcpy(incremental_pixels, pixels, sizeof(pixels));
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(NULL);
  if (memcmp(incremental_pixels, pixels, sizeof(pixels))) {
    for (unsigned i = 0; i < 320 * 172; i++)
      if (incremental_pixels[i] != pixels[i]) {
        fprintf(stderr,
                "Incremental mismatch x=%u y=%u rpm=%lu speed=%u page=%u\n",
                i % 320, i / 320, (unsigned long)s->reading.rpm,
                s->reading.speed_dkph, s->nav.screen);
        fprintf(stderr, "pixel incremental=%04x full=%04x dirty=%lu\n",
                incremental_pixels[i], pixels[i], (unsigned long)dirty_pixels);
        break;
      }
    assert(!"Incremental framebuffer differs from full redraw");
  }
  if (small_step) {
    printf("Small moving update transferred %lu pixels\n",
           (unsigned long)dirty_pixels);
    assert(dirty_pixels < 20000);
  }
}
static void verify_incremental(void) {
  ui_view_state_t s = {.nav = {UI_ENGINE, 0},
                       .reading = {.available = true,
                                   .rpm = 3420,
                                   .status = UI_DATA_DEMO,
                                   .valid_fields = 31,
                                   .speed_dkph = 1080,
                                   .gear = 0x45,
                                   .coolant_c = 94,
                                   .oil_c = 102,
                                   .closure_known = 63}};
  equivalent(&s, false);
  s.reading.rpm = 3430;
  s.reading.speed_dkph = 1081;
  equivalent(&s, true);
  const uint32_t rpms[] = {3500, 5000,       7100, 7131, 7190, 8000,
                           9000, UINT32_MAX, 8000, 7140, 7100, 3000,
                           35,   1,          0,    8000, 0};
  const uint16_t speeds[] = {1100, 1300,  2130, 2140, 2170, 2400,
                             3000, 65535, 2400, 2150, 2130, 900,
                             10,   1,     0,    2400, 0};
  for (unsigned i = 0; i < sizeof(rpms) / sizeof(rpms[0]); i++) {
    s.reading.rpm = rpms[i];
    s.reading.speed_dkph = speeds[i];
    equivalent(&s, i == 0);
  }
  /* Exercise endpoints on both sides of cardinal/degree rounding boundaries. */
  for (unsigned i = 0; i < 48; i++) {
    s.reading.rpm = (i & 1) ? (8000 - i * 137) : (i * 137);
    s.reading.speed_dkph = (i & 1) ? (2400 - i * 41) : (i * 41);
    equivalent(&s, false);
  }
  for (unsigned i = 0; i <= 80; i++) {
    s.reading.rpm = i * 100;
    s.reading.speed_dkph = i * 30;
    equivalent(&s, false);
  }
  for (unsigned i = 80; i > 0; i--) {
    s.reading.rpm = (i - 1) * 100;
    s.reading.speed_dkph = (i - 1) * 30;
    equivalent(&s, false);
  }
  uint32_t random = 42;
  for (unsigned i = 0; i < 1024; i++) {
    random = random * 1664525u + 1013904223u;
    int rpm = (int)s.reading.rpm + (int)(random % 1401) - 700;
    int speed = (int)s.reading.speed_dkph + (int)((random >> 16) % 421) - 210;
    s.reading.rpm = rpm < 0 ? 0 : rpm > 8000 ? 8000 : rpm;
    s.reading.speed_dkph = speed < 0 ? 0 : speed > 2400 ? 2400 : speed;
    equivalent(&s, false);
  }
  s.reading.valid_fields = 0;
  s.reading.available = false;
  equivalent(&s, false);
  s.reading.valid_fields = 31;
  s.reading.available = true;
  s.reading.rpm = 4500;
  s.reading.speed_dkph = 1210;
  equivalent(&s, false);
  s.nav.screen = UI_SHIFT;
  equivalent(&s, false);
  s.nav.screen = UI_ENGINE;
  equivalent(&s, false);
  s.doors = (ui_door_state_t){
      .open = 1, .known = 63, .active = true, .visible = true};
  equivalent(&s, false);
  s.doors = (ui_door_state_t){0};
  equivalent(&s, false);
  printf("Incremental/full LVGL pixel equivalence passed: %u cases\n",
         equivalent_cases);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  lv_init();
  lv_display_t *d = lv_display_create(320, 172);
  lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(d, drawbuf, NULL, sizeof(drawbuf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(d, flush);
  ui_view_create(lv_screen_active());

  ui_view_state_t s = {.nav = {UI_ENGINE, 0},
                       .reading = {.available = true,
                                   .rpm = 3420,
                                   .status = UI_DATA_DEMO,
                                   .valid_fields = 31,
                                   .speed_dkph = 1080,
                                   .gear = 0x45,
                                   .coolant_c = 94,
                                   .oil_c = 102,
                                   .closure_known = 63}};
  capture(argv[1], "drive-108-D5", &s);
  s.nav.screen = UI_SHIFT;
  capture(argv[1], "sport-108-D5", &s);
  s.nav.screen = UI_ENGINE;
  s.reading.speed_dkph = 0;
  s.reading.rpm = 0;
  s.reading.gear = 0x10;
  s.reading.coolant_c = 0;
  s.reading.oil_c = 0;
  capture(argv[1], "drive-valid-zero", &s);
  const char *closure_names[] = {"closure-FL", "closure-FR",    "closure-RL",
                                 "closure-RR", "closure-trunk", "closure-hood"};
  for (unsigned i = 0; i < 6; i++) {
    s.doors = (ui_door_state_t){
        .open = 1u << i, .known = 63, .active = true, .visible = true};
    capture(argv[1], closure_names[i], &s);
  }
  s.doors.open = 15;
  capture(argv[1], "closure-all-doors", &s);
  s.doors.open = 9;
  capture(argv[1], "closure-multiple", &s);
  s.doors.known = 0;
  s.reading = (ui_reading_t){.status = UI_DATA_STALE};
  capture(argv[1], "closure-unknown", &s);
  s.doors = (ui_door_state_t){0};
  capture(argv[1], "drive-stale", &s);
  s.nav.screen = UI_SHIFT;
  capture(argv[1], "sport-stale", &s);
  s.nav.screen = UI_ENGINE;
  s.reading = (ui_reading_t){.available = true,
                             .rpm = 3420,
                             .status = UI_DATA_DEMO,
                             .valid_fields = UI_VALID_RPM};
  capture(argv[1], "drive-v1-rpm-only", &s);
  s.nav.screen = UI_GATEWAY;
  s.detail = "Authenticated / current\nSource: gateway demo\nRPM 4321 / age 74 "
             "ms\nCAN control disabled";
  s.reading = (ui_reading_t){true, 4321, UI_DATA_DEMO};
  s.footer = "K1 next  /  hold K1 home";
  capture(argv[1], "gateway", &s);
  s.nav.screen = UI_SERVICE;
  s.detail = "Maintenance and diagnostics\nWi-Fi updates / component "
             "tests\n\nK2 opens the service menu";
  s.footer = "K1 next  /  K2 open";
  capture(argv[1], "service", &s);
  s.nav.screen = UI_MENU;
  s.footer = "K1 next  K2 open  Hold K1 back";
  capture(argv[1], "menu", &s);
  s.nav.screen = UI_DETAIL;
  s.password = "abcdefghjkmnpqrs";
  capture(argv[1], "qr", &s);
  s.password = "abcdefghjkmnpqr!";
  capture(argv[1], "qr-fallback", &s);
  s.password = "";
  s.writing = true;
  s.progress = 32;
  s.detail = "Wi-Fi connected\nDownloading wheel firmware\n32% / installed 4";
  s.footer = "Hold K1: cancel";
  capture(argv[1], "update", &s);
  s.writing = false;
  s.offer = true;
  s.detail = "Wi-Fi connected\nRelease 5 ready\n0% / installed 4";
  s.footer = "Hold K2 install  Hold K1 back";
  capture(argv[1], "update-offer", &s);
  s.offer = false;
  s.detail = "Wi-Fi connected\nRelease response no memory\n0% / installed 4";
  s.footer = "K2 retry  /  hold K1 back";
  capture(argv[1], "update-error", &s);
  verify_incremental();
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  printf("Root objects=%u (plus QR child)\n",
         (unsigned)lv_obj_get_child_count(lv_screen_active()));
  printf("Host LVGL used=%zu peak=%zu largest_free=%zu (64-bit host, not ESP32 "
         "measurement)\n",
         m.total_size - m.free_size, m.max_used, m.free_biggest_size);
  return 0;
}
