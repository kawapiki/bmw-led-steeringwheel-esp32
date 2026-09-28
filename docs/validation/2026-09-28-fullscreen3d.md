# Fullscreen 3D and surface lighting validation

The successful Vehicle, closure and boot scenes now occupy the full 320x172 display with no text, icons or gauges. Service/OTA and unavailable/paused fallback screens remain usable. The boot sequence is black → front angel eyes → body reveal/zoom → yawing lateral drift off-screen. Light pools and dim mirrored emissive faces provide stylized surface reflection without a second full mesh pass.

Wheel image: 1,929,280 bytes, SHA-256 `34c2a1f022d287de5b50b80653c69ade96f21253b7d314271b6125df81002629`. Application-only write to ota_1 at 0x620000 verified by esptool. Gateway image/protocol unchanged, vehicle disconnected. Original backup hash and build partition/security checks passed.

Actual TGX/LVGL host verification: 1,459 incremental/full comparisons pass. Whole-screen pixel equality with the renderer proves no UI overlays on successful boot, Vehicle and all 64 closure scenes. Six lighting changes, unknown-source behavior, throttled updates and maintenance restoration remain covered. Native boot captures at progress 0/.18/.4/.56/.72/.85/.98 were inspected: only rings initially, body emerges, then car exits; final black frame is intentional. Close-up roof cropping during the boot zoom is intentional; ordinary multi-opening views retain the full model.

The two external buffers now use 220,160 bytes. The 90-second physical run passed PSRAM and boot self-tests. Runtime timing is materially slower than the smaller viewport: roughly 56 ms CPU render and 100 ms total boot UI cycle. This is a functional visual increment, not a high-FPS or physical-input-latency qualification. Static scenes stop flushing and normal non-3D menus remain independent. Final visual owner confirmation and exact-build Wi-Fi/OTA regression are pending.

## Private capture measurements

COM6 capture SHA-256: `bdb5c45fed6acf481bb8ba74f26d8cc91bb8528a95010518caaaee1be9c7a0b1`. Fault signatures: 0.

- `render_avg_us` logged range: 55531–56912.
- `render_max_us` logged range: 67744–67744.
- `period_avg_us` logged range: 5134–105499.
- `internal_heap_min` logged range: 76216–76216.
- `internal_largest` logged range: 31744–31744.
- `stack_free` logged range: 1040–1040.

Period averages mix boot, transitions and settled scenes; they are not display FPS.

COM7 capture SHA-256: `d7a951084b4c168d33b38a7ef89865b7f19561e978bed140e3375f262467184d`. Fault signatures: 0.

## Hood/trunk emblem follow-up

Image SHA-256 `5241e588456d2adc8a9b81d29e6abb623f6a94aa72e0e66a04286679531e2892`, unchanged 1,929,280-byte image size and 2,498 triangles. Added explicit blue/white roundels to hood/trunk groups, preserving their opening transforms. Actual LVGL/TGX host suite passes 1,459 comparisons; front, rear and open-trunk captures visually inspected. Build/audit and application-only write verification passed. A 25-second USB run passed the boot self-test with no panic/assert/watchdog signatures.

Wheel private capture SHA-256: `4e05f0bdb21780a97f2fb8849a4c3119c4e97173c57283e932af2003b541c8c8`.
