# Fender repeaters and open-door tint — 2026-09-28

Runtime3D1 and both communication contracts remain unchanged. Two body-mounted side repeater quads on the front fenders use the existing valid left/right indicator bits and incoming blink phase. They stay attached to the body while doors open, are depth-tested, remain dark during boot/unknown lamp states, and use the existing dim mirrored-light treatment. Position and size are illustrative, slightly enlarged for the 320x172 display. Four extra triangles normally, up to four more for lit reflections; original mesh unchanged.

Only paint on a known-open front/rear door receives a subtle red tint. Glass and trim remain unchanged. Closed, unknown and decorative boot door states are not highlighted. Existing metallic shading remains visible beneath the tint.

Actual TGX/LVGL host render suite passed (1459 incremental/full comparisons, 64 closure masks, lamp and unknown states, maintenance interruption). Native-size captures visually checked: side repeater lies behind the front wheel and before the front door; open front-left door has a muted red paint tint.

ESP-IDF 6.1 build and system-build audit passed; immutable original backup hash unchanged. Application 1,930,448 bytes, SHA-256 `3b760798828e3d1d005f66039152a6b6ccc5b47295b595224337034cf503b07c`. Application-only USB flash at wheel ota_1 0x620000 verified its hash. Gateway firmware unchanged.

30-second USB bench run: boot self-test passed, no panic/assert/watchdog found. At 160 frames render average 60,922 us, max 72,929 us. Internal heap minimum 76,100 bytes, largest block 31,744 bytes, UI stack low-water 1,052 bytes. PSRAM buffers unchanged at 220,160 bytes. These are CPU measurements, not panel FPS. On-device visual confirmation and Wi-Fi/OTA requalification of this exact build remain pending.
