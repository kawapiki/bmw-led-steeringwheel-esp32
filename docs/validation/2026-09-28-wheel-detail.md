# Wheel geometry detail — 2026-09-28

The mesh exporter increases wheel decimation weight from 0.28 to 0.70 and the total triangle ceiling from 2500 to 3100. Each wheel now retains 261–262 triangles instead of 105–106. Total geometry is 3100 triangles / 124,000 bytes (previously 2498 / 99,920). No new runtime buffers, textures or allocations; wheel spin and transforms are unchanged. The original Blender asset is not modified. Shared contracts and OTA recovery remain unchanged.

Native-size actual TGX/LVGL capture visually checked for improved tire/rim outlines. Host render suite passed, including 1459 incremental/full comparisons and all 64 closure masks. Some faceting remains inherent to the source geometry and 320x172 display; no photorealism claim. Detailed-wheels preview is in the design runtime-renders folder.

ESP-IDF 6.1 build and system audit passed; original backup unchanged. Wheel application 1,954,528 bytes, SHA-256 `9380fdacf7e6aa280c97a7ff7948a4fbd87e34a1104e2204bb2a0fc42a54e11c`. Application-only USB flash to ota_1 at 0x620000 verified; gateway software unchanged.

30-second USB bench: boot self-test passed; no panic/assert/watchdog in capture. Render average at 160 frames 67,618 us, maximum 81,374 us (previous build average 60,922 us). About 6.7 ms extra CPU rendering cost, not zero-cost detail. Active UI loop windows approximately 117–121 ms; settled loop 7.85 ms. Internal heap minimum 76,076 bytes, largest internal block 31,744 bytes, UI stack low-water 1,052 bytes. Existing PSRAM buffers remain 220,160 bytes. Not measured physical panel FPS. Exact-build Wi-Fi/OTA requalification and user visual confirmation remain pending.
