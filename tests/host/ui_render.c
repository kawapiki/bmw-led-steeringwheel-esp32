/* Host-only LVGL framebuffer capture. Production ui_view.c is compiled
 * unchanged. */
#include "ui_view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[320 * 172];
static uint8_t drawbuf[15360];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
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
                       .reading = {true, 0, UI_DATA_DEMO},
                       .footer = "K1 next  /  Gateway data"};
  capture(argv[1], "engine-zero", &s);
  s.reading.rpm = 7000;
  capture(argv[1], "engine-7000", &s);
  s.nav.screen = UI_SHIFT;
  capture(argv[1], "shift-7000", &s);
  s.reading.rpm = 4000;
  capture(argv[1], "shift-4000", &s);
  s.reading = (ui_reading_t){false, 0, UI_DATA_STALE};
  capture(argv[1], "shift-stale", &s);
  s.nav.screen = UI_ENGINE;
  capture(argv[1], "engine-stale", &s);
  s.reading.status = UI_DATA_OFFLINE;
  capture(argv[1], "engine-disconnected", &s);
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
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  printf("Host LVGL used=%zu peak=%zu largest_free=%zu (64-bit host, not ESP32 "
         "measurement)\n",
         m.total_size - m.free_size, m.max_used, m.free_biggest_size);
  return 0;
}
