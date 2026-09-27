#include "motion.h"
#include "demo.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/task.h"
#include "wheel_board.h"
#include "wheel_core.h"
static i2c_master_dev_handle_t dev;
static uint8_t bytes[32];
static bool valid, configured;
static uint8_t mode, units, system_status;
static void publish(demo_state_t *s, void *a) {
  (void)a;
  s->sensor_ok = valid;
  s->orientation_valid = valid && system_status == 5 && (bytes[31] >> 6) >= 2;
  if (valid) {
    for (int i = 0; i < 3; i++) {
      s->accel[i] = (int16_t)(bytes[i * 2] | bytes[i * 2 + 1] << 8);
      s->mag[i] = (int16_t)(bytes[6 + i * 2] | bytes[7 + i * 2] << 8);
      s->gyro[i] = (int16_t)(bytes[12 + i * 2] | bytes[13 + i * 2] << 8);
      s->euler[i] = (int16_t)(bytes[18 + i * 2] | bytes[19 + i * 2] << 8);
    }
    s->calibration = bytes[31];
  }
}
static esp_err_t readreg(uint8_t r, void *b, size_t n) {
  return i2c_master_transmit_receive(dev, &r, 1, b, n, 20);
}

static int sensor_read(void *ctx, uint8_t reg, uint8_t *value) {
  (void)ctx;
  return readreg(reg, value, 1) == ESP_OK ? 0 : -1;
}
static int sensor_write(void *ctx, uint8_t reg, uint8_t value) {
  (void)ctx;
  uint8_t data[] = {reg, value};
  return i2c_master_transmit(dev, data, sizeof(data), 20) == ESP_OK ? 0 : -1;
}
static void sensor_delay(void *ctx, unsigned ms) {
  (void)ctx;
  vTaskDelay(pdMS_TO_TICKS(ms));
}
static void run(void *a) {
  (void)a;
  vTaskDelay(pdMS_TO_TICKS(800));
  configured = sensor_configure(NULL, sensor_write, sensor_read, sensor_delay);
  if (!configured) {
    valid = false;
    demo_edit(publish, NULL);
    vTaskDelete(NULL);
    return;
  }
  for (;;) {
    valid = configured && readreg(0x3d, &mode, 1) == ESP_OK && mode == 0x0c &&
            readreg(0x3b, &units, 1) == ESP_OK && units == 0 &&
            readreg(0x39, &system_status, 1) == ESP_OK && system_status == 5 &&
            readreg(8, bytes, 24) == ESP_OK &&
            readreg(0x35, &bytes[31], 1) == ESP_OK;
    demo_edit(publish, NULL);
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
void motion_start(void) {
  gpio_config_t en = {.pin_bit_mask = 1ULL << WHEEL_SENSOR_ENABLE,
                      .mode = GPIO_MODE_OUTPUT};
  ESP_ERROR_CHECK(gpio_config(&en));
  gpio_set_level(WHEEL_SENSOR_ENABLE, 1);
  i2c_master_bus_config_t bus = {.i2c_port = 0,
                                 .sda_io_num = WHEEL_SENSOR_SDA,
                                 .scl_io_num = WHEEL_SENSOR_SCL,
                                 .clk_source = I2C_CLK_SRC_DEFAULT,
                                 .glitch_ignore_cnt = 7,
                                 .flags.enable_internal_pullup = true};
  i2c_master_bus_handle_t b;
  if (i2c_new_master_bus(&bus, &b) != ESP_OK)
    return;
  i2c_device_config_t cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                             .device_address = 0x28,
                             .scl_speed_hz = 400000};
  if (i2c_master_bus_add_device(b, &cfg, &dev) != ESP_OK)
    return;
  configASSERT(xTaskCreatePinnedToCore(run, "motion", 3072, NULL, 2, NULL, 0) ==
               pdPASS);
}
