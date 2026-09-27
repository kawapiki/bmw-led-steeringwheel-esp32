#include "wheel_io.h"
#include "demo.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "wheel_board.h"
#include "wheel_core.h"
static led_strip_handle_t strips[2];
static rmt_channel_handle_t motor_channel;
static rmt_encoder_handle_t motor_encoder;
static const rmt_symbol_word_t motor_pulse = {
    .level0 = 1, .duration0 = 20000, .level1 = 0, .duration1 = 1};
static uint32_t counts[2], drops;
static bool stable[2];
static uint64_t starts[5];
static void publish(demo_state_t *s, void *a) {
  (void)a;
  s->input_heartbeat = demo_ms();
  s->input_drops = drops;
  for (int i = 0; i < 2; i++) {
    s->pressed[i] = stable[i];
    s->buttons[i] = counts[i];
  }
}
static void sendkey(demo_key_t k) {
  if (xQueueSend(demo_keys, &k, 0) != pdTRUE)
    drops++;
}
static void clear_test(demo_state_t *s, void *a);
static void input(void *a) {
  (void)a;
  int pins[2] = {WHEEL_BUTTON_K1, WHEEL_BUTTON_K2};
  bool raw[2] = {0}, held[2] = {0};
  uint64_t changed[2] = {0}, down[2] = {0};
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    uint64_t now = demo_ms();
    demo_state_t s;
    demo_get(&s);
    if (s.haptic_test) {
      demo_edit(clear_test, NULL);
      if (!s.writing && !s.maintenance && now - starts[4] >= 100 &&
          now - starts[0] >= 1000) {
        for (int j = 0; j < 4; j++)
          starts[j] = starts[j + 1];
        starts[4] = now;
        rmt_transmit_config_t t = {.loop_count = 0, .flags.eot_level = 0};
        rmt_transmit(motor_channel, motor_encoder, &motor_pulse,
                     sizeof(motor_pulse), &t);
      }
    }
    for (int i = 0; i < 2; i++) {
      bool v = !gpio_get_level(pins[i]);
      if (v != raw[i]) {
        raw[i] = v;
        changed[i] = now;
      }
      if (v != stable[i] && now - changed[i] >= 15) {
        stable[i] = v;
        if (v) {
          counts[i]++;
          down[i] = now;
          held[i] = false;
          if (!s.writing && !s.maintenance && now - starts[4] >= 100 &&
              now - starts[0] >= 1000) {
            for (int j = 0; j < 4; j++)
              starts[j] = starts[j + 1];
            starts[4] = now;
            rmt_transmit_config_t t = {.loop_count = 0, .flags.eot_level = 0};
            rmt_transmit(motor_channel, motor_encoder, &motor_pulse,
                         sizeof(motor_pulse), &t);
          }
        } else if (!held[i])
          sendkey(i ? KEY_SELECT : KEY_NEXT);
      }
      if (stable[i] && !held[i] && now - down[i] >= 700) {
        held[i] = true;
        sendkey(i ? KEY_CONFIRM : KEY_BACK);
      }
    }
    demo_edit(publish, NULL);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(5));
  }
}
static void quiet(demo_state_t *s, void *a) { s->io_quiet = *(bool *)a; }
static void clear_test(demo_state_t *s, void *a) {
  (void)a;
  s->haptic_test = false;
}
static void effects(void *a) {
  (void)a;
  bool shift = false;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    demo_state_t s;
    demo_get(&s);
    if (s.writing && s.io_quiet) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (s.rpm >= 6500)
      shift = true;
    else if (s.rpm < 6350)
      shift = false;
    uint32_t mask = rpm_mask(s.rpm);
    bool lit = !shift || ((demo_ms() / 125) % 2);
    for (int chain = 0; chain < 2; chain++) {
      for (int p = 0; p < 23; p++) {
        bool on = !s.writing && !s.maintenance && lit && (mask & (1u << p));
        uint8_t r = p < 15 ? 0 : 25, g = p < 20 ? 25 : 0;
        if (s.led_mode) {
          on = !s.writing && !s.maintenance &&
               (s.led_mode == 1   ? chain == 0
                : s.led_mode == 2 ? chain == 1
                : s.led_mode == 3 ? p == (demo_ms() / 150) % 23
                                  : true);
          r = s.led_mode == 4 ? 25 : r;
          g = s.led_mode == 4 ? 0 : g;
        }
        led_strip_set_pixel(strips[chain], p, on ? r : 0, on ? g : 0, 0);
      }
      led_strip_set_pixel(strips[chain], 23, 0, s.pressed[chain] ? 25 : 3,
                          s.pressed[chain] ? 25 : 3);
      led_strip_refresh(strips[chain]);
    }
    demo_edit(quiet, &s.writing);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}
void wheel_io_start(void) {
  gpio_config_t out = {.pin_bit_mask = 1ULL << WHEEL_MOTOR,
                       .mode = GPIO_MODE_OUTPUT};
  ESP_ERROR_CHECK(gpio_config(&out));
  gpio_set_level(WHEEL_MOTOR, 0);
  gpio_config_t in = {.pin_bit_mask =
                          (1ULL << WHEEL_BUTTON_K1) | (1ULL << WHEEL_BUTTON_K2),
                      .mode = GPIO_MODE_INPUT,
                      .pull_up_en = GPIO_PULLUP_ENABLE};
  ESP_ERROR_CHECK(gpio_config(&in));
  rmt_tx_channel_config_t mc = {.gpio_num = WHEEL_MOTOR,
                                .clk_src = RMT_CLK_SRC_DEFAULT,
                                .resolution_hz = 1000000,
                                .mem_block_symbols = 48,
                                .trans_queue_depth = 1};
  ESP_ERROR_CHECK(rmt_new_tx_channel(&mc, &motor_channel));
  rmt_copy_encoder_config_t ec = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&ec, &motor_encoder));
  ESP_ERROR_CHECK(rmt_enable(motor_channel));
  int pins[2] = {WHEEL_LED_CHAIN_0, WHEEL_LED_CHAIN_1};
  for (int i = 0; i < 2; i++) {
    led_strip_config_t cfg = {.strip_gpio_num = pins[i],
                              .max_leds = 24,
                              .led_model = LED_MODEL_WS2812,
                              .color_component_format =
                                  LED_STRIP_COLOR_COMPONENT_FMT_GRB};
    led_strip_rmt_config_t rmt = {.resolution_hz = 10000000,
                                  .mem_block_symbols = 96,
                                  .flags.with_dma = (i == 0)};
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &strips[i]));
    ESP_ERROR_CHECK(led_strip_clear(strips[i]));
  }
  configASSERT(xTaskCreatePinnedToCore(input, "wheel_input", 3072, NULL, 6,
                                       NULL, 1) == pdPASS);
  configASSERT(xTaskCreatePinnedToCore(effects, "wheel_effects", 3072, NULL, 4,
                                       NULL, 1) == pdPASS);
}
