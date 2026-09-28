# Ribbon UI and gateway telemetry bench — 2026-09-28

## Scope and result

Implemented two selectable instruments (Engine and Shift lights), Gateway status, and a nested nine-tool Service menu. The owner confirmed that both instruments show changing RPM and that navigation works on the physical wheel. The displayed RPM now originates on the LILYGO and crosses the authenticated BLE link; there is no wheel-local simulation fallback. Source is explicitly Demo. No vehicle decoder or CAN command is enabled.

Four agent roles contributed: automotive design, gateway/BLE development, LVGL implementation and independent review. The integrator owns shared state, app composition, build and device access. Contract: [gateway/UI revision 1](../architecture/gateway-ui-v1.md); the existing 16-byte application format and permanent OTA recovery v1 wire format are unchanged.

## Build and desktop verification

ESP-IDF6.1 / LVGL9.4.0. Both targets built successfully. Wheel image 1,719,424 bytes, gateway 1,265,168 bytes. Wheel gained 77,232 bytes versus r4, including the built-in Montserrat40 font and new view/model; gateway gained 1,600 bytes. No new framebuffer or PSRAM use.

- 27 shared native core tests pass.
- 7 packaging/authenticity tests against both new images pass.
- New production telemetry codec and fake-ATT callback/gate harnesses pass: malformed/range/duplicate/regression/wrap/session/latency cases, one in-flight request, maintenance suppression and recovery handoff.
- Pure production UI model tests pass: four-screen cycle, nested navigation, offline/stale/invalid/current-zero, and confirmation binding to the actually displayed release/generation/digest/mode.
- Existing LED transport, HTTP redirect, release response and OTA read-retry harnesses pass.
- Both-target partition/chip/security audit passes. Original backup hash remains c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45.
- Actual LVGL production view rendered at320x172 on desktop, inspected including Shift/stale/QR/update offer. QR decoded successfully with fictional credentials. Host allocator peak19,376 bytes is a64-bit host result, not ESP32 RAM measurement.

Review corrected two findings before the final build: maintenance validity suppression and a confirmation race which could otherwise package a newer candidate than the displayed offer. No remaining reviewer blocker.

## Authorized physical deployment

Both boards remained on the previously authorized USB-only, vehicle-disconnected bench. Before flashing, reset logs identified wheel ota_1 at0x620000 and gateway ota_0 at0x20000. Only those application regions were written, and esptool verified their hashes. Bootloader, partition table, otadata, NVS, eFuses and immutable original backup were not written. Both new applications passed their boot self-tests in the same slots.

These are USB-loaded development builds, not a new signed GitHub release or a repeated end-to-end OTA installation. The preserved OTA journal/release floor still reports release4. The exact tested application hashes are:

- Wheel: 5e95889858fde0fa914af936dddf4be8136b781a9a0aba5f2275afd884e39b2a
- Gateway: 88383a517be458539043e378c29cf330d7138e6d7f198f45295ea90f98958b76

Known r4 restoration assets were verified before writing: wheel164cab08eb9db3ae543a9a04730ff0e241160a3f6a1cf951834579750ece035f; gateway38c76e7b80a8309b3a2fdfa51ff25d70a13ae775f17c7d4a01dbc61201299126. Private captures/builds are kept locally, not committed.

## Runtime evidence

In the initial45-second capture, authenticated sequences advanced41 ->91 ->141 ->192 over successive roughly5-second samples, consistent with the10Hz gateway producer target. Observed receiver age at those samples was87–193ms. This is sampled receiver age, not end-to-end sensor latency or proof every packet was rendered.

Normal Engine/Shift capture: minimum free heap80,372 bytes; five-second mean UI loop periods approximately5.8–8.4ms. Largest handler time58,631us occurred in the initial startup window; subsequent captured windows reached56,164us during Shift/navigation. These loop/handler values are not FPS or input-to-photon measurements. The preceding r4 capture had minimum80,500 bytes, but workload/runtime history differed, so the128-byte difference is not an isolated allocator delta.

A controlled gateway ROM-loader interval lasted approximately15seconds, without flash writes. Wheel records changed to secure=0/source=NONE and stopped redrawing unchanged missing data. Restarting the gateway automatically restored the authenticated link and accepted its new sequence20,70,120,170. No wheel restart was needed. The owner independently confirmed both instruments and page switching.

Startup PHY pll_cal timing messages were present in both the pre-change r4 and new captures. No panic, assertion or LED transport failure was observed in these captures.

## Remaining limits

Actual BMW signal decoding, electrical bus qualification, calibrated shift thresholds, vehicle control, long-duration endurance and fault-injected rollback remain out of scope/unverified. The6500RPM visual cue is a demo threshold. A new complete paired OTA cycle has not been repeated for this UI build; the successful r4 paired OTA evidence remains separate. Network-service coexistence results are appended below when observed.

## Wi-Fi service coexistence

The owner ran the Release check through the new nested menu using saved Wi-Fi credentials. Private logs confirm `Checking signed GitHub releases` at150849ms and `No newer authenticated release` at173725ms (about22.9seconds; four nonempty release pages plus end-of-list). Minimum free heap reached9,220bytes during this service session. No allocation failure, panic or reset was observed. Application telemetry intentionally paused while maintenance remained active; the independent authenticated BLE connection remained present. This validates discovery with the new UI, not a new firmware download/install or rollback cycle.
