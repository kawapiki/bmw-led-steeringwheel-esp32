#include "display_port.h"
#include "demo.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_timer.h"
#include "wheel_board.h"
static esp_lcd_panel_handle_t panel;
static lv_display_t *display;
static volatile uint32_t completions, elapsed;
static volatile int64_t started;
static bool done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e,
                 void *u) {
  (void)io;
  (void)e;
  (void)u;
  elapsed = (uint32_t)(esp_timer_get_time() - started);
  completions++;
  lv_display_flush_ready(display);
  return false;
}
static void metrics(demo_state_t *s, void *a) {
  (void)a;
  s->flushes = completions;
  s->flush_us = elapsed;
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
  size_t count = (a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
  lv_draw_sw_rgb565_swap(p, count);
  started = esp_timer_get_time();
  ESP_ERROR_CHECK(
      esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, p));
  demo_edit(metrics, NULL);
}
static uint32_t tick(void) { return (uint32_t)demo_ms(); }
lv_display_t *display_port_start(void) {
  gpio_config_t out = {.pin_bit_mask = 1ULL << WHEEL_LCD_BACKLIGHT,
                       .mode = GPIO_MODE_OUTPUT};
  ESP_ERROR_CHECK(gpio_config(&out));
  gpio_set_level(WHEEL_LCD_BACKLIGHT, 0);
  spi_bus_config_t bus = {.mosi_io_num = WHEEL_LCD_MOSI,
                          .miso_io_num = -1,
                          .sclk_io_num = WHEEL_LCD_SCLK,
                          .quadwp_io_num = -1,
                          .quadhd_io_num = -1,
                          .max_transfer_sz = 320 * 24 * 2};
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_io_spi_config_t cfg = {.dc_gpio_num = WHEEL_LCD_DC,
                                       .cs_gpio_num = WHEEL_LCD_CS,
                                       .pclk_hz = 40000000,
                                       .lcd_cmd_bits = 8,
                                       .lcd_param_bits = 8,
                                       .spi_mode = 0,
                                       .trans_queue_depth = 2,
                                       .on_color_trans_done = done};
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &cfg, &io));
  esp_lcd_panel_dev_config_t dev = {.reset_gpio_num = WHEEL_LCD_RESET,
                                    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                    .bits_per_pixel = 16};
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &dev, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, WHEEL_LCD_SWAP_XY));
  ESP_ERROR_CHECK(
      esp_lcd_panel_mirror(panel, WHEEL_LCD_MIRROR_X, WHEEL_LCD_MIRROR_Y));
  /* Candidate landscape gap: physical orientation must be checked before
   * release. */
  ESP_ERROR_CHECK(
      esp_lcd_panel_set_gap(panel, WHEEL_LCD_GAP_X, WHEEL_LCD_GAP_Y));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
  lv_init();
  lv_tick_set_cb(tick);
  display = lv_display_create(320, 172);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  void *b1 =
      heap_caps_malloc(320 * 24 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void *b2 =
      heap_caps_malloc(320 * 24 * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  configASSERT(b1 && b2);
  lv_display_set_buffers(display, b1, b2, 320 * 24 * 2,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  gpio_set_level(WHEEL_LCD_BACKLIGHT, 1);
  return display;
}
