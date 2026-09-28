# Operational wheel and gateway lighting â€” USB bench

The wheel consumes normalized telemetry and runs the normal UI for simulated and vehicle-originated samples. Only the gateway generates synthetic values. Normal instruments have no demo badge; provenance remains diagnostic. Lighting-v1 is additive and the OTA recovery protocol is unchanged. CAN remains disabled.

## Firmware and checks

ESP-IDF 6.1. Wheel ESP32-S3 COM6, active ota_1 at0x620000; LILYGO ESP32 COM7, active ota_0 at0x20000. Both disconnected from the vehicle, powered over USB. Application-only writes completed with esptool hash verification; pairing/NVS/partition table were not rewritten.

| Image | Bytes | SHA-256 |
|---|---:|---|
| Wheel | 5470544 | 9eb2d707bb29b1f352698b25bd99ae6275cf95d81dbb790f057aee4d42cd8d43 |
| Gateway | 1267808 | 3a51f84a03293e4ba215fcac694996f01a7d8c1f02e86e395f94addb203aa08b |

Both builds and tools/check-system-build.py passed: image/slot/chip/security checks and immutable original backup SHA-256. Wheel has820912bytes remaining in its existing6MiB application slot.

Host verification passed: legacy telemetry, cockpit, lighting codec and BLE harnesses; UI model/navigation and intro lifecycle. UI worker ran1481 incremental/full redraw comparisons plus92 asset pixel checks. Source1/source2 normal rendering equality and acknowledged-open visibility covered. Independent review found and confirmed fixes for cockpit encoder source preservation and persistent acknowledged-door markers.

## Runtime evidence

Both boot self-tests passed after reset. Wheel received each individual closure bit, a multi-opening mask0x19 and closed0x00. Independently received valid lighting masks0x01,02,04,08,10,20 and known-off0x00, including gateway-owned indicator phases. Initial unknown state remained invalid. Gateway log confirms CAN disabled and authenticated recovery available.

The E90 scene is currently pre-rendered RGB565, not a real-time3D engine. Boot lamp flashes are decorative; received lamp states are displayed as six header icons, not yet applied to the car mesh. Runtime3D remains a separate pending increment.

The owner confirmed the intro, automatic E90 closure graphic and changing lamp icons. Saved-Wi-Fi release discovery on this exact image remains pending. Full OTA of the enlarged image has not been tested. Previous smaller-image OTA evidence does not establish its download reliability. This USB run does not validate a CAN decoder, electrical compatibility, physical frame rate or input latency.

## Completed90-second capture

COM6 private capture SHA-256: `115317200b5a91fb1a1f87c315acdc3c0c8e51628ce46cb9e4835efa7bbb60c4`. Panic/assert/watchdog signatures: 0.

- `period_avg_us` across logged windows: 5224–7533.
- `handler_avg_us` across logged windows: 78–2281.
- `handler_max_us` across logged windows: 20812–134216.
- `heap_min` across logged windows: 80100–80100.

Task-loop and handler timings are not measured display FPS or physical input latency. Maximum handler time includes scene transitions; smoothness is not inferred from average loop time.

COM7 private capture SHA-256: `b0acebc5d1f1a1e535e7039b661872b848c9168c1380fc5855a8ce31a394294a`. Panic/assert/watchdog signatures: 0.
