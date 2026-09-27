#include "wheel_led.h"
#include <stdbool.h>
#include <stddef.h>
#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "wheel_board.h"

/* S3 has one DMA-capable RMT TX channel. Share it between the two
 * sequential strips. The full frame is copied before hardware starts:
 * 576 data symbols + reset + driver EOF, with no mid-frame refill. */
enum { LED_PIXELS = 24, LED_BYTES = LED_PIXELS * 3,
       LED_FRAME_SYMBOLS = LED_BYTES * 8 + 1, LED_DMA_SYMBOLS = 640 };
_Static_assert(LED_DMA_SYMBOLS > LED_FRAME_SYMBOLS,
               "DMA buffer must include the complete frame and EOF");
_Static_assert(LED_DMA_SYMBOLS % 16 == 0, "DMA half buffers must be aligned");
static const int pins[2] = {WHEEL_LED_CHAIN_0, WHEEL_LED_CHAIN_1};
static uint8_t pixels[2][LED_BYTES];
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
static unsigned active;
static bool faulted;

/* No per-frame allocation: emit directly into the driver DMA buffer.
 * A full frame fits the initial encoding pass before the transmitter starts.
 * Small-chunk support also keeps the simple encoder's overflow buffer bounded. */
static size_t IRAM_ATTR encode_frame(const void *data, size_t data_size,
                                    size_t written, size_t available,
                                    rmt_symbol_word_t *symbols, bool *done,
                                    void *arg) {
  (void)arg;
  if (data_size != LED_BYTES || written >= LED_FRAME_SYMBOLS) {
    *done = true;
    return 0;
  }
  const uint8_t *bytes = data;
  size_t count = LED_FRAME_SYMBOLS - written;
  if (count > available)
    count = available;
  for (size_t i = 0; i < count; ++i) {
    size_t bit = written + i;
    if (bit == LED_FRAME_SYMBOLS - 1) {
      symbols[i] = (rmt_symbol_word_t){
          .level0 = 0, .duration0 = 1400, .level1 = 0, .duration1 = 1400};
    } else {
      bool one = bytes[bit / 8] & (0x80u >> (bit % 8));
      /* Preserve the pinned led_strip WS2812 timing at 10 MHz. */
      symbols[i] = (rmt_symbol_word_t){
          .level0 = 1, .duration0 = one ? 9 : 3,
          .level1 = 0, .duration1 = one ? 3 : 9};
    }
  }
  *done = written + count == LED_FRAME_SYMBOLS;
  return count;
}

void wheel_led_set(unsigned chain, unsigned pixel, uint8_t r, uint8_t g,
                   uint8_t b) {
  if (chain >= 2 || pixel >= LED_PIXELS || faulted)
    return;
  pixels[chain][pixel * 3] = g;
  pixels[chain][pixel * 3 + 1] = r;
  pixels[chain][pixel * 3 + 2] = b;
}

esp_err_t wheel_led_refresh(unsigned chain) {
  if (chain >= 2)
    return ESP_ERR_INVALID_ARG;
  if (!channel || faulted)
    return ESP_ERR_INVALID_STATE;
  if (chain != active) {
    unsigned old = active;
    esp_err_t err = rmt_tx_switch_gpio(channel, pins[chain], false);
    if (err != ESP_OK)
      return err;
    active = chain;
    /* switch_gpio disables the previous output. Park it at a defined low
     * level, with its pulldown covering the brief routing transition.
     * IDF6.1 gpio_set_direction -> gpio_output_enable also disconnects the
     * old peripheral matrix route (gpio_hal_matrix_out_default).
     * Do not use gpio_config: it would reserve a GPIO owned by this mux. */
    err = gpio_set_level(pins[old], 0);
    if (err == ESP_OK)
      err = gpio_set_direction(pins[old], GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
      faulted = true;
      return err;
    }
  }
  esp_err_t err = rmt_enable(channel);
  if (err != ESP_OK)
    return err;
  rmt_transmit_config_t tx = {.loop_count = 0, .flags.eot_level = 0};
  err = rmt_transmit(channel, encoder, pixels[chain], LED_BYTES, &tx);
  if (err == ESP_OK)
    err = rmt_tx_wait_all_done(channel, 20);
  /* Always stop before reusing pixels or switching pins, also on timeout.
   * A failed stop latches off: an unfinished transaction may own pixels. */
  esp_err_t stop = rmt_disable(channel);
  if (stop != ESP_OK) {
    faulted = true;
    return stop;
  }
  return err;
}

esp_err_t wheel_led_init(void) {
  for (unsigned i = 0; i < 2; ++i) {
    ESP_ERROR_CHECK(gpio_set_level(pins[i], 0));
    ESP_ERROR_CHECK(gpio_set_direction(pins[i], GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_pulldown_en(pins[i]));
  }
  rmt_tx_channel_config_t config = {
      .gpio_num = pins[0], .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10000000, .mem_block_symbols = LED_DMA_SYMBOLS,
      .trans_queue_depth = 1, .flags.with_dma = true};
  esp_err_t err = rmt_new_tx_channel(&config, &channel);
  if (err != ESP_OK)
    return err;
  rmt_simple_encoder_config_t ec = {
      .callback = encode_frame, .min_chunk_size = 24};
  err = rmt_new_simple_encoder(&ec, &encoder);
  if (err != ESP_OK) {
    rmt_del_channel(channel);
    channel = NULL;
    return err;
  }
  active = 0;
  faulted = false;
  err = wheel_led_refresh(0);
  if (err == ESP_OK)
    err = wheel_led_refresh(1);
  return err;
}
