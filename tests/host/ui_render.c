/* Host-only LVGL framebuffer capture. Production ui_view.c is compiled
 * unchanged. */
#include "ui_view.h"
#include "vehicle_3d.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t test_clock;
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
  if ((s->doors.visible || s->nav.screen == UI_VEHICLE) && !s->boot) {
    for (unsigned i = 0; i < 40; ++i) {
      s->now_ms = (test_clock += 50);
      ui_view_render(s);
      lv_refr_now(NULL);
    }
  }
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

static void verify_source_equivalence(void) {
  ui_view_state_t s = {.nav = {UI_ENGINE, 0},
                       .reading = {.available = true,
                                   .lights_valid = 63,
                                   .lights_on = 21,
                                   .rpm = 3500,
                                   .speed_dkph = 1080,
                                   .gear = 0x45,
                                   .valid_fields = 31,
                                   .coolant_c = 90,
                                   .oil_c = 103}};
  for (unsigned page = UI_ENGINE; page <= UI_SHIFT; ++page) {
    s.nav.screen = page;
    for (unsigned mask = 0; mask < 64; ++mask) {
      s.doors = (ui_door_state_t){
          .known = 63, .open = mask, .active = mask != 0, .visible = mask != 0};
      s.now_ms = (test_clock += 50);
      s.reading.status = UI_DATA_DEMO;
      ui_view_render(&s);
      lv_refr_now(NULL);
      memcpy(incremental_pixels, pixels, sizeof(pixels));
      s.reading.status = UI_DATA_LIVE;
      ui_view_render(&s);
      lv_refr_now(NULL);
      assert(memcmp(incremental_pixels, pixels, sizeof(pixels)) == 0);
    }
  }
  s.doors.visible = false;
  s.doors.active = false;
  s.reading.lights_valid = 63;
  s.reading.lights_on = 0;
  ui_view_render(&s);
  lv_refr_now(NULL);
  memcpy(incremental_pixels, pixels, sizeof(pixels));
  s.reading.lights_on = 63;
  ui_view_render(&s);
  lv_refr_now(NULL);
  assert(!memcmp(incremental_pixels, pixels,
                 sizeof(pixels))); /* no dashboard lamp icons */
  for (unsigned mask = 0; mask < 64; ++mask) {
    s.reading.lights_valid = 63;
    s.reading.lights_on = mask;
    equivalent(&s, false);
    s.reading.lights_valid = mask;
    s.reading.lights_on &= mask;
    equivalent(&s, false);
  }
  puts("Simulated/live operational pixels identical on both pages and all 64 "
       "closure masks");
}
static void verify_engine_pixels(void) {
  uint16_t before[VEHICLE_3D_WIDTH * VEHICLE_3D_HEIGHT];
  for (unsigned bit = 0; bit < 6; ++bit) {
    uint8_t mask = bit == 5 ? 16 : bit == 3 ? 1 : bit == 4 ? 2 : 0;
    const lv_image_dsc_t *frame = NULL;
    vehicle_3d_pause();
    for (unsigned i = 0; i < 160; ++i)
      frame = vehicle_3d_frame(test_clock += 50, false, 0, mask, 63,
                               (i & 1) ? 63 : 0, 0);
    assert(frame && frame->data_size == sizeof(before));
    memcpy(before, frame->data, sizeof(before));
    frame = vehicle_3d_frame(test_clock += 50, false, 0, mask, 63, 63, 0);
    assert(
        !memcmp(before, frame->data, sizeof(before))); /* stationary baseline */
    frame = vehicle_3d_frame(test_clock += 50, false, 0, mask, 63, 0, 0);
    uint32_t generation = vehicle_3d_generation();
    frame = vehicle_3d_frame(test_clock + 1, false, 0, mask, 63, 63, 1u << bit);
    assert(vehicle_3d_generation() ==
           generation); /* throttled change retained by caller */
    frame =
        vehicle_3d_frame(test_clock += 50, false, 0, mask, 63, 63, 1u << bit);
    unsigned changed = 0;
    for (unsigned i = 0; i < VEHICLE_3D_WIDTH * VEHICLE_3D_HEIGHT; ++i)
      changed += before[i] != ((const uint16_t *)frame->data)[i];
    printf("Runtime model lamp bit%u changed %u pixels\n", bit, changed);
    assert(changed > 0);
    frame = vehicle_3d_frame(test_clock += 50, false, 0, mask, 63, 0, 0);
    memcpy(before, frame->data, sizeof(before));
    frame = vehicle_3d_frame(test_clock += 50, false, 0, mask, 63, 0, 63);
    assert(!memcmp(before, frame->data,
                   sizeof(before))); /* invalid on bits cannot illuminate;
                                        unknown may be muted */
  }
}
static void verify_fullscreen(const ui_view_state_t *s) {
  const lv_image_dsc_t *frame =
      vehicle_3d_frame(s->now_ms, s->boot, s->boot_progress,
                       s->boot ? s->reading.closure_open : s->doors.open,
                       s->boot ? s->reading.closure_known : s->doors.known,
                       s->reading.lights_valid, s->reading.lights_on);
  assert(frame && frame->header.w == 320 && frame->header.h == 172);
  assert(memcmp(pixels, frame->data, sizeof(pixels)) ==
         0); /* no labels/icons/gauges */
}
static void verify_vehicle(const char *dir) {
  ui_view_state_t s = {.nav = {UI_ENGINE, 0},
                       .reading = {.available = true,
                                   .rpm = 800,
                                   .status = UI_DATA_DEMO,
                                   .valid_fields = 31,
                                   .gear = 0x10,
                                   .closure_known = 63}};
  assert(ui_view_vehicle_ready());
  s.boot = true;
  uint32_t phase_hashes[7] = {0};
  const float phases[] = {0.0f, 0.18f, 0.4f, 0.56f, 0.72f, 0.85f, 0.98f};
  for (unsigned i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
    s.now_ms = (test_clock += 50);
    s.boot_progress = phases[i];
    equivalent(&s, false);
    char name[40];
    snprintf(name, sizeof(name), "runtime-boot-%02u", i);
    capture(dir, name, &s);
    verify_fullscreen(&s);
    uint32_t hash = 2166136261u;
    for (unsigned px = 0; px < 320 * 172; ++px)
      hash = (hash ^ pixels[px]) * 16777619u;
    phase_hashes[i] = hash;
  }
  assert(phase_hashes[0] != phase_hashes[2] &&
         phase_hashes[2] != phase_hashes[3] &&
         phase_hashes[3] !=
             phase_hashes[4]); /* actual continuous camera viewpoints */
  s.boot = false;
  s.doors = (ui_door_state_t){.known = 63, .active = true, .visible = true};
  for (unsigned mask = 0; mask < 64; ++mask) {
    s.doors.open = mask;
    s.now_ms = (test_clock += 50);
    equivalent(&s, false);
    verify_fullscreen(&s);
  }
  s.doors.open = 1;
  s.doors.known = 0;
  capture(dir, "runtime-unknown", &s);
  s.nav.screen = UI_DETAIL;
  s.nav.item = 0;
  s.doors.visible = false;
  s.password = "abcdefghjkmnpqrs";
  uint32_t generation = vehicle_3d_generation();
  s.now_ms = (test_clock += 100);
  capture(dir, "runtime-to-qr", &s);
  assert(vehicle_3d_generation() == generation);
  s.boot = true;
  s.boot_progress = 0.5f;
  equivalent(&s, false);
  assert(vehicle_3d_generation() == generation); /* service suppresses engine */
  s.nav.screen = UI_ENGINE;
  s.password = "";
  s.maintenance = true;
  equivalent(&s, false);
  assert(vehicle_3d_generation() == generation);
  s.boot = false;
  s.maintenance = false;
  s.password = "";
  s.nav.screen = UI_VEHICLE;
  s.doors = (ui_door_state_t){.known = 63};
  capture(dir, "runtime-vehicle-ready", &s);
  verify_fullscreen(&s);
  s.maintenance = true;
  capture(dir, "runtime-vehicle-paused", &s);
  s.maintenance = false;
  s.nav.screen = UI_ENGINE;
  s.doors = (ui_door_state_t){
      .known = 63, .open = 1, .visible = true, .active = true};
  capture(dir, "runtime-before-close", &s);
  s.doors = (ui_door_state_t){.known = 63};
  s.now_ms = (test_clock += 50);
  capture(dir, "runtime-closing", &s);
  s.now_ms = (test_clock += 1050);
  capture(dir, "runtime-closed-return", &s);
  puts("Runtime3D boot/64 closure masks/unknown/maintenance-service rendering "
       "checked");
}
int main(int argc, char **argv) {
  assert(argc == 2);
  setvbuf(stdout, NULL, _IONBF, 0);
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
  s.reading =
      (ui_reading_t){.available = true, .rpm = 4321, .status = UI_DATA_DEMO};
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
  s = (ui_view_state_t){.nav = {UI_ENGINE, 0},
                        .reading = {.status = UI_DATA_LIVE,
                                    .valid_fields = 31,
                                    .rpm = 3500,
                                    .speed_dkph = 1080,
                                    .gear = 0x45,
                                    .lights_valid = 63,
                                    .lights_on = 21}};
  capture(argv[1], "lighting-mixed", &s);
  s.reading.lights_valid = 9;
  s.reading.lights_on = 8;
  capture(argv[1], "lighting-partial", &s);
  s.reading.lights_valid = 0;
  s.reading.lights_on = 0;
  capture(argv[1], "lighting-unknown", &s);
  s.doors = (ui_door_state_t){
      .active = true, .acknowledged = true, .known = 63, .open = 1};
  s.reading.status = UI_DATA_DEMO;
  capture(argv[1], "closure-acknowledged", &s);
  memcpy(incremental_pixels, pixels, sizeof(pixels));
  s.reading.status = UI_DATA_LIVE;
  ui_view_render(&s);
  lv_refr_now(NULL);
  assert(memcmp(incremental_pixels, pixels, sizeof(pixels)) == 0);
  s.doors.active = false;
  ui_view_render(&s);
  lv_refr_now(NULL);
  bool marker_changed = false;
  for (unsigned y = 8; y < 24; ++y)
    for (unsigned x = 240; x < 308; ++x)
      marker_changed |= incremental_pixels[y * 320 + x] != pixels[y * 320 + x];
  assert(marker_changed); /* acknowledged opening leaves a visible marker */
  s.doors.active = true;
  s.doors.known = 0;
  capture(argv[1], "closure-acknowledged-unknown", &s);
  verify_source_equivalence();
  s = (ui_view_state_t){
      .nav = {UI_VEHICLE, 0},
      .reading = {.status = UI_DATA_LIVE, .lights_valid = 63, .lights_on = 7},
      .doors = {.known = 63}};
  capture(argv[1], "runtime-vehicle-closed-lights", &s);
  memcpy(incremental_pixels, pixels, sizeof(pixels));
  s.reading.status = UI_DATA_DEMO;
  ui_view_render(&s);
  lv_refr_now(NULL);
  assert(!memcmp(incremental_pixels, pixels, sizeof(pixels)));
  s.reading.lights_on = 0;
  capture(argv[1], "runtime-vehicle-closed-off", &s);
  s.reading.lights_valid = 0;
  capture(argv[1], "runtime-vehicle-lights-unknown", &s);
  s.reading.lights_valid = 63;
  for (unsigned bit = 0; bit < 6; ++bit) {
    s = (ui_view_state_t){.nav = {UI_VEHICLE, 0},
                          .reading = {.status = UI_DATA_LIVE,
                                      .lights_valid = 63,
                                      .lights_on = 1u << bit},
                          .doors = {.known = 63}};
    char name[48];
    snprintf(name, sizeof(name), "runtime-actual-lamp-%u", bit);
    capture(argv[1], name, &s);
  }
  verify_engine_pixels();
  verify_vehicle(argv[1]);
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
