# Black metallic E90 material validation — 2026-09-28

Contract runtime3D1; application and OTA recovery contracts unchanged. Body faces now carry paint material identity, glass a separate identity; badges, lamps and wheels retain their materials. Triangle count remains 2498 and storage remains 40 bytes per triangle. The source Blender model is unchanged.

Paint has neutral black/charcoal shading with camera-relative additive studio highlights computed from transformed face normals. This is a bounded low-poly metallic appearance, not physically based rendering. No textures, extra buffers or per-frame allocations were added. Boot reveal, unknown-state dimming and on-model lamps retain their existing behavior.

Validation: actual TGX/LVGL host rendering passed, including 1459 incremental/full comparisons, all 64 closure masks, lamp states, unknown states and maintenance interruption. Native-size front and open-door captures are in the design runtime-renders folder. ESP-IDF 6.1 wheel build and system-build audit passed; original firmware backup hash unchanged. Wheel application size 1,929,840 bytes, SHA-256 `58fd2726caf1b38721faa589146f1c31970b02dd8ee2664459437de5f995ff77`.

An intermediate build with redundant specular-table changes ran slowly and one boot self-test failed. The final renderer disables unused specular tables; no boot-health threshold was relaxed. USB application-only installation targets wheel ota_1 at 0x620000; gateway software and NVS are unchanged.

Physical appearance still needs user confirmation. Wi-Fi/OTA under this exact graphics build has not been newly qualified. Host screenshots and timing are not physical-display FPS measurements.

Final 30-second USB bench run: write hash verified; boot self-test passed on ota_1. No panic/assert/watchdog was found in the capture. Render average at 160 frames was 60,558 us, maximum 73,400 us. Active-scene UI loop windows were approximately 110–115 ms; settled scene approximately 7.9 ms. Internal heap minimum 76,100 bytes, largest internal block 31,744 bytes, UI stack low-water 1,052 bytes. PSRAM render buffers remain 220,160 bytes. This is CPU/task timing, not measured panel FPS; high-refresh animation is not claimed.
