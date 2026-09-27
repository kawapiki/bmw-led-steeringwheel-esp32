# E90 LED/TFT steering wheel

Hardware reference and original firmware backup for a Chinese aftermarket BMW E90 steering-wheel display board. Custom firmware development has not started.

## Hardware

| Item | Specification |
|---|---|
| PCB marking | CVS8161-332-V01-T16 |
| Additional markings | MARTIN · 2026.4.20 |
| MCU | ESP32-S3, QFN56, silicon revision v0.2 |
| PSRAM | 8 MB embedded |
| Flash | Winbond, JEDEC EF4019, 32 MiB, 3.3 V |
| Crystal | 40 MHz |
| Display | ST7789/ST7789VW driver, 320 × 172 |
| LEDs | Two WS2812 chains, 24 addressed LEDs per chain |
| Motion-sensor interface | BNO055, I²C address 0x28, 400 kHz |
| USB | Native USB Serial/JTAG |
| Secure Boot / flash encryption | Both disabled at backup time |

## Firmware GPIO map

Assignments below are confirmed in the original firmware. PCB continuity and connector contact order have not been measured. QFN pin numbers refer to the ESP32-S3 chip, not the display flex or white connectors.

| Component | Signal | GPIO | QFN56 pin | Behavior |
|---|---|---:|---:|---|
| TFT | MOSI / data | 15 | 21 | SPI output |
| TFT | SCLK | 16 | 22 | SPI clock |
| TFT | CS | 17 | 23 | Active low |
| TFT | D/C | 18 | 24 | Low = command; high = data |
| TFT | RESET | 13 | 18 | Active low |
| TFT | Backlight control | 38 | 43 | High = on |
| WS2812 chain 0 | DIN | 3 | 8 | 24 addressed LEDs |
| WS2812 chain 1 | DIN | 4 | 9 | 24 addressed LEDs |
| Button K1 | Input | 12 | 17 | Active low, internal pull-up |
| Button K2 | Input | 11 | 16 | Active low, internal pull-up |
| Vibration driver | Control | 21 | 27 | High = on |
| BNO055 interface | SDA | 39 | 44 | I²C data |
| BNO055 interface | SCL | 40 | 45 | I²C clock |
| BNO055 power control | Enable/control | 10 | 15 | High = enabled |

Display configuration: SPI2, mode 0, no MISO, 10 MHz initialization followed by 40 MHz operation. LED wire order: GRB. GPIO10 and GPIO21 are logic-control signals, not identified power terminals.

Standard ESP32-S3 native USB assignments are GPIO19 / QFN25 for D− and GPIO20 / QFN26 for D+. Physical USB routing has not been traced. Physical chip-pin numbering follows the [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf).

## Original firmware backup

**File:** [steering-wheel-original.bin](back/steering-wheel-original.bin)

| Property | Value |
|---|---|
| Backup date | 2026-09-27 |
| Image type | Complete raw flash image |
| Size | 33,554,432 bytes (32 MiB) |
| Flash range | 0x00000000–0x01FFFFFF |
| Integrity | Verified against the device's complete flash digest |
| Restore testing | Not performed |

SHA-256:

```text
c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45
```

The image contains the bootloader, partition table, both application slots, assets, and flash-stored settings. It is a device-specific snapshot and may contain pairing or network configuration. It does not contain the chip's eFuses. Keep this file unchanged.

### Stored applications

| Slot | Flash offset | Project | Version | Build time | ESP-IDF |
|---|---|---|---|---|---|
| ota_0 | 0x20000 | fangxp | d70139b-dirty | 2026-07-10 16:27:35 | v5.4.3 |
| ota_1 | 0x720000 | fangxp | 7af8fdb-dirty | 2026-06-30 10:32:41 | v5.4.3 |

Saved OTA metadata selects ota_0 under standard ESP-IDF selection rules. The `dirty` suffix is part of the original build label.

### Partition layout

| Partition | Offset | Size |
|---|---|---|
| nvs | 0x9000 | 0x6000 |
| otadata | 0xF000 | 0x2000 |
| phy_init | 0x11000 | 0x1000 |
| ota_0 | 0x20000 | 0x700000 |
| ota_1 | 0x720000 | 0x700000 |
| nvs_key | 0xE20000 | 0x1000 |
| storage | 0xE21000 | 0x80000 |
| spiffs | 0xEA1000 | 0x600000 |

## Software and compatibility notes

The original application includes LVGL, NimBLE, and ELM327 support. BLE references include the name `OBDBLE` and advertising UUIDs `0x4353` and `0xFFF0`. Separate GATT-server references mention service `ABCD`, TX/notify `AB02`, and RX `AB01`. These are leads, not a verified replacement-dongle protocol specification.

## Unresolved hardware details

- White connector and TFT-flex contact order, supply connections, and voltages.
- Physical left/right assignment of the LED chains and K1/K2 buttons.
- Regulator, protection, motor-driver, and sensor-power circuit wiring.
- BNO055 reset/interrupt/strap connections and flash-pad routing overrides.

This is a firmware-derived peripheral map, not a complete PCB schematic. Unlisted GPIOs are not proven available. The backup supports software recovery on compatible, undamaged hardware; it cannot reverse eFuse changes or electrical damage.
